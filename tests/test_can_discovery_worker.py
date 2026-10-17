"""Threaded CAN capture regressions; no BLE, network, or timing benchmarks."""

import asyncio
import contextlib
import threading
import unittest

from test_elm327_can_discover import FakeBleClient
import elm327_can_discover as discover
from can_capture_worker import CaptureWorker


class ControlledRecorder:
    """Block a raw write with explicit events, never by blocking the loop."""

    def __init__(self):
        self.rows = []
        self.calls = []
        self.clock = 0.0
        self.block_raw = False
        self.entered = threading.Event()
        self.release = threading.Event()
        self.fail_raw = False

    def now(self):
        return self.clock

    def record(self, kind, t=None, **fields):
        self.calls.append((kind, threading.get_ident()))
        if kind == "rx" and self.block_raw:
            self.entered.set()
            if not self.release.wait(5):
                raise RuntimeError("test did not release blocked recorder")
        if kind == "rx" and self.fail_raw:
            raise OSError("simulated disk write failure")
        self.rows.append({"type": kind, "t": self.now() if t is None else t, **fields})

    def flush(self):
        self.calls.append(("flush", threading.get_ident()))


class CaptureWorkerTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.workers = []
        self.sessions = []
        self.main_thread = threading.get_ident()

    async def asyncTearDown(self):
        # Always unblock before joining, including failed assertions. No writer
        # thread may survive its test or access a subsequently closed recorder.
        for worker in self.workers:
            worker.recorder.release.set()
            worker.request_close()
        for session in self.sessions:
            session.closing = True
            if session.consumer is not None:
                session.consumer.cancel()
                with contextlib.suppress(asyncio.CancelledError, OSError):
                    await session.consumer
        for worker in self.workers:
            with contextlib.suppress(OSError):
                await worker.wait_closed()
            self.assertFalse(worker.thread.is_alive())

    async def wait_entered(self, recorder):
        self.assertTrue(await asyncio.to_thread(recorder.entered.wait, 3),
                        "worker never reached controlled disk write")

    async def drain(self, worker):
        await asyncio.wait_for(worker.join(), 3)
        await asyncio.sleep(0)

    def make_worker(self, *, protocol="6", buffer_bytes=64 * 1024 * 1024):
        recorder = ControlledRecorder()
        faults = []
        worker = CaptureWorker(recorder, discover.LineFramer(), discover.parse_can_line,
                               protocol, lambda *fault: faults.append(fault), buffer_bytes)
        self.workers.append(worker)
        worker.start()
        return worker, recorder, faults

    async def make_session(self, *, buffer_bytes=64 * 1024 * 1024):
        recorder = ControlledRecorder()
        client = FakeBleClient(monitor_chunks=())
        session = discover.MonitorSession(client, client.write_char, recorder,
                                          buffer_bytes=buffer_bytes)
        self.workers.append(session.worker)
        self.sessions.append(session)
        await client.start_notify(client.notify_char, session.on_notify)
        session.consumer = asyncio.create_task(session.consume())
        await asyncio.sleep(0)
        await session.start_monitor("baseline")
        await self.drain(session.worker)
        return session, client, recorder

    async def test_ble_burst_and_heartbeat_continue_while_writer_is_blocked(self):
        session, client, recorder = await self.make_session()
        recorder.block_raw = True
        chunk = b"7E80102\r"
        session.on_notify(client.notify_char, chunk)
        await self.wait_entered(recorder)
        heartbeats = []

        async def heartbeat():
            for _ in range(32):
                heartbeats.append(True)
                await asyncio.sleep(0)

        async def burst():
            for _ in range(128):
                session.on_notify(client.notify_char, chunk)
                await asyncio.sleep(0)

        try:
            await asyncio.wait_for(asyncio.gather(heartbeat(), burst()), 3)
            self.assertEqual(len(heartbeats), 32)
            stats = session.worker.snapshot()
            self.assertEqual(stats["rx_chunks"], 129)
            self.assertEqual(stats["processed_bytes"], 0)
            self.assertEqual(stats["buffer_bytes"], 129 * (len(chunk) + 256))
            self.assertEqual(session.frames, 0)
            self.assertFalse(session.stop_event.is_set())
            # Metadata and parser output cannot run on the notification thread.
            self.assertTrue(all(thread != self.main_thread for _, thread in recorder.calls))
        finally:
            recorder.release.set()
        await self.drain(session.worker)
        self.assertEqual(session.frames, 129)
        await session.stop_monitor()
        self.assertTrue(await session.finish())
        self.assertEqual([row["type"] for row in recorder.rows][-2:], ["monitor_end", "summary"])
        self.assertEqual(sum(row["type"] == "summary" for row in recorder.rows), 1)

    async def test_stop_prompt_confirms_before_blocked_writer_drains(self):
        session, client, recorder = await self.make_session()
        recorder.block_raw = True
        session.on_notify(client.notify_char, b"7E801\r")
        await self.wait_entered(recorder)
        try:
            session.request_stop()
            await asyncio.wait_for(session.stop_monitor(), 3)
            self.assertTrue(session.stop_confirmed)
            self.assertFalse(session.monitor_active)
            self.assertEqual(client.writes, [b"ATMA\r", b"\r"])
            self.assertEqual(session.worker.snapshot()["processed_bytes"], 0)
            self.assertFalse(recorder.release.is_set())
            self.assertNotIn("monitor_end", [row["type"] for row in recorder.rows])
        finally:
            recorder.release.set()
        self.assertTrue(await session.finish())

    async def test_unexpected_prompt_latches_without_repeating_cr_behind_backlog(self):
        session, client, recorder = await self.make_session()
        recorder.block_raw = True
        session.on_notify(client.notify_char, b"7E801\r")
        await self.wait_entered(recorder)
        try:
            session.on_notify(client.notify_char, b">")
            self.assertTrue(session.stop_event.is_set())
            self.assertTrue(session.incomplete)
            self.assertTrue(session.stop_confirmed)
            await asyncio.wait_for(session.stop_monitor(), 3)
            self.assertEqual(client.writes, [b"ATMA\r"])
            self.assertEqual(session.worker.snapshot()["processed_bytes"], 0)
        finally:
            recorder.release.set()
        self.assertFalse(await session.finish())
        self.assertIn("unexpected_prompt", recorder.rows[-1]["events"])

    async def test_byte_budget_includes_inflight_and_highwater_is_exact(self):
        first, second = b"7E801\r", b"7E90203\r"
        budget = len(first) + len(second) + 2 * CaptureWorker.ITEM_OVERHEAD
        worker, recorder, faults = self.make_worker(buffer_bytes=budget)
        recorder.block_raw = True
        self.assertTrue(worker.receive(0.0, first, "monitoring", None))
        await self.wait_entered(recorder)
        try:
            self.assertTrue(worker.receive(0.1, second, "monitoring", None))
            self.assertFalse(worker.receive(0.2, b"X", "monitoring", None))
            stats = worker.snapshot()
            self.assertEqual(stats["buffer_bytes"], budget)
            self.assertEqual(stats["peak_buffer_bytes"], budget)
            self.assertEqual(stats["buffer_limit_bytes"], budget)
            self.assertEqual(stats["queue_drops"], 1)
            self.assertEqual(stats["dropped_bytes"], 1)
            self.assertEqual(stats["processed_bytes"], 0)
        finally:
            recorder.release.set()
        await self.drain(worker)
        self.assertFalse(worker.receive(0.3, b"YY", "monitoring", None))
        stats = worker.snapshot()
        self.assertEqual(stats["buffer_bytes"], 0)
        self.assertEqual(stats["peak_buffer_bytes"], budget)
        self.assertEqual(stats["processed_bytes"], len(first) + len(second))
        self.assertEqual(stats["rx_bytes"], len(first) + len(second) + 3)
        self.assertEqual((stats["rx_chunks"], stats["queue_drops"], stats["dropped_bytes"]), (4, 2, 3))
        self.assertEqual(faults, [])

    async def test_byte_overflow_is_incomplete_even_when_stop_prompt_arrives(self):
        chunk = b"7E801\r"
        session, client, recorder = await self.make_session(buffer_bytes=len(chunk) + 256)
        recorder.block_raw = True
        session.on_notify(client.notify_char, chunk)
        await self.wait_entered(recorder)
        try:
            session.on_notify(client.notify_char, b"7E902\r")
            self.assertTrue(session.incomplete)
            self.assertTrue(session.stop_event.is_set())
            await asyncio.wait_for(session.stop_monitor(), 3)
            self.assertTrue(session.stop_confirmed)
        finally:
            recorder.release.set()
        self.assertFalse(await session.finish())
        summary = recorder.rows[-1]
        self.assertFalse(summary["complete"])
        self.assertEqual(summary["frames"], 1)
        self.assertEqual(summary["queue_drops"], 4)
        self.assertEqual(summary["dropped_bytes"], len(b"7E902\rSTOPPED\r>"))
        self.assertIn("host_queue_overflow", summary["events"])

    async def test_marker_timestamp_and_fifo_survive_writer_backlog(self):
        worker, recorder, _ = self.make_worker()
        recorder.block_raw = True
        worker.receive(1.25, b"7E801\r", "monitoring", None)
        await self.wait_entered(recorder)
        try:
            recorder.clock = 2.5
            worker.record("marker", label="roof open")
            worker.receive(3.75, b"7E902\r", "monitoring", None)
            recorder.clock = 100.0
            self.assertEqual(worker.snapshot()["writer_lag_seconds"], 98.75)
        finally:
            recorder.release.set()
        await self.drain(worker)
        self.assertEqual([(row["type"], row["t"]) for row in recorder.rows],
                         [("rx", 1.25), ("frame", 1.25), ("marker", 2.5),
                          ("rx", 3.75), ("frame", 3.75)])
        self.assertEqual(worker.snapshot()["writer_lag_seconds"], 0)

    async def test_compact_11_and_29_bit_frames_keep_payload_and_sequence(self):
        for protocol, wire, expected in (
                ("6", b"ATMA\r7E8084100\r7FF\r", [(0x7E8, "084100"), (0x7FF, "")]),
                ("7", b"ATMA\r18DAF110084100\r1FFFFFFF\r", [(0x18DAF110, "084100"), (0x1FFFFFFF, "")])):
            with self.subTest(protocol=protocol):
                worker, recorder, faults = self.make_worker(protocol=protocol)
                for byte in wire:
                    self.assertTrue(worker.receive(1.0, bytes([byte]), "monitoring", None))
                await self.drain(worker)
                frames = [row for row in recorder.rows if row["type"] == "frame"]
                self.assertEqual([(row["id"], row["data"]) for row in frames], expected)
                self.assertEqual([row["seq"] for row in frames], [1, 2])
                self.assertEqual(faults, [])

    async def test_telemetry_is_nonfault_event_with_receive_and_processing_counts(self):
        session, client, recorder = await self.make_session()
        recorder.block_raw = True
        chunk = b"7E801\r"
        session.on_notify(client.notify_char, chunk)
        await self.wait_entered(recorder)
        try:
            recorder.clock = 2.0
            stats = session.telemetry()
            self.assertEqual(stats["rx_bytes"], len(chunk))
            self.assertEqual(stats["rx_chunks"], 1)
            self.assertEqual(stats["processed_bytes"], 0)
            self.assertEqual(stats["writer_lag_seconds"], 2.0)
            self.assertEqual(stats["rx_bytes_per_second"], len(chunk) / 2)
            self.assertFalse(session.incomplete)
        finally:
            recorder.release.set()
        await self.drain(session.worker)
        recorder.clock = 4.0
        stats = session.telemetry()
        self.assertEqual(stats["frames"], 1)
        self.assertEqual(stats["unique_ids"], 1)
        self.assertEqual(stats["buffer_bytes"], 0)
        self.assertEqual(stats["processed_bytes"], len(chunk))
        self.assertEqual(stats["processed_bytes_per_second"], len(chunk) / 2)
        await self.drain(session.worker)
        events = [row for row in recorder.rows if row.get("code") == "telemetry"]
        self.assertEqual(len(events), 2)
        self.assertTrue(all(row["type"] == "event" and row["incomplete"] is False for row in events))
        self.assertFalse(session.stop_event.is_set())

    async def test_worker_filter_and_adapter_errors_are_delivered_on_loop(self):
        worker, recorder, faults = self.make_worker()
        worker.receive(1.0, b"7E901\rBUFFER FULL\rBUFFER FULL\r", "monitoring", (0x7E8, 0x7FF))
        await self.drain(worker)
        self.assertEqual([code for code, _ in faults], ["filter_mismatch", "adapter_error"])
        self.assertEqual(worker.snapshot()["frames"], 1)
        events = [row for row in recorder.rows if row["type"] == "event"]
        self.assertEqual([row["code"] for row in events], ["filter_mismatch", "adapter_error"])
        self.assertTrue(all(row["incomplete"] for row in events))
        self.assertTrue(all(thread != self.main_thread for _, thread in recorder.calls))

    async def test_disk_failure_wakes_join_and_worker_really_exits(self):
        worker, recorder, faults = self.make_worker()
        recorder.block_raw = True
        recorder.fail_raw = True
        worker.receive(1.0, b"7E801\r", "monitoring", None)
        await self.wait_entered(recorder)
        waiter = asyncio.create_task(worker.join())
        try:
            await asyncio.sleep(0)
            self.assertFalse(waiter.done())
        finally:
            recorder.release.set()
        with self.assertRaisesRegex(OSError, "simulated disk write failure"):
            await asyncio.wait_for(waiter, 3)
        with self.assertRaisesRegex(OSError, "simulated disk write failure"):
            await worker.wait_closed()
        await asyncio.sleep(0)
        self.assertFalse(worker.thread.is_alive())
        self.assertEqual([code for code, _ in faults], ["writer_failure"])
        with self.assertRaises(OSError):
            worker.record("marker", label="after failure")

    async def test_consumer_cancellation_waits_for_actual_inflight_write(self):
        session, client, recorder = await self.make_session()
        recorder.block_raw = True
        session.on_notify(client.notify_char, b"7E801\r")
        await self.wait_entered(recorder)
        try:
            session.consumer.cancel()
            for _ in range(4):
                await asyncio.sleep(0)
            self.assertFalse(session.consumer.done())
            self.assertTrue(session.worker.thread.is_alive())
        finally:
            recorder.release.set()
        with self.assertRaises(asyncio.CancelledError):
            await asyncio.wait_for(session.consumer, 3)
        self.assertFalse(session.worker.thread.is_alive())


if __name__ == "__main__":
    unittest.main()
