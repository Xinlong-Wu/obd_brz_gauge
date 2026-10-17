"""Receive-only CAN discovery tests; never connect to a real BLE adapter.

Run just this module with:
    python3 -B -m unittest discover -s tests -p 'test_elm327_can_discover.py'
"""

import argparse
import asyncio
import contextlib
import csv
import io
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import elm327_can_discover as discover
import elm327_ble_dump as transport


class FakeCharacteristic:
    def __init__(self, uuid, properties):
        self.uuid = uuid
        self.properties = list(properties)


class FakeBleClient:
    """UART fake: notifications, not write completion, finish AT commands.

    ATMA deliberately has no success prompt. It streams until a bare CR is
    written, just as a monitor-mode adapter must, rather than replying like an
    ordinary request/response AT command.
    """

    def __init__(self, *, responses=None, monitor_chunks=None):
        self.write_char = FakeCharacteristic(
            transport._u16(0xFFF1), ["write", "write-without-response"])
        self.notify_char = FakeCharacteristic(transport._u16(0xFFF2), ["notify"])
        self.services = [SimpleNamespace(
            uuid=transport._u16(0xFFF0),
            characteristics=[self.write_char, self.notify_char])]
        self.responses = dict(responses or {})
        self.monitor_chunks = (
            (b"AT", b"MA\r", b"7E", b"8 01 02\r")
            if monitor_chunks is None else monitor_chunks)
        self.stop_chunks = (b"STOP", b"PED\r", b">")
        self.notification = None
        self.disconnected_callback = None
        self.subscription_error = None
        self.write_errors = {}
        self.hang_payloads = set()
        self.cancelled_writes = []
        self.writes = []
        self.write_options = []
        self.write_started = asyncio.Event()
        self.never = asyncio.Event()
        self.active_writes = 0
        self.max_active_writes = 0
        self.start_notify_calls = []
        self.stop_notify_calls = []
        self.entered = False
        self.exited = False
        self.streaming = False

    async def __aenter__(self):
        self.entered = True
        return self

    async def __aexit__(self, *exc):
        self.exited = True
        self.streaming = False

    async def start_notify(self, characteristic, callback):
        self.start_notify_calls.append(characteristic)
        if self.subscription_error is not None:
            raise self.subscription_error
        self.notification = callback

    async def stop_notify(self, characteristic):
        self.stop_notify_calls.append(characteristic)
        self.notification = None

    async def emit(self, chunks):
        if chunks is not None:
            for chunk in chunks:
                self.notification(self.notify_char, bytearray(chunk))
                await asyncio.sleep(0)

    def disconnect(self):
        self.streaming = False
        self.disconnected_callback(self)

    async def write_gatt_char(self, characteristic, payload, *, response):
        payload = bytes(payload)
        self.writes.append(payload)
        self.write_options.append((characteristic, response))
        self.active_writes += 1
        self.max_active_writes = max(self.max_active_writes, self.active_writes)
        self.write_started.set()
        try:
            if payload in self.hang_payloads:
                try:
                    await self.never.wait()
                except asyncio.CancelledError:
                    self.cancelled_writes.append(payload)
                    raise
            if payload in self.write_errors:
                raise self.write_errors[payload]
            if payload == b"\r":
                if self.stop_chunks is not None:
                    self.streaming = False
                chunks = self.stop_chunks
            else:
                command = payload.decode("ascii").removesuffix("\r")
                if command == "ATMA":
                    self.streaming = True
                    chunks = self.monitor_chunks
                elif command in self.responses:
                    chunks = self.responses[command]
                elif command == "ATZ":
                    chunks = (b"ATZ\r", b"ELM", b"327 v1.5\r", b">")
                else:
                    chunks = (command.encode("ascii") + b"\r", b"O", b"K\r", b">")
            await self.emit(chunks)
        finally:
            self.active_writes -= 1


class ArgumentsAndFiltersTests(unittest.TestCase):
    def parse(self, *argv):
        with contextlib.redirect_stderr(io.StringIO()):
            return discover.build_parser().parse_args(argv)

    def test_positive_cli_floats_reject_nonfinite_zero_and_negative(self):
        cases = [(["scan"], "--scan-timeout")]
        cases += [(["capture", "--out", "capture.jsonl"], option)
                  for option in ("--scan-timeout", "--duration", "--timeout", "--drain-timeout")]
        for prefix, option in cases:
            for value in ("nan", "NaN", "inf", "+inf", "-inf", "1e309", "-1e309", "0", "-1"):
                with self.subTest(option=option, value=value, action=prefix[0]):
                    with self.assertRaises(SystemExit) as raised:
                        self.parse(*prefix, f"{option}={value}")
                    self.assertEqual(raised.exception.code, 2)

    def test_positive_float_values_and_defaults_are_finite(self):
        args = self.parse("capture", "--out", "new.jsonl", "--duration", "0.001",
                          "--timeout", "2", "--scan-timeout", "0.5")
        self.assertEqual((args.duration, args.timeout, args.scan_timeout), (0.001, 2, 0.5))
        defaults = self.parse("capture", "--out", "new.jsonl")
        for value in (defaults.duration, defaults.timeout, defaults.scan_timeout):
            self.assertTrue(math.isfinite(value) and value > 0)

    def test_host_buffer_cli_bounds_and_drain_defaults(self):
        defaults = self.parse("capture", "--out", "new.jsonl")
        self.assertEqual(defaults.buffer_mb, 64)
        self.assertEqual(defaults.drain_timeout, 30)
        for value in ("1", "64", "1024"):
            args = self.parse("capture", "--out", "new.jsonl", "--buffer-mb", value,
                              "--drain-timeout", "0.25")
            self.assertEqual(args.buffer_mb, int(value))
            self.assertEqual(args.drain_timeout, 0.25)
        for value in ("0", "-1", "1025", "1.5", "nan", "inf"):
            with self.subTest(value=value), self.assertRaises(SystemExit):
                self.parse("capture", "--out", "new.jsonl", f"--buffer-mb={value}")

    def test_settle_accepts_zero_but_rejects_negative_and_nonfinite(self):
        prefix = ["analyze", "capture.jsonl", "--state-a", "closed", "--state-b", "open"]
        for value in ("0", "0.25"):
            with self.subTest(valid=value):
                args = self.parse(*prefix, "--settle", value)
                self.assertEqual(args.settle, float(value))
        for value in ("-1", "nan", "inf", "-inf", "1e309"):
            with self.subTest(invalid=value), self.assertRaises(SystemExit):
                self.parse(*prefix, f"--settle={value}")

    def test_integer_bounds_and_protocol_choices(self):
        cases = [(["summary", "capture.jsonl"], "--top"),
                 (["analyze", "capture.jsonl", "--state-a", "a", "--state-b", "b"], "--top"),
                 (["analyze", "capture.jsonl", "--state-a", "a", "--state-b", "b"], "--min-samples")]
        for prefix, option in cases:
            for value in ("0", "-1", "1.5", "nan", "inf"):
                with self.subTest(option=option, value=value), self.assertRaises(SystemExit):
                    self.parse(*prefix, f"{option}={value}")
            self.assertEqual(getattr(self.parse(*prefix, option, "1"), option[2:].replace("-", "_")), 1)
        for protocol in ("0", "5", "A", "6\r0100"):
            with self.subTest(protocol=protocol), self.assertRaises(SystemExit):
                self.parse("capture", "--out", "new.jsonl", "--protocol", protocol)

    def test_hexadecimal_input_cannot_inject_adapter_commands(self):
        invalid = ("", "0x", "-1", "+1", "123456789", "G00", "7E8\r0100",
                   "7E8\nATMA", "7E8;0100", "7E8 0100", "7E8\x00", "7E8>ATZ")
        for option in ("--can-id", "--filter", "--mask"):
            for value in invalid:
                with self.subTest(option=option, value=value), self.assertRaises(SystemExit):
                    self.parse("capture", "--out", "new.jsonl", f"{option}={value}")
        for text, expected in (("0", 0), ("7e8", 0x7E8), ("0x7E8", 0x7E8),
                               ("0X1fffffff", 0x1FFFFFFF)):
            self.assertEqual(discover.hex_id(text), expected)

    def test_label_validation_keeps_printable_command_like_text_local(self):
        self.assertEqual(discover.label_text("  roof; 0100 ATZ  "), "roof; 0100 ATZ")
        for label in ("", "   ", "x" * 129, "roof\r0100", "roof\nATZ", "a\x00b", "a\tb"):
            with self.subTest(label=label), self.assertRaises(argparse.ArgumentTypeError):
                discover.label_text(label)

    def test_exact_and_mask_filters_have_protocol_specific_widths(self):
        for protocol in ("6", "8", "7", "9"):
            bits = 11 if protocol in ("6", "8") else 29
            width = 3 if bits == 11 else 8
            limit = (1 << bits) - 1
            with self.subTest(protocol=protocol):
                self.assertEqual(discover.filter_commands(protocol),
                                 ["ATCRA", "ATCF" + "0" * width, "ATCM" + "0" * width])
                for value in (0, 1, limit):
                    self.assertEqual(discover.filter_commands(protocol, can_id=value),
                                     [f"ATCRA{value:0{width}X}"])
                for value, mask in ((0, 0), (limit, 0), (0, limit), (limit, limit)):
                    commands = discover.filter_commands(protocol, can_filter=value, mask=mask)
                    self.assertEqual(commands, [f"ATCF{value:0{width}X}", f"ATCM{mask:0{width}X}"])
                    for command in commands:
                        self.assertRegex(command, r"\AAT(?:CF|CM)[0-9A-F]+\Z")

    def test_every_filter_field_rejects_out_of_range_for_11_and_29_bits(self):
        for protocol in ("6", "8", "7", "9"):
            limit = 0x7FF if protocol in ("6", "8") else 0x1FFFFFFF
            for value in (-1, limit + 1):
                for fields in ({"can_id": value}, {"can_filter": value, "mask": 0},
                               {"can_filter": 0, "mask": value}):
                    with self.subTest(protocol=protocol, fields=fields), self.assertRaises(ValueError):
                        discover.filter_commands(protocol, **fields)

    def test_exact_filter_and_mask_pair_are_mutually_exclusive_and_complete(self):
        for fields in ({"can_id": 1, "can_filter": 1}, {"can_id": 1, "mask": 1},
                       {"can_id": 1, "can_filter": 1, "mask": 1},
                       {"can_filter": 0}, {"mask": 0}):
            with self.subTest(fields=fields), self.assertRaises(ValueError):
                discover.filter_commands("6", **fields)


class FramingAndParsingTests(unittest.TestCase):
    def test_fragmented_headers_crlf_and_prompt_are_framed(self):
        framer = discover.LineFramer()
        output = []
        chunks = (b"\r\n7", b"E", b"8 08 01", b" 02\r", b"\n18 DA ", b"F1 10 AA\n", b"  >")
        for chunk in chunks:
            output.extend(framer.feed(chunk))
        self.assertEqual(output, [("line", "7E8 08 01 02"), ("line", "18 DA F1 10 AA"),
                                  ("prompt", ">")])
        self.assertEqual(discover.parse_can_line(output[0][1], "6"), (0x7E8, b"\x08\x01\x02"))
        self.assertEqual(discover.parse_can_line(output[1][1], "7"), (0x18DAF110, b"\xAA"))

    def test_all_single_byte_notification_splits_preserve_frames(self):
        for protocol, wire, expected in (
                ("6", b"7e8 08 41 00\r\n>", (0x7E8, b"\x08\x41\x00")),
                ("7", b"18 DA F1 10 01 02\r>", (0x18DAF110, b"\x01\x02"))):
            with self.subTest(protocol=protocol):
                framer = discover.LineFramer()
                events = [event for byte in wire for event in framer.feed(bytes([byte]))]
                self.assertEqual([kind for kind, _ in events], ["line", "prompt"])
                self.assertEqual(discover.parse_can_line(events[0][1], protocol), expected)

    def test_dlc_disabled_payload_is_not_stripped_as_a_length_field(self):
        for protocol, header in (("6", "7FF"), ("8", "000"),
                                 ("7", "1FFFFFFF"), ("9", "00000000")):
            with self.subTest(protocol=protocol):
                identifier = int(header, 16)
                self.assertEqual(discover.parse_can_line(header, protocol), (identifier, b""))
                self.assertEqual(discover.parse_can_line(header + " 08", protocol), (identifier, b"\x08"))
                payload = bytes(range(8))
                self.assertEqual(discover.parse_can_line(header + payload.hex(), protocol), (identifier, payload))

    def test_malformed_lines_and_dlc_enabled_output_are_rejected(self):
        common = ("", "OK", "ATMA", "BUFFER FULL", "NO DATA", ">", "7E", "7E8 0", "7E8 GG",
                  "7E8: 01 02", "0: 7E8 01", "[7E8] 01", "7E8 8 00 01 02 03 04 05 06 07",
                  "7E8 00 01 02 03 04 05 06 07 08", "7E8 \ufffd")
        for text in common + ("800 01", "FFF"):
            with self.subTest(protocol="6", text=text), self.assertRaises(ValueError):
                discover.parse_can_line(text, "6")
        for text in ("18DAF11", "20000000 01", "FFFFFFFF 01", "18DAF110 0",
                     "18DAF110 8 00 01 02 03 04 05 06 07", "18DAF110 000102030405060708"):
            with self.subTest(protocol="7", text=text), self.assertRaises(ValueError):
                discover.parse_can_line(text, "7")

    def test_unterminated_line_is_bounded_and_framer_recovers(self):
        framer = discover.LineFramer(limit=8)
        self.assertEqual(list(framer.feed(b"12345678")), [])
        with self.assertRaises(ValueError):
            list(framer.feed(b"9"))
        self.assertEqual(list(framer.feed(b"7E8 01\r>")), [("line", "7E8 01"), ("prompt", ">")])


class RecorderAndCsvTests(unittest.TestCase):
    def test_real_recorder_preserves_metadata_and_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.jsonl"
            recorder = discover.Recorder(path)
            try:
                recorder.record("marker", label="车顶 open; 0100")
                recorder.record("frame", t=1.25, id=0x7E8, dlc=2, data="0102")
                recorder.flush()
                original = path.read_bytes()
                with self.assertRaises(FileExistsError):
                    discover.Recorder(path)
                self.assertEqual(path.read_bytes(), original)
            finally:
                recorder.close()
            rows = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
            self.assertEqual(rows[0]["label"], "车顶 open; 0100")
            self.assertTrue(math.isfinite(rows[0]["t"]) and rows[0]["t"] >= 0)
            self.assertEqual(rows[1]["t"], 1.25)

    def test_csv_is_readable_and_existing_output_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "inventory.csv"
            expected = [{"id": "7E8", "label": 'roof, "open"; 0100'}]
            discover.write_csv(path, expected, ["id", "label"])
            with path.open(encoding="utf-8-sig", newline="") as stream:
                self.assertEqual(list(csv.DictReader(stream)), expected)
            original = path.read_bytes()
            with self.assertRaises(FileExistsError):
                discover.write_csv(path, [], ["id", "label"])
            self.assertEqual(path.read_bytes(), original)


class AsyncSessionTestCase(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary.name)
        self.sessions = []
        self.capture_count = 0

    async def asyncTearDown(self):
        for session in self.sessions:
            session.closing = True
            if session.consumer is not None:
                session.consumer.cancel()
                with contextlib.suppress(asyncio.CancelledError, OSError):
                    await session.consumer
            session.worker.request_close()
            with contextlib.suppress(OSError):
                await session.worker.wait_closed()
            session.recorder.close()
        self.temporary.cleanup()

    async def make_session(self, *, protocol="6", timeout=0.2, queue_size=4096,
                           client=None, start_consumer=True):
        client = client or FakeBleClient()
        recorder = discover.Recorder(self.directory / f"capture-{len(self.sessions)}.jsonl")
        session = discover.MonitorSession(client, client.write_char, recorder,
                                          protocol, timeout, queue_size)
        self.sessions.append(session)
        client.disconnected_callback = session.on_disconnect
        await client.start_notify(client.notify_char, session.on_notify)
        if start_consumer:
            session.consumer = asyncio.create_task(session.consume())
        return session, client

    async def drain(self, session):
        await asyncio.wait_for(session.queue.join(), 3)
        # Worker faults are posted to the event loop rather than mutating the
        # session from the writer thread.
        await asyncio.sleep(0)

    async def rows(self, session):
        await self.drain(session)
        session.recorder.flush()
        return [json.loads(line) for line in Path(session.recorder.file.name).read_text(encoding="utf-8").splitlines()]

    async def summary(self, session):
        return [row for row in await self.rows(session) if row["type"] == "summary"][-1]

    def assert_receive_only(self, client):
        for payload in client.writes:
            self.assertTrue(payload == b"\r" or
                            (payload.startswith(b"AT") and payload.endswith(b"\r")
                             and b"\r" not in payload[:-1] and b"\n" not in payload), payload)
        self.assertLessEqual(client.max_active_writes, 1)


class MonitorSessionTests(AsyncSessionTestCase):
    async def test_initialization_requires_silent_raw_headered_can_before_monitoring(self):
        for protocol in ("6", "7", "8", "9"):
            with self.subTest(protocol=protocol):
                header = b"7E8" if protocol in ("6", "8") else b"18DAF110"
                client = FakeBleClient(monitor_chunks=(b"ATMA\r", header + b" 01 02\r"))
                session, client = await self.make_session(protocol=protocol, client=client)
                filters = discover.filter_commands(protocol)
                await session.initialize(filters, reset_delay=0)
                required = ["ATZ", "ATE0", "ATL0", "ATS0", "ATH1", "ATCAF0", "ATD0",
                            "ATCFC0", "ATCSM1", "ATSP" + protocol, *filters]
                self.assertEqual(client.writes, [(command + "\r").encode("ascii") for command in required])
                await session.start_monitor("baseline")
                await self.drain(session)
                self.assertTrue(client.streaming)
                self.assertFalse(session.stop_confirmed)
                self.assertEqual(session.frames, 1)
                await session.stop_monitor()
                self.assertFalse(client.streaming)
                self.assertTrue(await session.finish())
                self.assertTrue((await self.summary(session))["complete"])
                self.assertEqual(client.writes[-2:], [b"ATMA\r", b"\r"])
                self.assert_receive_only(client)

    async def test_every_required_initialization_command_must_acknowledge(self):
        required = ["ATZ", "ATE0", "ATL0", "ATS0", "ATH1", "ATCAF0", "ATD0",
                    "ATCFC0", "ATCSM1", "ATSP6", "ATCRA7E8"]
        for failed in required:
            with self.subTest(command=failed):
                session, client = await self.make_session(client=FakeBleClient(responses={failed: (b"?\r>",)}))
                with self.assertRaises(ValueError):
                    await session.initialize(["ATCRA7E8"], reset_delay=0)
                self.assertEqual(client.writes[-1], (failed + "\r").encode("ascii"))
                self.assertNotIn(b"ATMA\r", client.writes)
                self.assertFalse(await session.finish())
                self.assert_receive_only(client)

    async def test_csm_and_cfc_fail_closed_for_missing_or_negative_acknowledgements(self):
        for command in ("ATCSM1", "ATCFC0"):
            for response in (b"?\r>", b"ERROR\r>", b"NO DATA\r>", b">", b"OK\rERROR\r>"):
                with self.subTest(command=command, response=response):
                    client = FakeBleClient(responses={command: (response,)})
                    session, client = await self.make_session(client=client)
                    with self.assertRaises(ValueError):
                        await session.initialize(discover.filter_commands("6"), reset_delay=0)
                    self.assertNotIn(b"ATMA\r", client.writes)
                    self.assert_receive_only(client)

    async def test_reset_accepts_supported_adapter_banners_but_not_bare_ok(self):
        for banner in (b"ELM327 v2.1", b"STN1170 v5.0", b"OBDLink MX+"):
            with self.subTest(banner=banner):
                client = FakeBleClient(responses={"ATZ": (banner[:3], banner[3:] + b"\r>")})
                session, _ = await self.make_session(client=client)
                await session.command("ATZ", reset=True)
        session, _ = await self.make_session(client=FakeBleClient(responses={"ATZ": (b"OK\r>",)}))
        with self.assertRaises(ValueError):
            await session.command("ATZ", reset=True)

    async def test_overlapping_commands_are_rejected_without_second_write(self):
        session, client = await self.make_session(client=FakeBleClient(responses={"ATE0": None}))
        task = asyncio.create_task(session.command("ATE0"))
        try:
            await asyncio.wait_for(client.write_started.wait(), 1)
            with self.assertRaises(RuntimeError):
                await session.command("ATH1")
            self.assertEqual(client.writes, [b"ATE0\r"])
            await client.emit((b"ATE", b"0\rO", b"K\r", b">"))
            self.assertIn("OK", await asyncio.wait_for(task, 1))
            await session.command("ATH1")
            self.assertEqual(client.writes, [b"ATE0\r", b"ATH1\r"])
        finally:
            task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await task
        self.assert_receive_only(client)

    async def test_commands_cannot_interrupt_active_monitor(self):
        session, client = await self.make_session()
        await session.start_monitor("baseline")
        before = list(client.writes)
        with self.assertRaises(RuntimeError):
            await session.command("ATH1")
        self.assertEqual(client.writes, before)
        await session.stop_monitor()
        self.assertTrue(await session.finish())

    async def test_write_property_selects_acknowledged_or_unacknowledged_uart_write(self):
        for properties, response in ((["write"], True), (["write-without-response"], False)):
            with self.subTest(properties=properties):
                client = FakeBleClient()
                client.write_char.properties = properties
                session, _ = await self.make_session(client=client)
                await session.command("ATE0")
                self.assertEqual(client.write_options, [(client.write_char, response)])

    async def test_monitor_consumes_fragmented_29_bit_headers_and_raw_notifications(self):
        chunks = (b"ATMA\r", b"18 ", b"DA F1", b" 10 08 41", b" 00\r\n", b"1FFFFFFF\r")
        session, client = await self.make_session(protocol="7", client=FakeBleClient(monitor_chunks=chunks))
        await session.start_monitor("baseline")
        await self.drain(session)
        frames = [row for row in await self.rows(session) if row["type"] == "frame"]
        self.assertEqual([(row["id"], row["dlc"], row["data"]) for row in frames],
                         [(0x18DAF110, 3, "084100"), (0x1FFFFFFF, 0, "")])
        self.assertEqual([row["seq"] for row in frames], [1, 2])
        received = [bytes.fromhex(row["hex"]) for row in await self.rows(session) if row["type"] == "rx"]
        self.assertEqual(received, list(chunks))
        self.assertTrue(all(math.isfinite(row["t"]) and row["t"] >= 0 for row in frames))
        await session.stop_monitor()
        self.assertTrue(await session.finish())
        self.assert_receive_only(client)

    async def test_labels_are_local_metadata_and_never_adapter_commands(self):
        session, client = await self.make_session()
        await session.start_monitor("baseline")
        before = list(client.writes)
        session.mark("  roof open; 0100 ATZ  ")
        session.mark("roof open; 0100 ATZ")
        session.mark("roof closed")
        with self.assertRaises(argparse.ArgumentTypeError):
            session.mark("roof\r0100")
        self.assertEqual(client.writes, before)
        markers = [row["label"] for row in await self.rows(session) if row["type"] == "marker"]
        self.assertEqual(markers, ["baseline", "roof open; 0100 ATZ", "roof closed"])
        await session.stop_monitor()
        self.assertTrue(await session.finish())
        self.assert_receive_only(client)

    async def test_unexpected_prompt_marks_capture_incomplete_even_with_a_frame(self):
        session, client = await self.make_session()
        await session.start_monitor("baseline")
        await client.emit((b">",))
        await self.drain(session)
        self.assertTrue(session.stop_event.is_set())
        self.assertTrue(session.incomplete)
        await session.stop_monitor()
        self.assertFalse(await session.finish())
        summary = (await self.summary(session))
        self.assertFalse(summary["complete"])
        self.assertGreater(summary["frames"], 0)
        self.assertIn("unexpected_prompt", summary["events"])
        self.assertNotIn(b"\r", client.writes)

    async def test_buffer_full_and_other_adapter_errors_are_never_frames(self):
        for error in (b"BUFFER FULL", b"BUS BUSY", b"CAN ERROR", b"NO DATA", b"STOPPED", b"?"):
            with self.subTest(error=error):
                session, client = await self.make_session()
                await session.start_monitor("baseline")
                await client.emit((error[:3], error[3:] + b"\r"))
                await self.drain(session)
                self.assertTrue(session.incomplete)
                self.assertTrue(session.stop_event.is_set())
                self.assertEqual(session.frames, 1)
                await session.stop_monitor()
                self.assertFalse(await session.finish())
                self.assertIn("adapter_error", (await self.summary(session))["events"])
                self.assert_receive_only(client)

    async def test_malformed_monitor_lines_fail_closed_without_counting_them(self):
        for line in (b"7E8 0\r", b"800 01\r", b"7E8: 01 02\r", b"7E8 GG\r", b"\xff\r"):
            with self.subTest(line=line):
                session, client = await self.make_session()
                await session.start_monitor("baseline")
                await client.emit((line,))
                await self.drain(session)
                self.assertEqual(session.frames, 1)
                self.assertTrue(session.stop_event.is_set())
                await session.stop_monitor()
                self.assertFalse(await session.finish())
                self.assertIn("malformed_frame", (await self.summary(session))["events"])

    async def test_unterminated_oversize_notification_is_a_capture_fault(self):
        session, client = await self.make_session()
        await session.start_monitor("baseline")
        await client.emit((b"A" * 513,))
        await self.drain(session)
        self.assertTrue(session.stop_event.is_set())
        await session.stop_monitor()
        self.assertFalse(await session.finish())
        self.assertIn("framing_error", (await self.summary(session))["events"])

    async def test_disconnection_stops_capture_and_does_not_write_to_dead_transport(self):
        session, client = await self.make_session()
        await session.start_monitor("baseline")
        await self.drain(session)
        client.disconnect()
        before = list(client.writes)
        self.assertTrue(session.stop_event.is_set())
        await session.stop_monitor()
        self.assertEqual(client.writes, before)
        self.assertFalse(await session.finish())
        summary = (await self.summary(session))
        self.assertFalse(summary["stop_confirmed"])
        self.assertIn("disconnected", summary["events"])
        self.assertIn("stop_unconfirmed", summary["events"])

    async def test_disconnect_releases_command_waiter(self):
        session, client = await self.make_session(client=FakeBleClient(responses={"ATE0": None}))
        task = asyncio.create_task(session.command("ATE0"))
        try:
            await asyncio.wait_for(client.write_started.wait(), 1)
            client.disconnect()
            with self.assertRaises(ConnectionError):
                await asyncio.wait_for(task, 1)
            self.assertTrue(session.incomplete)
            self.assertEqual(client.writes, [b"ATE0\r"])
        finally:
            task.cancel()
            with contextlib.suppress(asyncio.CancelledError, ConnectionError):
                await task

    async def test_queue_overflow_is_reported_and_cannot_produce_complete_summary(self):
        client = FakeBleClient(monitor_chunks=())
        client.stop_chunks = (b"STOPPED\r>",)
        session, client = await self.make_session(client=client, queue_size=1, start_consumer=False)
        await session.start_monitor("baseline")
        session.on_notify(client.notify_char, b"7E8 01\r")
        session.on_notify(client.notify_char, b"7E9 02\r")
        self.assertTrue(session.stop_event.is_set())
        self.assertTrue(session.incomplete)
        session.consumer = asyncio.create_task(session.consume())
        await self.drain(session)
        await session.stop_monitor()
        self.assertFalse(await session.finish())
        summary = (await self.summary(session))
        # Overflow latches raw admission closed: the rejected frame and the
        # later STOPPED/prompt notification are both accounted as lost bytes.
        self.assertEqual(summary["queue_drops"], 2)
        self.assertEqual(summary["dropped_bytes"], len(b"7E9 02\rSTOPPED\r>"))
        self.assertEqual(summary["frames"], 1)
        self.assertIn("host_queue_overflow", summary["events"])
        self.assertTrue(summary["stop_confirmed"])

    async def test_stop_timeout_is_bounded_and_never_claims_confirmation(self):
        client = FakeBleClient()
        client.stop_chunks = None
        session, client = await self.make_session(client=client, timeout=0.02)
        await session.start_monitor("baseline")
        await asyncio.wait_for(session.stop_monitor(), 1)
        self.assertFalse(await session.finish())
        summary = (await self.summary(session))
        self.assertFalse(summary["stop_confirmed"])
        self.assertIn("stop_unconfirmed", summary["events"])
        self.assertEqual(client.writes, [b"ATMA\r", b"\r"])
        self.assert_receive_only(client)

    async def test_hung_uart_write_is_cancelled_within_command_timeout(self):
        client = FakeBleClient()
        client.hang_payloads.add(b"ATE0\r")
        session, client = await self.make_session(client=client, timeout=0.02)
        with self.assertRaises(asyncio.TimeoutError):
            await asyncio.wait_for(session.command("ATE0"), 1)
        self.assertEqual(client.cancelled_writes, [b"ATE0\r"])
        await session.command("ATH1")
        self.assert_receive_only(client)

    async def test_hung_stop_write_is_cancelled_and_capture_stays_incomplete(self):
        client = FakeBleClient()
        client.hang_payloads.add(b"\r")
        session, client = await self.make_session(client=client, timeout=0.02)
        await session.start_monitor("baseline")
        await asyncio.wait_for(session.stop_monitor(), 1)
        self.assertFalse(await session.finish())
        self.assertEqual(client.cancelled_writes, [b"\r"])
        self.assertIn("stop_unconfirmed", (await self.summary(session))["events"])

    async def test_stop_transport_errors_do_not_skip_final_drain_and_summary(self):
        for error in (OSError("UART unavailable"), RuntimeError("backend failed")):
            with self.subTest(error=type(error).__name__):
                client = FakeBleClient()
                client.write_errors[b"\r"] = error
                session, client = await self.make_session(client=client)
                await session.start_monitor("baseline")
                await session.stop_monitor()
                self.assertFalse(await session.finish())
                summary = (await self.summary(session))
                self.assertIn("stop_unconfirmed", summary["events"])
                self.assertFalse(summary["stop_confirmed"])
                self.assertIn("monitor_end", [row["type"] for row in await self.rows(session)])
                self.assert_receive_only(client)

    async def test_queued_spontaneous_prompt_is_not_answered_with_monitor_interrupt(self):
        session, client = await self.make_session()
        await session.start_monitor("baseline")
        await self.drain(session)
        # Do not yield before stop: the queued prompt must be consumed before
        # deciding whether CR could accidentally repeat the previous command.
        session.on_notify(client.notify_char, b">")
        await session.stop_monitor()
        self.assertEqual(client.writes, [b"ATMA\r"])
        self.assertFalse(await session.finish())
        self.assertIn("unexpected_prompt", (await self.summary(session))["events"])

    async def test_prompt_scheduled_before_interrupt_write_does_not_send_bare_cr(self):
        session, client = await self.make_session()
        await session.start_monitor("baseline")
        await self.drain(session)
        asyncio.get_running_loop().call_soon(session.on_notify, None, b">")
        await session.stop_monitor()
        self.assertEqual(client.writes, [b"ATMA\r"])
        self.assertTrue(session.stop_confirmed)
        self.assertTrue(session.incomplete)
        self.assertFalse(await session.finish())
        self.assertIn("unexpected_prompt", (await self.summary(session))["events"])

    async def test_selected_exact_and_mask_ids_are_checked_against_adapter_output(self):
        cases = (("6", 0x7E8, 0x7FF, 0x7E8, 0x7E9),
                 ("6", 0x7E0, 0x7F0, 0x7EF, 0x7F0),
                 ("7", 0x18DAF110, 0x1FFFFFFF, 0x18DAF110, 0x18DAF111),
                 ("7", 0x18DA0000, 0x1FFF0000, 0x18DAF110, 0x18DBF110))
        for protocol, value, mask, accepted, rejected in cases:
            width = 3 if protocol == "6" else 8
            with self.subTest(protocol=protocol, value=value, mask=mask):
                client = FakeBleClient(monitor_chunks=())
                session, client = await self.make_session(protocol=protocol, client=client)
                session.expected_filter = (value, mask)
                await session.start_monitor("baseline")
                await client.emit(((f"{accepted:0{width}X} 01\r").encode("ascii"),))
                await self.drain(session)
                self.assertFalse(session.incomplete)
                await client.emit(((f"{rejected:0{width}X} 02\r").encode("ascii"),))
                await self.drain(session)
                self.assertTrue(session.stop_event.is_set())
                await session.stop_monitor()
                self.assertFalse(await session.finish())
                self.assertIn("filter_mismatch", (await self.summary(session))["events"])
                self.assert_receive_only(client)

    async def test_zero_mask_accepts_all_valid_ids(self):
        session, client = await self.make_session()
        session.expected_filter = (0, 0)
        await session.start_monitor("baseline")
        await client.emit((b"000 01\r7FF 02\r",))
        await self.drain(session)
        self.assertEqual(session.frames, 3)
        self.assertFalse(session.incomplete)
        await session.stop_monitor()
        self.assertTrue(await session.finish())

    async def test_unterminated_output_at_end_is_not_silently_discarded(self):
        client = FakeBleClient()
        client.stop_chunks = None
        session, client = await self.make_session(client=client, timeout=0.02)
        await session.start_monitor("baseline")
        await client.emit((b"7E8 0",))
        await self.drain(session)
        await session.stop_monitor()
        self.assertFalse(await session.finish())
        self.assertIn("partial_line", (await self.summary(session))["events"])

    async def test_monitor_with_no_frames_has_incomplete_summary_despite_clean_stop(self):
        session, client = await self.make_session(client=FakeBleClient(monitor_chunks=(b"ATMA\r",)))
        await session.start_monitor("baseline")
        await session.stop_monitor()
        self.assertFalse(await session.finish())
        summary = (await self.summary(session))
        self.assertFalse(summary["complete"])
        self.assertEqual(summary["frames"], 0)
        self.assertEqual(summary["unique_ids"], 0)
        self.assertTrue(summary["stop_confirmed"])
        self.assertIn("no_frames", summary["events"])
        self.assert_receive_only(client)

    async def test_stop_without_monitor_does_not_send_anything_and_is_not_complete(self):
        session, client = await self.make_session()
        await session.stop_monitor()
        self.assertEqual(client.writes, [])
        self.assertFalse(await session.finish())
        self.assertFalse((await self.summary(session))["complete"])


class CaptureCleanupTests(AsyncSessionTestCase):
    async def run_capture(self, client, *, timeout=0.2, chars=None, extra_args=()):
        path = self.directory / f"cli-{self.capture_count}.jsonl"
        self.capture_count += 1
        args = discover.build_parser().parse_args([
            "capture", "--address", "FAKE-ADAPTER", "--out", str(path),
            "--timeout", str(timeout), "--duration", "0.001", "--no-input", *extra_args])
        session_type = discover.MonitorSession

        def make_session(*args, **kwargs):
            session = session_type(*args, **kwargs)
            self.sessions.append(session)
            return session

        def connect(address, **kwargs):
            self.assertEqual(address, "FAKE-ADAPTER")
            client.disconnected_callback = kwargs["disconnected_callback"]
            return client

        loop = asyncio.get_running_loop()
        with contextlib.ExitStack() as patches:
            patches.enter_context(mock.patch.object(transport, "_load_bleak"))
            factory = patches.enter_context(mock.patch.object(transport, "BleakClient", side_effect=connect))
            scan = patches.enter_context(mock.patch.object(
                transport, "scan_rows", new=mock.AsyncMock(side_effect=AssertionError("must not scan BLE"))))
            patches.enter_context(mock.patch.object(discover, "MonitorSession", side_effect=make_session))
            patches.enter_context(mock.patch.object(loop, "add_signal_handler"))
            patches.enter_context(mock.patch.object(loop, "remove_signal_handler"))
            if chars is not None:
                patches.enter_context(mock.patch.object(transport, "find_chars", return_value=chars))
            patches.enter_context(contextlib.redirect_stdout(io.StringIO()))
            patches.enter_context(contextlib.redirect_stderr(io.StringIO()))
            result = await asyncio.wait_for(discover.capture(args), 3)
        factory.assert_called_once()
        scan.assert_not_awaited()
        rows = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
        self.assertTrue(client.exited)
        self.assert_receive_only(client)
        return result, rows

    def assert_session_cleaned(self, session):
        self.assertTrue(session.consumer.done())
        self.assertFalse(session.worker.thread.is_alive())
        self.assertTrue(session.recorder.file.closed)

    async def test_fake_capture_streams_until_stop_and_closes_subscription_and_file(self):
        client = FakeBleClient()
        result, rows = await self.run_capture(client)
        self.assertEqual(result, 0)
        self.assertEqual(client.start_notify_calls, [client.notify_char])
        self.assertEqual(client.stop_notify_calls, [client.notify_char])
        self.assertEqual(client.writes[-2:], [b"ATMA\r", b"\r"])
        self.assertTrue(rows[-1]["complete"])
        self.assertEqual(rows[-1]["frames"], 1)
        self.assert_session_cleaned(self.sessions[-1])

    async def test_capture_checks_acknowledged_exact_and_mask_filter_behavior(self):
        for flags, accepted, rejected in (
                (("--can-id", "7E8"), b"7E8 01\r", b"7E9 02\r"),
                (("--filter", "7E0", "--mask", "7F0"), b"7EF 01\r", b"7F0 02\r")):
            with self.subTest(flags=flags):
                client = FakeBleClient(monitor_chunks=(accepted, rejected))
                result, rows = await self.run_capture(client, extra_args=flags)
                self.assertEqual(result, 2)
                self.assertFalse(rows[-1]["complete"])
                self.assertIn("filter_mismatch", rows[-1]["events"])
                self.assertEqual(client.writes[-2:], [b"ATMA\r", b"\r"])
                self.assert_session_cleaned(self.sessions[-1])

    async def test_failed_subscription_cleans_up_consumer_and_recorder_without_writing(self):
        client = FakeBleClient()
        client.subscription_error = OSError("notification subscription rejected")
        result, rows = await self.run_capture(client)
        self.assertEqual(result, 2)
        self.assertEqual(client.writes, [])
        self.assertEqual(client.stop_notify_calls, [])
        self.assertFalse(rows[-1]["complete"])
        self.assertEqual(rows[-1]["frames"], 0)
        self.assert_session_cleaned(self.sessions[-1])

    async def test_hung_initial_write_is_bounded_and_capture_resources_are_closed(self):
        client = FakeBleClient()
        client.hang_payloads.add(b"ATZ\r")
        result, rows = await asyncio.wait_for(self.run_capture(client, timeout=0.02), 1)
        self.assertEqual(result, 2)
        self.assertEqual(client.cancelled_writes, [b"ATZ\r"])
        self.assertNotIn(b"ATMA\r", client.writes)
        self.assertEqual(client.stop_notify_calls, [client.notify_char])
        self.assertFalse(rows[-1]["complete"])
        self.assert_session_cleaned(self.sessions[-1])

    async def test_untrusted_or_incapable_characteristics_are_rejected_without_commands(self):
        for service, write_props, notify_props in (
                (None, ["write"], ["notify"]), ("GENERIC", ["write"], ["notify"]),
                ("FFF0", ["read"], ["notify"]), ("FFF0", ["write"], ["read"])):
            with self.subTest(service=service, write=write_props, notify=notify_props):
                client = FakeBleClient()
                client.write_char.properties = write_props
                client.notify_char.properties = notify_props
                chars = service, client.write_char, client.notify_char
                result, rows = await self.run_capture(client, chars=chars)
                self.assertEqual(result, 2)
                self.assertEqual(client.writes, [])
                self.assertEqual(client.start_notify_calls, [])
                self.assertFalse(rows[-1]["complete"])

    async def test_existing_capture_is_not_overwritten_or_connected(self):
        path = self.directory / "existing.jsonl"
        original = b"do not replace this capture\n"
        path.write_bytes(original)
        args = discover.build_parser().parse_args([
            "capture", "--address", "FAKE-ADAPTER", "--out", str(path), "--no-input"])
        with mock.patch.object(transport, "_load_bleak") as load, \
                mock.patch.object(transport, "BleakClient") as connect, \
                mock.patch.object(transport, "scan_rows", new_callable=mock.AsyncMock) as scan:
            with self.assertRaises(FileExistsError):
                await discover.capture(args)
        load.assert_not_called()
        connect.assert_not_called()
        scan.assert_not_awaited()
        self.assertEqual(path.read_bytes(), original)


class OfflineCliTests(unittest.TestCase):
    # A fresh interpreter with an import hook simulates bleak being absent even
    # on machines that have it installed. No package discovery or BLE is needed.
    BLOCK_BLEAK = """
import builtins
import runpy
import sys
original_import = builtins.__import__
def without_bleak(name, *args, **kwargs):
    if name == 'bleak' or name.startswith('bleak.'):
        raise ModuleNotFoundError('bleak deliberately unavailable in offline test')
    return original_import(name, *args, **kwargs)
builtins.__import__ = without_bleak
sys.path.insert(0, sys.argv[1])
"""

    def run_offline(self, body, *args):
        return subprocess.run([sys.executable, "-B", "-c", self.BLOCK_BLEAK + body,
                               str(TOOLS), *args], cwd=str(ROOT),
                              capture_output=True, text=True, timeout=10, check=False)

    def test_discovery_and_transport_import_without_bleak(self):
        result = self.run_offline("""
import elm327_can_discover
import elm327_ble_dump
assert 'bleak' not in sys.modules
assert elm327_ble_dump.BleakClient is None
print('offline imports succeeded')
""")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("offline imports succeeded", result.stdout)

    def test_root_and_every_subcommand_help_work_without_bleak(self):
        body = """
script = sys.argv[1] + '/elm327_can_discover.py'
sys.argv = [script] + sys.argv[2:]
runpy.run_path(script, run_name='__main__')
"""
        for prefix in ((), ("scan",), ("capture",), ("summary",), ("analyze",)):
            with self.subTest(command=prefix):
                result = self.run_offline(body, *prefix, "--help")
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn("usage:", result.stdout)
                self.assertNotIn("Traceback", result.stderr)


if __name__ == "__main__":
    unittest.main()
