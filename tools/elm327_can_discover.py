#!/usr/bin/env python3
"""Generic, receive-only BLE ELM327 CAN capture and offline discovery.

See docs/CAN_DISCOVERY.md. Capture requires bleak; offline commands use only
Python's standard library. No vehicle-specific signal definitions are included.
"""

import argparse
import asyncio
import contextlib
import csv
import json
import math
import re
import signal
import sys
import time
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

from can_capture_worker import CaptureWorker


PROTOCOLS = {
    "6": (11, 500000), "7": (29, 500000),
    "8": (11, 250000), "9": (29, 250000),
}
SCHEMA = "elm327_can_discover"
VERSION = 1


def positive_float(value):
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise argparse.ArgumentTypeError("must be a finite positive number")
    return number


def nonnegative_float(value):
    number = float(value)
    if not math.isfinite(number) or number < 0:
        raise argparse.ArgumentTypeError("must be a finite nonnegative number")
    return number


def positive_int(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be a positive integer")
    return number


def buffer_megabytes(value):
    number = positive_int(value)
    if number > 1024:
        raise argparse.ArgumentTypeError("buffer must be between 1 and 1024 MiB")
    return number


def label_text(value):
    value = value.strip()
    if not value or len(value) > 128 or not value.isprintable():
        raise argparse.ArgumentTypeError("label must contain 1–128 printable characters")
    return value


def note_text(value):
    if len(value) > 4096 or (value and not value.isprintable()):
        raise argparse.ArgumentTypeError("note must contain at most 4096 printable characters")
    return value


def hex_id(value):
    value = value.removeprefix("0x").removeprefix("0X")
    if not re.fullmatch(r"[0-9a-fA-F]{1,8}", value):
        raise argparse.ArgumentTypeError("expected a hexadecimal CAN ID or mask")
    return int(value, 16)


def filter_commands(protocol, can_id=None, can_filter=None, mask=None):
    bits, _ = PROTOCOLS[protocol]
    width = 3 if bits == 11 else 8
    limit = (1 << bits) - 1
    for value in (can_id, can_filter, mask):
        if value is not None and not 0 <= value <= limit:
            raise ValueError(f"ID/filter/mask exceeds {bits}-bit range")
    if can_id is not None and (can_filter is not None or mask is not None):
        raise ValueError("--can-id and --filter/--mask are mutually exclusive")
    if (can_filter is None) != (mask is None):
        raise ValueError("--filter and --mask must be specified together")
    if can_id is not None:
        return [f"ATCRA{can_id:0{width}X}"]
    if can_filter is not None:
        return [f"ATCF{can_filter:0{width}X}", f"ATCM{mask:0{width}X}"]
    return ["ATCRA", f"ATCF{0:0{width}X}", f"ATCM{0:0{width}X}"]


def parse_can_line(line, protocol):
    """Parse classic CAN with headers and DLC display disabled (ATD0)."""
    text = "".join(line.split()).upper()
    bits, _ = PROTOCOLS[protocol]
    width = 3 if bits == 11 else 8
    if not re.fullmatch(r"[0-9A-F]+", text) or len(text) < width:
        raise ValueError("not a header-preserving CAN frame")
    header, payload = text[:width], text[width:]
    if len(payload) % 2 or len(payload) > 16:
        raise ValueError("expected 0–8 bytes with DLC display disabled")
    identifier = int(header, 16)
    if identifier >= 1 << bits:
        raise ValueError(f"CAN ID exceeds {bits}-bit range")
    return identifier, bytes.fromhex(payload)


class LineFramer:
    def __init__(self, limit=512):
        self.pending = bytearray()
        self.limit = limit

    def feed(self, chunk):
        for byte in chunk:
            if byte in (10, 13, 62):
                if self.pending:
                    line = bytes(self.pending).decode("ascii", "replace").strip()
                    self.pending.clear()
                    if line:
                        yield "line", line
                if byte == 62:
                    yield "prompt", ">"
            else:
                self.pending.append(byte)
                if len(self.pending) > self.limit:
                    self.pending.clear()
                    raise ValueError("unterminated adapter line exceeds size limit")


class Recorder:
    def __init__(self, path):
        self.file = Path(path).expanduser().open("x", encoding="utf-8", buffering=65536)
        self.started = time.monotonic()
        self.last_flush = self.started

    def now(self):
        return time.monotonic() - self.started

    def record(self, kind, t=None, **fields):
        row = {"type": kind, "t": self.now() if t is None else t, **fields}
        self.file.write(json.dumps(row, ensure_ascii=False, allow_nan=False) + "\n")
        if time.monotonic() - self.last_flush >= 1:
            self.flush()

    def flush(self):
        self.file.flush()
        self.last_flush = time.monotonic()

    def close(self):
        self.file.close()


class MonitorSession:
    def __init__(self, client, write_char, recorder, protocol="6", timeout=3.0,
                 queue_size=None, *, buffer_bytes=64 * 1024 * 1024, drain_timeout=30.0):
        self.client = client
        self.write_char = write_char
        self.recorder = recorder
        self.protocol = protocol
        self.timeout = timeout
        self.drain_timeout = drain_timeout
        self.loop = asyncio.get_running_loop()
        self.worker = CaptureWorker(recorder, LineFramer(), parse_can_line, protocol,
                                    self.worker_fault, buffer_bytes, queue_size)
        self.queue = self.worker
        self.stop_event = asyncio.Event()
        self.pending = None
        self.command_lines = []
        self.control_framer = LineFramer()
        self.state = "initializing"
        self.monitor_active = False
        self.monitor_attempted = False
        self.stop_confirmed = False
        self.incomplete = False
        self.connected = True
        self.closing = False
        self.consumer = None
        self.events = Counter()
        self.stop_reason = "duration"
        self.label = None
        self.expected_filter = None
        self._overflow_reported = False
        self._last_stats = (recorder.now(), 0, 0)
        self._finished = False
        self.monitor_end_t = None
        self._interrupt_sent = False

    @property
    def frames(self):
        return self.worker.snapshot()["frames"]

    @property
    def queue_drops(self):
        return self.worker.snapshot()["queue_drops"]

    @property
    def ids(self):
        with self.worker.condition:
            return dict(self.worker.ids)

    def record(self, kind, **fields):
        try:
            self.worker.record(kind, **fields)
        except (OSError, BufferError, RuntimeError):
            self.incomplete = True
            self.stop_reason = "writer_failure"
            self.stop_event.set()
            raise

    def on_notify(self, _characteristic, data):
        if self.closing:
            return
        timestamp = self.recorder.now()
        chunk = bytes(data)
        mode = self.state
        accepted = self.worker.receive(timestamp, chunk, mode, self.expected_filter)
        if not accepted and not self._overflow_reported:
            self._overflow_reported = True
            self.fault("host_queue_overflow", "receive byte budget exhausted; capture has gaps")
        # Only command responses use loop-side framing. Monitoring parsing and all
        # filesystem work belong to the worker, including when it falls behind.
        if mode in ("monitoring", "stopping"):
            if b">" in chunk:
                self.monitor_active = False
                self.stop_confirmed = True
                if mode == "monitoring" or not self._interrupt_sent:
                    self.fault("unexpected_prompt", "adapter ended monitoring before requested stop")
                if mode == "stopping" and self.pending is not None and not self.pending.done():
                    self.pending.set_result([])
            return
        try:
            for kind, text in self.control_framer.feed(chunk):
                if self.pending is None or self.pending.done():
                    continue
                if kind == "prompt":
                    self.pending.set_result(list(self.command_lines))
                elif len(self.command_lines) >= 128:
                    self.pending.set_exception(ValueError("adapter command response too long"))
                else:
                    self.command_lines.append(text)
        except ValueError as exc:
            if self.pending is not None and not self.pending.done():
                self.pending.set_exception(exc)
            self.fault("framing_error", str(exc))

    def on_disconnect(self, _client):
        self.connected = False
        if not self.closing:
            self.fault("disconnected", "BLE connection lost")
            if self.pending is not None and not self.pending.done():
                self.pending.set_exception(ConnectionError("BLE connection lost"))

    def worker_fault(self, code, message):
        self.incomplete = True
        if not self.events[code]:
            self.events[code] += 1
        self.stop_reason = code
        self.stop_event.set()

    def fault(self, code, message):
        self.incomplete = True
        self.events[code] += 1
        self.stop_reason = code
        self.stop_event.set()
        try:
            self.worker.record("event", code=code, message=message, incomplete=True)
        except (OSError, BufferError, RuntimeError):
            pass

    def request_stop(self, reason="user"):
        self.stop_reason = reason
        self.stop_event.set()

    async def consume(self):
        self.worker.start()
        try:
            await asyncio.shield(self.worker.done)
        finally:
            self.worker.request_close()
            await self.worker.wait_closed()

    async def write(self, payload, monitor_interrupt=False):
        async def perform():
            if monitor_interrupt:
                await asyncio.sleep(0)
                if not self.monitor_active:
                    return
                self._interrupt_sent = True
            await self.client.write_gatt_char(
                self.write_char, payload,
                response="write" in self.write_char.properties)

        await asyncio.wait_for(perform(), self.timeout)

    async def command(self, command, reset=False):
        if self.monitor_active or self.pending is not None:
            raise RuntimeError("adapter operations must be serialized")
        self.pending = self.loop.create_future()
        self.command_lines = []
        self.record("command", command=command)
        try:
            await self.write((command + "\r").encode("ascii"))
            lines = await asyncio.wait_for(self.pending, max(self.timeout, 5) if reset else self.timeout)
        finally:
            if self.pending is not None and not self.pending.done():
                self.pending.cancel()
            self.pending = None
        self.record("command_response", command=command, lines=lines)
        response = [line.upper() for line in lines if line.upper() != command]
        if reset:
            valid = any(any(name in line for name in ("ELM", "STN", "OBDLINK")) for line in response)
        else:
            valid = "OK" in response
        if not valid or any("?" in line or "ERROR" in line for line in response):
            raise ValueError(f"required adapter command {command} failed: {' | '.join(lines) or 'empty response'}")
        return lines

    async def initialize(self, filters, reset_delay=1.0):
        await self.command("ATZ", reset=True)
        await asyncio.sleep(reset_delay)
        for command in ("ATE0", "ATL0", "ATS0", "ATH1", "ATCAF0", "ATD0",
                        "ATCFC0", "ATCSM1", f"ATSP{self.protocol}", *filters):
            await self.command(command)
        self.state = "ready"

    def mark(self, label):
        label = label_text(label)
        if label != self.label:
            self.label = label
            self.record("marker", label=label)

    async def start_monitor(self, label):
        await self.worker.join()
        self.record("monitor_start")
        self.mark(label)
        self.state = "monitoring"
        self.monitor_active = True
        self.monitor_attempted = True
        self.record("command", command="ATMA")
        await self.write(b"ATMA\r")

    def telemetry(self):
        stats = self.worker.snapshot()
        now = self.recorder.now()
        previous, rx, processed = self._last_stats
        elapsed = max(now - previous, 1e-9)
        stats["rx_bytes_per_second"] = (stats["rx_bytes"] - rx) / elapsed
        stats["processed_bytes_per_second"] = (stats["processed_bytes"] - processed) / elapsed
        self._last_stats = (now, stats["rx_bytes"], stats["processed_bytes"])
        self.record("event", code="telemetry", message="host receive/writer statistics",
                    incomplete=False, **stats)
        return stats

    async def stop_monitor(self):
        if not self.monitor_attempted:
            return
        # Interrupt before draining: a slow disk must not keep the adapter sending.
        # The callback latches spontaneous prompts even while the writer is blocked.
        if self.monitor_active and self.connected:
            self.state = "stopping"
            self.pending = self.loop.create_future()
            with contextlib.suppress(OSError, BufferError, RuntimeError):
                self.record("command", command="monitor_interrupt")
            try:
                await self.write(b"\r", monitor_interrupt=True)
                await asyncio.wait_for(self.pending, self.timeout)
            except Exception as exc:
                self.fault("stop_unconfirmed", str(exc) or "no prompt after monitor interrupt")
            finally:
                if self.pending is not None and not self.pending.done():
                    self.pending.cancel()
                self.pending = None
        elif self.monitor_active:
            self.fault("stop_unconfirmed", "disconnected before monitor interrupt")

    async def finish(self):
        if self._finished:
            raise RuntimeError("capture already finalized")
        self._finished = True
        if self.monitor_end_t is None:
            self.monitor_end_t = self.recorder.now()
        try:
            await asyncio.wait_for(self.worker.join(), self.drain_timeout)
        except asyncio.TimeoutError:
            self.fault("drain_timeout", "receive backlog did not drain within --drain-timeout")
            self.worker.request_close(discard=True)
        except OSError as exc:
            self.worker_fault("writer_failure", str(exc))
        if not self.worker.closing and self.worker.framer.pending:
            self.fault("partial_line", "unterminated adapter output at capture end")
        if self.monitor_attempted and not self.frames:
            self.fault("no_frames", "no valid CAN frames received; verify bus, protocol and filter")
        self.worker.request_close()
        await self.worker.wait_closed()
        # The worker has exited; only now may the loop own the file again.
        if self.monitor_attempted:
            self.recorder.record("monitor_end", t=self.monitor_end_t)
        complete = (self.monitor_attempted and self.stop_confirmed and
                    not self.incomplete and not self.queue_drops and self.frames > 0)
        metrics = self.worker.snapshot()
        for code, count in self.worker.events.items():
            self.events[code] = max(self.events[code], count)
        if self.worker.events or self.worker.error:
            complete = False
        self.recorder.record("summary", complete=complete, events=dict(self.events),
                             stop_confirmed=self.stop_confirmed, reason=self.stop_reason, **metrics)
        self.recorder.flush()
        return complete


async def select_device(args, transport):
    if args.address:
        return args.address, args.address
    rows = await transport.scan_rows(args.scan_timeout)
    if args.name:
        matches = [row for row in rows if args.name.lower() in row["name"].lower()]
    else:
        matches = [row for row in rows if row["known"]]
    if len(matches) != 1:
        transport.print_rows(rows)
        raise ValueError("expected exactly one matching adapter; select --address from scan results")
    return matches[0]["address"], matches[0]["name"]


def attach_labels(session, disabled):
    loop = asyncio.get_running_loop()
    if disabled or not sys.stdin.isatty():
        return lambda: None
    try:
        fd = sys.stdin.fileno()

        def read_label():
            line = sys.stdin.readline(1024)
            if not line:
                loop.remove_reader(fd)
            elif line.strip() == "/quit":
                session.request_stop()
            elif line.strip():
                try:
                    session.mark(line.strip())
                    print(f"Label: {session.label}", flush=True)
                except argparse.ArgumentTypeError as exc:
                    print(f"Invalid label: {exc}", file=sys.stderr)

        loop.add_reader(fd, read_label)
        print("Type a label + Enter to mark a state; /quit or Ctrl-C stops. Labels stay local.")
        return lambda: loop.remove_reader(fd)
    except (AttributeError, OSError, NotImplementedError):
        print("Interactive labels unavailable; using initial label only.", file=sys.stderr)
        return lambda: None


async def capture(args):
    import elm327_ble_dump as transport

    filters = filter_commands(args.protocol, args.can_id, args.can_filter, args.mask)
    path = Path(args.out).expanduser()
    if path.exists():
        raise FileExistsError(f"refusing to overwrite {path}")
    if not path.parent.is_dir():
        raise ValueError(f"output directory does not exist: {path.parent}")
    transport._load_bleak()
    address, name = await select_device(args, transport)
    recorder = Recorder(path)
    bits, bitrate = PROTOCOLS[args.protocol]
    recorder.record("session", t=0, schema=SCHEMA, version=VERSION,
                    utc=datetime.now(timezone.utc).isoformat(), adapter=name,
                    address=address, protocol=args.protocol, id_bits=bits,
                    bitrate=bitrate, filters=filters, note=args.note,
                    buffer_limit_bytes=args.buffer_mb * 1024 * 1024,
                    drain_timeout=args.drain_timeout, compact_output=True,
                    clock="host_ble_receive_monotonic_seconds")
    session = None
    complete = False
    try:
        def disconnected(client):
            if session is not None:
                session.on_disconnect(client)

        async with transport.BleakClient(address, timeout=10,
                                         disconnected_callback=disconnected) as client:
            service, write_char, notify_char = transport.find_chars(client)
            if service in (None, "GENERIC"):
                raise ValueError("no recognized BLE UART service; generic characteristic pairing is not trusted")
            if not ({"write", "write-without-response"} & set(write_char.properties)):
                raise ValueError("adapter write characteristic is not writable")
            if not ({"notify", "indicate"} & set(notify_char.properties)):
                raise ValueError("adapter receive characteristic cannot notify/indicate")
            recorder.record("transport", service=service, write=write_char.uuid, notify=notify_char.uuid)
            session = MonitorSession(client, write_char, recorder, args.protocol, args.timeout,
                                     buffer_bytes=args.buffer_mb * 1024 * 1024,
                                     drain_timeout=args.drain_timeout)
            if args.can_id is not None:
                session.expected_filter = (args.can_id, (1 << bits) - 1)
            elif args.can_filter is not None:
                session.expected_filter = (args.can_filter, args.mask)
            session.consumer = asyncio.create_task(session.consume())
            session.consumer.add_done_callback(lambda _task: session.stop_event.set())
            subscribed = False
            remove_labels = lambda: None
            signal_installed = False
            previous_sigint = signal.getsignal(signal.SIGINT)
            loop = asyncio.get_running_loop()
            try:
                await client.start_notify(notify_char, session.on_notify)
                subscribed = True
                await session.initialize(filters)
                print(f"Connected {name}; protocol {args.protocol} ({bits}-bit/{bitrate}).")
                print("Silent mode acknowledged; clone firmware may not implement electrical listen-only.")
                await session.start_monitor(args.label)
                remove_labels = attach_labels(session, args.no_input)
                try:
                    loop.add_signal_handler(signal.SIGINT, session.request_stop)
                    signal_installed = True
                except (NotImplementedError, RuntimeError):
                    pass
                end = loop.time() + args.duration
                while not session.stop_event.is_set():
                    remaining = end - loop.time()
                    if remaining <= 0:
                        break
                    try:
                        await asyncio.wait_for(session.stop_event.wait(), min(2, remaining))
                    except asyncio.TimeoutError:
                        stats = session.telemetry()
                        print(f"RX {stats['rx_bytes_per_second'] / 1024:.1f} KiB/s | "
                              f"writer {stats['processed_bytes_per_second'] / 1024:.1f} KiB/s | "
                              f"buffer {stats['buffer_bytes'] / 1048576:.2f}/"
                              f"{stats['buffer_limit_bytes'] / 1048576:.0f} MiB "
                              f"(peak {stats['peak_buffer_bytes'] / 1048576:.2f}) | "
                              f"lag {stats['writer_lag_seconds']:.2f}s | "
                              f"{stats['frames']} frames | label={session.label}", flush=True)
                    if session.consumer.done():
                        session.consumer.result()
            except asyncio.CancelledError:
                session.request_stop("user")
            except Exception as exc:
                session.fault("capture_failure", str(exc))
                print(f"Capture error: {exc}", file=sys.stderr)
            finally:
                remove_labels()

                async def finalize():
                    try:
                        await session.stop_monitor()
                    finally:
                        if subscribed and session.connected:
                            try:
                                await asyncio.wait_for(client.stop_notify(notify_char), args.timeout)
                            except Exception as exc:
                                session.fault("unsubscribe_failure", str(exc))
                        session.closing = True
                        session.monitor_end_t = recorder.now()
                        try:
                            result = await session.finish()
                        finally:
                            session.worker.request_close()
                            await session.worker.wait_closed()
                            with contextlib.suppress(asyncio.CancelledError, OSError):
                                await session.consumer
                    return result

                cleanup = asyncio.create_task(finalize())
                try:
                    while True:
                        try:
                            complete = await asyncio.shield(cleanup)
                            break
                        except asyncio.CancelledError:
                            session.request_stop("user")
                            if cleanup.done():
                                complete = cleanup.result()
                                break
                finally:
                    if signal_installed:
                        loop.remove_signal_handler(signal.SIGINT)
                        signal.signal(signal.SIGINT, previous_sigint)
    except Exception as exc:
        recorder.record("event", code="connection_failure", message=str(exc), incomplete=True)
        recorder.record("summary", complete=False, frames=session.frames if session else 0,
                        stop_confirmed=False, reason="connection_failure")
        print(f"Capture error: {exc}", file=sys.stderr)
        complete = False
    finally:
        recorder.close()
    print(f"{'Complete' if complete else 'INCOMPLETE'} capture saved: {path}")
    if not complete:
        print("Do not treat this as a lossless capture. Check RX/writer rates, host backlog and events before retrying.", file=sys.stderr)
    return 0 if complete else 2


def write_csv(path, rows, fields):
    with Path(path).expanduser().open("x", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def show_rows(rows, fields, top):
    def cell(value):
        return str(value).replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n").replace("\x1b", "\\x1b")

    print("\t".join(fields))
    for row in rows[:top]:
        print("\t".join(cell(row.get(key, "")) for key in fields))
    print(f"{len(rows)} rows; showing {min(top, len(rows))}.")


def build_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    scan = sub.add_parser("scan", help="scan BLE adapters (no vehicle requests)")
    scan.add_argument("--scan-timeout", type=positive_float, default=5.0)
    cap = sub.add_parser("capture", help="receive-only classic CAN capture over BLE")
    device = cap.add_mutually_exclusive_group()
    device.add_argument("--name", help="unique adapter name substring")
    device.add_argument("--address", help="BLE address, or CoreBluetooth UUID on macOS")
    cap.add_argument("--scan-timeout", type=positive_float, default=5.0)
    cap.add_argument("--protocol", choices=PROTOCOLS, default="6",
                     help="6=11/500k, 7=29/500k, 8=11/250k, 9=29/250k (default: 6)")
    cap.add_argument("--duration", type=positive_float, default=60.0, help="monitor seconds (default: 60)")
    cap.add_argument("--timeout", type=positive_float, default=3.0, help="adapter command timeout seconds")
    cap.add_argument("--buffer-mb", type=buffer_megabytes, default=64,
                     help="host receive budget in MiB, including estimated item overhead (1–1024; default: 64)")
    cap.add_argument("--drain-timeout", type=positive_float, default=30.0,
                     help="seconds to drain host backlog after stopping adapter (default: 30)")
    cap.add_argument("--label", type=label_text, default="baseline")
    cap.add_argument("--note", type=note_text, default="", help="local vehicle/experiment notes")
    cap.add_argument("--out", required=True, help="new JSONL file; parent directory must exist")
    cap.add_argument("--no-input", action="store_true", help="disable interactive local labels")
    cap.add_argument("--can-id", type=hex_id, help="exact hexadecimal receive ID")
    cap.add_argument("--filter", dest="can_filter", type=hex_id, help="hexadecimal hardware filter")
    cap.add_argument("--mask", type=hex_id, help="hexadecimal hardware mask (1 bits must match)")
    for action in ("summary", "analyze"):
        cmd = sub.add_parser(action, help="offline traffic inventory" if action == "summary" else "offline labelled bit association")
        cmd.add_argument("files", nargs="+", type=Path)
        cmd.add_argument("--allow-incomplete", action="store_true", help="diagnostic inspection only; keep loss warnings")
        cmd.add_argument("--top", type=positive_int, default=50 if action == "summary" else 20)
        cmd.add_argument("--out", help="new CSV file; exports all rows, not just --top")
        if action == "analyze":
            cmd.add_argument("--state-a", required=True, type=label_text)
            cmd.add_argument("--state-b", required=True, type=label_text)
            cmd.add_argument("--settle", type=nonnegative_float, default=1.0)
            cmd.add_argument("--min-samples", type=positive_int, default=5)
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)
    try:
        if args.action == "capture":
            return asyncio.run(capture(args))
        if args.action == "scan":
            import elm327_ble_dump as transport
            transport._load_bleak()
            transport.print_rows(asyncio.run(transport.scan_rows(args.scan_timeout)))
            return 0
        import can_discovery_analysis as analysis
        if args.action == "summary":
            rows, warnings = analysis.summarize_files(args.files, allow_incomplete=args.allow_incomplete)
            fields = analysis.SUMMARY_FIELDS
        else:
            rows, warnings = analysis.analyze_files(
                args.files, args.state_a, args.state_b, settle=args.settle,
                min_samples=args.min_samples, allow_incomplete=args.allow_incomplete)
            fields = analysis.CANDIDATE_FIELDS
            print("Candidates only, not verified signals. byte=0-based, bit=LSB0.")
        for warning in warnings:
            print(f"WARNING: {warning}", file=sys.stderr)
        if args.out:
            write_csv(args.out, rows, fields)
        display_fields = (["id", "dlc", "frames", "host_receive_hz", "varying_bytes", "varying_mask"]
                          if args.action == "summary" else
                          ["id", "dlc", "byte", "bit", "occupancy_a", "occupancy_b", "score", "flags"])
        show_rows(rows, display_fields, args.top)
        if args.out:
            print(f"All columns exported: {Path(args.out).expanduser()}")
        return 0
    except (ValueError, OSError, asyncio.TimeoutError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        print("Interrupted.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
