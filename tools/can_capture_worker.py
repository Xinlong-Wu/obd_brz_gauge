"""Bounded receive FIFO and single-owner CAN parsing/JSONL writer thread."""

import asyncio
from collections import Counter, deque
import threading
import time


class CaptureWorker:
    ITEM_OVERHEAD = 256
    CONTROL_LIMIT = 1024

    def __init__(self, recorder, framer, parser, protocol, fault_callback,
                 buffer_bytes=64 * 1024 * 1024, item_limit=None):
        self.recorder = recorder
        self.framer = framer
        self.parser = parser
        self.protocol = protocol
        self.fault_callback = fault_callback
        self.buffer_limit = buffer_bytes
        self.item_limit = item_limit
        self.condition = threading.Condition()
        self.items = deque()
        self.raw_times = deque()
        self.pending_bytes = 0
        self.peak_bytes = 0
        self.raw_items = 0
        self.controls = 0
        self.unfinished = 0
        self.rx_bytes = 0
        self.rx_chunks = 0
        self.processed_bytes = 0
        self.dropped_bytes = 0
        self.dropped_chunks = 0
        self.frames = 0
        self.ids = Counter()
        self.events = Counter()
        self.oldest_t = None
        self.inflight_t = None
        self.error = None
        self.accept_raw = True
        self.closing = False
        self.thread = None
        self.loop = None
        self.done = None
        self.waiters = []

    def start(self):
        if self.thread is not None:
            raise RuntimeError("capture worker already started")
        self.loop = asyncio.get_running_loop()
        self.done = self.loop.create_future()
        self.thread = threading.Thread(target=self._run, name="can-capture-writer")
        self.thread.start()

    def _append(self, item):
        self.items.append(item)
        self.unfinished += 1
        self.condition.notify()

    def receive(self, timestamp, chunk, mode, expected_filter):
        cost = len(chunk) + self.ITEM_OVERHEAD
        with self.condition:
            self.rx_bytes += len(chunk)
            self.rx_chunks += 1
            if (self.closing or self.error or not self.accept_raw or
                    self.pending_bytes + cost > self.buffer_limit or
                    (self.item_limit is not None and self.raw_items >= self.item_limit)):
                self.accept_raw = False
                self.dropped_chunks += 1
                self.dropped_bytes += len(chunk)
                return False
            self.pending_bytes += cost
            self.peak_bytes = max(self.peak_bytes, self.pending_bytes)
            self.raw_items += 1
            self.raw_times.append(timestamp)
            if self.oldest_t is None:
                self.oldest_t = timestamp
            self._append(("rx", timestamp, (chunk, mode, expected_filter), cost))
            return True

    def record(self, kind, t=None, **fields):
        timestamp = self.recorder.now() if t is None else t
        with self.condition:
            if self.error:
                raise OSError(f"capture writer failed: {self.error}")
            if self.closing:
                raise RuntimeError("capture writer is closed")
            if self.controls >= self.CONTROL_LIMIT:
                raise BufferError("capture metadata queue is full")
            self.controls += 1
            self._append(("record", timestamp, (kind, fields), 0))

    def flush(self):
        with self.condition:
            if self.error:
                raise OSError(f"capture writer failed: {self.error}")
            if self.closing:
                return
            if self.controls >= self.CONTROL_LIMIT:
                raise BufferError("capture metadata queue is full")
            self.controls += 1
            self._append(("flush", self.recorder.now(), None, 0))

    def snapshot(self):
        with self.condition:
            now = self.recorder.now()
            lag = max(0.0, now - self.oldest_t) if self.oldest_t is not None else 0.0
            return {
                "rx_bytes": self.rx_bytes, "rx_chunks": self.rx_chunks,
                "processed_bytes": self.processed_bytes,
                "queue_drops": self.dropped_chunks, "dropped_bytes": self.dropped_bytes,
                "buffer_bytes": self.pending_bytes, "buffer_limit_bytes": self.buffer_limit,
                "peak_buffer_bytes": self.peak_bytes, "writer_lag_seconds": lag,
                "frames": self.frames, "unique_ids": len(self.ids),
            }

    async def join(self):
        loop = asyncio.get_running_loop()
        waiter = loop.create_future()
        with self.condition:
            if self.error:
                raise OSError(f"capture writer failed: {self.error}")
            if not self.unfinished:
                return
            self.waiters.append(waiter)
        try:
            await waiter
        finally:
            with self.condition:
                if waiter in self.waiters:
                    self.waiters.remove(waiter)
        if self.error:
            raise OSError(f"capture writer failed: {self.error}")

    def request_close(self, discard=False):
        with self.condition:
            self.closing = True
            if discard:
                retained = deque()
                while self.items:
                    item = self.items.popleft()
                    if item[0] == "rx":
                        self.pending_bytes -= item[3]
                        self.raw_items -= 1
                        self.unfinished -= 1
                        self.dropped_chunks += 1
                        self.dropped_bytes += len(item[2][0])
                    else:
                        retained.append(item)
                self.items = retained
                self.raw_times = deque([self.inflight_t]) if self.inflight_t is not None else deque()
                self.oldest_t = self.raw_times[0] if self.raw_times else None
            self.condition.notify_all()

    async def wait_closed(self):
        if self.thread is None:
            return
        # Cancelling an asyncio waiter cannot terminate an in-flight filesystem write.
        await asyncio.to_thread(self.thread.join)
        if self.error:
            raise OSError(f"capture writer failed: {self.error}")

    def _notify_fault(self, code, message):
        with self.condition:
            first = not self.events[code]
            self.events[code] += 1
        if first:
            self.recorder.record("event", code=code, message=message, incomplete=True)
            self.loop.call_soon_threadsafe(self.fault_callback, code, message)

    def _receive(self, timestamp, chunk, mode, expected_filter):
        self.recorder.record("rx", t=timestamp, hex=chunk.hex().upper())
        if mode not in ("monitoring", "stopping"):
            return
        for kind, text in self.framer.feed(chunk):
            if kind == "prompt":
                continue
            upper = text.upper()
            if upper == "ATMA" or (upper == "STOPPED" and mode == "stopping"):
                continue
            if any(mark in upper for mark in (
                    "ERROR", "BUFFER FULL", "BUS BUSY", "BUS INIT", "UNABLE",
                    "NO DATA", "STOPPED", "?", "SEARCHING", "LV RESET")):
                self._notify_fault("adapter_error", text)
                continue
            try:
                identifier, data = self.parser(text, self.protocol)
            except ValueError as exc:
                self._notify_fault("malformed_frame", f"{text}: {exc}")
                continue
            if expected_filter is not None:
                value, mask = expected_filter
                if identifier & mask != value & mask:
                    self._notify_fault("filter_mismatch", f"received {identifier:X} outside requested hardware filter")
            with self.condition:
                self.frames += 1
                self.ids[identifier] += 1
                seq = self.frames
            self.recorder.record("frame", t=timestamp, id=identifier,
                                 dlc=len(data), data=data.hex().upper(), seq=seq)

    def _wake(self, waiter):
        if not waiter.done():
            waiter.set_result(None)

    def _run(self):
        try:
            while True:
                with self.condition:
                    while not self.items and not self.closing:
                        self.condition.wait()
                    if not self.items:
                        break
                    kind, timestamp, value, cost = self.items.popleft()
                    self.inflight_t = timestamp if kind == "rx" else None
                try:
                    if kind == "rx":
                        try:
                            self._receive(timestamp, *value)
                        except ValueError as exc:
                            self._notify_fault("framing_error", str(exc))
                    elif kind == "record":
                        record_type, fields = value
                        self.recorder.record(record_type, t=timestamp, **fields)
                    else:
                        self.recorder.flush()
                finally:
                    with self.condition:
                        if kind == "rx":
                            self.pending_bytes -= cost
                            self.raw_items -= 1
                            self.processed_bytes += len(value[0])
                            self.raw_times.popleft()
                        else:
                            self.controls -= 1
                        self.inflight_t = None
                        self.oldest_t = self.raw_times[0] if self.raw_times else None
                        self.unfinished -= 1
                        if not self.unfinished:
                            for waiter in self.waiters:
                                self.loop.call_soon_threadsafe(self._wake, waiter)
            self.recorder.flush()
        except Exception as exc:
            with self.condition:
                self.error = exc
                for waiter in self.waiters:
                    self.loop.call_soon_threadsafe(self._wake, waiter)
            self.loop.call_soon_threadsafe(self.fault_callback, "writer_failure", str(exc))
        finally:
            self.loop.call_soon_threadsafe(self._wake, self.done)
