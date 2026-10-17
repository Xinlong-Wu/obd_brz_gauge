"""Synthetic-only offline fixtures for the generic CAN JSONL analyzer.

Run only this suite (no discovery of hardware tools)::

    python3 -B -m unittest discover -s tests -p 'test_can_discovery_analysis.py'

No repository/personal captures, BLE, network or third-party libraries are used.
"""

import csv
import io
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import can_discovery_analysis as analysis


def frame(timestamp, payload=b"\x00", can_id=0x123):
    return {"type": "frame", "t": timestamp, "id": can_id,
            "data": payload.hex().upper(), "dlc": len(payload)}


def marker(timestamp, label):
    return {"type": "marker", "t": timestamp, "label": label}


def window(timestamp, label, payloads, can_id=0x123):
    """用固定正时长容纳任意样本数，避免把帧率误当作 epoch 权重。"""
    payloads = list(payloads)
    return [marker(timestamp, label)] + [
        frame(timestamp + 1 + 7 * (index + 1) / (len(payloads) + 1), payload, can_id)
        for index, payload in enumerate(payloads)
    ]


class AnalysisFixture(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.serial = 0

    def records(self, body=None, protocol="6", start=0.0, end=40.0):
        body = list(body) if body is not None else [frame(2)]
        return [
            {"type": "session", "schema": "elm327_can_discover", "version": 1,
             "protocol": protocol},
            {"type": "monitor_start", "t": start},
            *body,
            {"type": "monitor_end", "t": end},
            {"type": "summary", "t": end + 1, "complete": True,
             "frames": sum(record.get("type") == "frame" for record in body),
             "stop_confirmed": True},
        ]

    def write_records(self, records, name=None):
        self.serial += 1
        path = self.root / (name or f"capture_{self.serial:03d}.jsonl")
        path.write_text("".join(json.dumps(record) + "\n" for record in records),
                        encoding="utf-8")
        return path

    def capture(self, body=None, **kwargs):
        return self.write_records(self.records(body, **kwargs))

    def repeated(self, payload_a=b"\x00", payload_b=b"\x01", count=6):
        return sum((window(index * 10, label, [payload] * count)
                    for index, (label, payload) in enumerate([
                        ("A", payload_a), ("B", payload_b),
                        ("A", payload_a), ("B", payload_b)])), [])

    def candidate(self, rows, can_id="123", byte=0, bit=0, dlc=1):
        return next(row for row in rows if (row["id"], row["dlc"], row["byte"], row["bit"])
                    == (can_id, dlc, byte, bit))

    def assert_scalar_rows(self, rows, fields):
        for row in rows:
            self.assertEqual(list(row), fields)
            for value in row.values():
                self.assertIn(type(value), (str, int, float, bool, type(None)))
            json.dumps(row, allow_nan=False)
        output = io.StringIO()
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)
        self.assertEqual(len(list(csv.DictReader(io.StringIO(output.getvalue())))), len(rows))


class SummaryTests(AnalysisFixture):
    def test_generic_summary_splits_dlc_and_uses_host_monitor_duration(self):
        path = self.capture([
            frame(1, b"\x00\xA0"), marker(3, "B"), frame(2, b"\x01\xA0"),
            marker(0, "A"), frame(4, b"\x01\xA2"), frame(5, b"\x55"),
            frame(6, b"", 0x7FF),
        ], end=10)
        rows, warnings = analysis.summarize_files([path])
        self.assertEqual([(row["id"], row["dlc"]) for row in rows],
                         [("123", 1), ("123", 2), ("7FF", 0)])
        row = rows[1]
        self.assertEqual(row["frames"], 3)
        self.assertEqual(row["monitor_seconds"], 10)
        self.assertAlmostEqual(row["host_receive_hz"], 0.3)
        self.assertEqual(row["unique_payloads"], 3)
        self.assertFalse(row["unique_payloads_capped"])
        self.assertEqual(row["varying_bytes"], "0,1")
        self.assertEqual(row["varying_mask"], "0102")
        self.assertEqual(row["first_payload"], "00A0")
        self.assertEqual(row["last_payload"], "01A2")
        self.assertEqual(json.loads(row["label_counts"]), {"A": 2, "B": 1})
        self.assertEqual(row["unlabelled_frames"], 0)
        self.assertEqual(rows[2]["first_payload"], "")
        self.assertIn("not the CAN wire", " ".join(warnings))
        self.assert_scalar_rows(rows, analysis.SUMMARY_FIELDS)

    def test_rate_sums_all_file_coverage_even_when_id_is_absent(self):
        first = self.write_records(self.records([frame(2), frame(3)]), "a.jsonl")
        second = self.write_records(self.records([frame(11, b"\xFF", 0x200)],
                                                 start=10, end=70), "z.jsonl")
        rows, _ = analysis.summarize_files([second, first])
        self.assertEqual(rows[0]["monitor_seconds"], 100)
        self.assertEqual(rows[0]["host_receive_hz"], 0.02)
        self.assertEqual(rows[0]["files_seen"], 1)
        self.assertEqual(rows[0]["files_total"], 2)
        self.assertEqual(analysis.summarize_files([first, second])[0], rows)

    def test_timestamp_first_last_and_unlabelled_vs_real_empty_sentinel(self):
        path = self.capture([frame(8, b"\x08"), frame(1, b"\x01"),
                             marker(2, "unlabelled"), frame(3, b"\x03")])
        rows, _ = analysis.summarize_files([path])
        self.assertEqual(rows[0]["first_payload"], "01")
        self.assertEqual(rows[0]["last_payload"], "08")
        self.assertEqual(rows[0]["unlabelled_frames"], 1)
        self.assertEqual(json.loads(rows[0]["label_counts"]), {"unlabelled": 2})

    def test_11_and_29_bit_identifiers_remain_distinct(self):
        short = self.capture([frame(2)], protocol="6")
        extended = self.capture([frame(2), frame(3, b"\xFF", 0x1FFFFFFF)], protocol="7")
        self.assertEqual(analysis.summarize_files([short])[0][0]["id"], "123")
        self.assertEqual([row["id"] for row in analysis.summarize_files([extended])[0]],
                         ["00000123", "1FFFFFFF"])
        with self.assertRaisesRegex(ValueError, "protocol/filter.*separately"):
            analysis.summarize_files([short, extended])
        for protocol in ("8", "9", "A"):
            with self.subTest(protocol=protocol):
                row = analysis.summarize_files([self.capture(protocol=protocol)])[0][0]
                self.assertEqual(len(row["id"]), 3 if protocol == "8" else 8)

    def test_long_counter_unique_payload_memory_is_capped(self):
        count = analysis.UNIQUE_PAYLOAD_LIMIT + 2000
        path = self.capture([frame(1 + index / count, index.to_bytes(4, "big"))
                             for index in range(count)])
        row = analysis.summarize_files([path])[0][0]
        self.assertEqual(row["frames"], count)
        self.assertEqual(row["unique_payloads"], analysis.UNIQUE_PAYLOAD_LIMIT)
        self.assertTrue(row["unique_payloads_capped"])
        self.assertEqual(row["last_payload"], (count - 1).to_bytes(4, "big").hex().upper())

    def test_unique_cap_boundary_and_duplicates_are_exact(self):
        with mock.patch.object(analysis, "UNIQUE_PAYLOAD_LIMIT", 2):
            path = self.capture([frame(1, b"\x00"), frame(2, b"\x01"), frame(3, b"\x00")])
            row = analysis.summarize_files([path])[0][0]
            self.assertEqual(row["unique_payloads"], 2)
            self.assertFalse(row["unique_payloads_capped"])

    def test_metadata_types_are_ignored_but_timestamps_checked(self):
        extras = [{"type": kind, "t": 0.1, "arbitrary_metadata": {"hex": "FFFF"}}
                  for kind in ("rx", "command", "command_response", "transport")]
        path = self.capture(extras + [frame(2)])
        self.assertEqual(analysis.summarize_files([path])[0][0]["frames"], 1)

    def test_empty_capture_and_no_input_raise(self):
        path = self.capture([])
        with self.assertRaisesRegex(ValueError, "no valid CAN frames"):
            analysis.summarize_files([path])
        for paths in ([], None, "capture.jsonl", [path, path], [self.root / "absent"]):
            with self.subTest(paths=paths), self.assertRaises(ValueError):
                analysis.summarize_files(paths)

    def test_incompatible_filters_are_rejected_in_both_apis(self):
        records = self.records(self.repeated())
        records[0]["filters"] = ["ATCRA123"]
        first = self.write_records(records)
        records[0]["filters"] = ["ATCF000", "ATCM700"]
        second = self.write_records(records)
        for call in (lambda: analysis.summarize_files([first, second]),
                     lambda: analysis.analyze_files([first, second], "A", "B")):
            with self.assertRaisesRegex(ValueError, "protocol/filter.*separately"):
                call()
        records[0]["filters"] = [" atcra 123 "]
        equivalent = self.write_records(records)
        self.assertEqual(analysis.summarize_files([first, equivalent])[0][0]["frames"], 48)

    def test_two_passes_detect_file_changed_without_returning_mixed_results(self):
        path = self.capture()
        original = analysis._records
        reads = 0

        def mutate_on_second_read(*args, **kwargs):
            nonlocal reads
            reads += 1
            if reads == 2:
                with path.open("a", encoding="utf-8") as target:
                    target.write("\n")
            yield from original(*args, **kwargs)

        with mock.patch.object(analysis, "_records", side_effect=mutate_on_second_read):
            with self.assertRaisesRegex(ValueError, "changed between analysis passes"):
                analysis.summarize_files([path])
        self.assertEqual(reads, 2)


class ValidationTests(AnalysisFixture):
    def test_header_is_required_and_schema_version_protocol_are_strict(self):
        records = self.records()
        for update in ({"schema": "old"}, {"version": 2}, {"version": True},
                       {"version": "1"}, {"protocol": 6}, {"protocol": "5"},
                       {"protocol": "a"}, {"protocol": []}, {"t": math.inf}):
            for allow in (False, True):
                with self.subTest(update=update, allow=allow):
                    altered = [{**records[0], **update}, *records[1:]]
                    with self.assertRaises(ValueError):
                        analysis.summarize_files([self.write_records(altered)], allow)
        for bad_records in (records[1:], [], [records[0], records[0], *records[1:]]):
            with self.subTest(records=bad_records), self.assertRaises(ValueError):
                analysis.summarize_files([self.write_records(bad_records)], True)
        legacy = self.root / "legacy.log"
        legacy.write_text("123 00 01 02\n", encoding="utf-8")
        with self.assertRaises(ValueError):
            analysis.summarize_files([legacy])

    def test_invalid_frame_types_ranges_and_payloads(self):
        bad_fields = [
            {"id": True}, {"id": "123"}, {"id": 1.0}, {"id": -1}, {"id": 0x800},
            {"dlc": True}, {"dlc": "1"}, {"dlc": -1}, {"dlc": 9},
            {"data": "aa"}, {"data": "0G"}, {"data": "0"}, {"data": "00 01"},
            {"data": "00\n"}, {"data": "0000"}, {"data": None}, {"data": []},
            {"t": True}, {"t": "2"}, {"t": -1}, {"t": None},
            {"t": float("nan")}, {"t": math.inf}, {"t": -math.inf}, {"t": 10**400},
        ]
        for fields in bad_fields:
            with self.subTest(fields=fields):
                path = self.capture([{**frame(1), **fields}, frame(2)])
                with self.assertRaises(ValueError):
                    analysis.summarize_files([path])
                rows, warnings = analysis.summarize_files([path], True)
                self.assertEqual(rows[0]["frames"], 1)
                self.assertIn("skipped", " ".join(warnings))
        path = self.capture([frame(1, can_id=0x20000000), frame(2)], protocol="7")
        with self.assertRaisesRegex(ValueError, "frame.id"):
            analysis.summarize_files([path])

    def test_invalid_record_types_and_required_fields(self):
        records = [
            {"type": "marker", "t": 1, "label": ""},
            {"type": "marker", "t": 1, "label": 0},
            {"type": "marker", "t": -1, "label": "A"},
            {"type": "event", "t": 1, "code": "error", "message": "x", "incomplete": 1},
            {"type": "event", "t": 1, "code": "error", "message": []},
            {"type": "event", "t": 1, "code": 1, "message": "x", "incomplete": False},
            {"type": "rx", "t": math.inf},
            {"type": "command_response", "t": False},
            {"type": "transport", "t": "1"},
            {"type": "unexpected", "t": 1},
            {"t": 1},
        ]
        for record in records:
            with self.subTest(record=record):
                path = self.capture([record, frame(2)])
                with self.assertRaises(ValueError):
                    analysis.summarize_files([path])
                self.assertIn("skipped", " ".join(analysis.summarize_files([path], True)[1]))

    def test_summary_types_completion_counts_and_timestamp(self):
        for update in ({"complete": False}, {"stop_confirmed": False}, {"frames": 2},
                       {"frames": True}, {"frames": -1}, {"frames": "1"},
                       {"complete": 1}, {"stop_confirmed": "yes"}, {"t": 1},
                       {"t": math.nan}):
            with self.subTest(update=update):
                records = self.records()
                records[-1].update(update)
                path = self.write_records(records)
                with self.assertRaises(ValueError):
                    analysis.summarize_files([path])
                rows, warnings = analysis.summarize_files([path], True)
                self.assertEqual(rows[0]["frames"], 1)
                self.assertIn("allow_incomplete", " ".join(warnings))

    def test_missing_summary_is_explicit_opt_in_in_both_apis(self):
        path = self.write_records(self.records(self.repeated())[:-1])
        for call in (lambda allow: analysis.summarize_files([path], allow),
                     lambda allow: analysis.analyze_files([path], "A", "B", allow_incomplete=allow)):
            with self.assertRaisesRegex(ValueError, "missing summary"):
                call(False)
            rows, warnings = call(True)
            self.assertTrue(rows)
            self.assertIn("missing summary", " ".join(warnings))

    def test_incomplete_events_are_not_overridden_by_complete_summary(self):
        event = {"type": "event", "t": 4, "code": "overflow", "message": "dropped input",
                 "incomplete": True}
        path = self.capture(self.repeated() + [event])
        with self.assertRaisesRegex(ValueError, "incomplete event overflow"):
            analysis.analyze_files([path], "A", "B")
        rows, warnings = analysis.analyze_files([path], "A", "B", allow_incomplete=True)
        self.assertTrue(rows)
        self.assertIn("dropped input", " ".join(warnings))
        event["incomplete"] = False
        path = self.capture([event, frame(2)])
        self.assertEqual(len(analysis.summarize_files([path])[1]), 1)

    def test_malformed_json_utf8_duplicate_keys_and_truncation_are_never_silent(self):
        bad_lines = ['{"type":"frame",', '{"type":"rx","t":NaN}',
                     '{"type":"rx","type":"frame","t":1}', '[]', '\xff']
        for bad in bad_lines:
            with self.subTest(bad=bad):
                path = self.capture()
                with path.open("ab") as output:
                    output.write(bad.encode("latin-1") + b"\n")
                with self.assertRaisesRegex(ValueError, "malformed"):
                    analysis.summarize_files([path])
                rows, warnings = analysis.summarize_files([path], True)
                self.assertEqual(rows[0]["frames"], 1)
                self.assertIn("malformed", " ".join(warnings))
                self.assertIn("skipped", " ".join(warnings))

    def test_monitor_duration_and_outside_frames(self):
        for start, end in ((1, 1), (2, 1), (0, math.inf), (True, 40), (-1, 40)):
            with self.subTest(start=start, end=end), self.assertRaises(ValueError):
                analysis.summarize_files([self.capture(start=start, end=end)])
        path = self.capture([frame(1), frame(5), frame(11)], start=2, end=10)
        with self.assertRaisesRegex(ValueError, "outside monitor coverage"):
            analysis.summarize_files([path])
        rows, warnings = analysis.summarize_files([path], True)
        self.assertEqual(rows[0]["frames"], 1)
        self.assertEqual(rows[0]["monitor_seconds"], 8)
        self.assertIn("outside monitor coverage", " ".join(warnings))

    def test_missing_end_can_recover_but_missing_start_cannot(self):
        records = self.records([frame(1), frame(3)])
        path = self.write_records([record for record in records if record["type"] != "monitor_end"])
        with self.assertRaisesRegex(ValueError, "missing monitor_end"):
            analysis.summarize_files([path])
        rows, warnings = analysis.summarize_files([path], True)
        self.assertEqual(rows[0]["monitor_seconds"], 3)
        self.assertIn("conservative end", " ".join(warnings))
        path = self.write_records([record for record in records if record["type"] != "monitor_start"])
        with self.assertRaisesRegex(ValueError, "missing monitor_start"):
            analysis.summarize_files([path], True)

    def test_duplicate_boundaries_and_ambiguous_marker_times_rejected(self):
        bodies = [
            [marker(1, "A"), marker(1, "B"), frame(2)],
            [{"type": "monitor_start", "t": 1}, frame(2)],
            [{"type": "monitor_end", "t": 3}, frame(2)],
        ]
        for body in bodies:
            with self.subTest(body=body), self.assertRaises(ValueError):
                analysis.summarize_files([self.capture(body)], True)
        records = self.records()
        records.append(records[-1])
        with self.assertRaisesRegex(ValueError, "duplicate summary"):
            analysis.summarize_files([self.write_records(records)], True)

    def test_nonfinite_rate_and_summed_duration_raise_instead_of_outputting_infinity(self):
        path = self.capture([frame(0), frame(0)], end=5e-324)
        with self.assertRaisesRegex(ValueError, "not finite"):
            analysis.summarize_files([path])
        paths = [self.capture(end=1e308) for _ in range(2)]
        with self.assertRaisesRegex(ValueError, "duration is not finite"):
            analysis.summarize_files(paths)


class BitAnalysisTests(AnalysisFixture):
    def test_repeated_stable_complementary_bits_and_csv_contract(self):
        path = self.capture(self.repeated(b"\x01\x00", b"\x02\x80"))
        rows, warnings = analysis.analyze_files([path], "A", "B")
        self.assertEqual([(row["byte"], row["bit"]) for row in rows], [(0, 0), (0, 1), (1, 7)])
        negative = self.candidate(rows, bit=0, dlc=2)
        positive = self.candidate(rows, bit=1, dlc=2)
        self.assertEqual(negative["delta"], -1)
        self.assertEqual(positive["delta"], 1)
        self.assertEqual(positive["score"], 100)
        self.assertEqual(positive["mask"], "02")
        self.assertEqual(positive["epochs_a"], 2)
        self.assertEqual(positive["epochs_b"], 2)
        self.assertEqual(positive["samples_a"], 12)
        self.assertEqual(positive["consistency_a"], 1)
        self.assertEqual(positive["repeatability_a"], 1)
        self.assertEqual(positive["transitions_a"], 0)
        self.assertEqual(positive["coverage_a"], 1)
        self.assertEqual(positive["flags"], "unverified_semantics")
        self.assertIn("bit0 is LSB", " ".join(warnings))
        self.assertIn("not verified", " ".join(warnings))
        self.assert_scalar_rows(rows, analysis.CANDIDATE_FIELDS)
        swapped = analysis.analyze_files([path], "B", "A")[0]
        self.assertEqual(swapped[0]["delta"], 1)

    def test_markers_and_delayed_frame_records_use_timestamps_not_file_order(self):
        body = self.repeated()
        markers = [record for record in body if record["type"] == "marker"]
        frames = [record for record in body if record["type"] == "frame"]
        reference = self.capture(body)
        reordered = self.capture(list(reversed(markers)) + frames)
        self.assertEqual(analysis.analyze_files([reference], "A", "B")[0],
                         analysis.analyze_files([reordered], "A", "B")[0])
        self.assertEqual(analysis.summarize_files([reordered])[0][0]["label_counts"],
                         analysis.summarize_files([reference])[0][0]["label_counts"])

    def test_equal_epoch_weight_despite_unequal_frame_rates(self):
        body = (window(0, "A", [b"\x00"] * 1000)
                + window(10, "B", [b"\x01"] * 7)
                + window(20, "A", [b"\x01"] * 5)
                + window(30, "B", [b"\x01"] * 30))
        row = analysis.analyze_files([self.capture(body)], "A", "B")[0][0]
        self.assertEqual(row["occupancy_a"], 0.5)
        self.assertEqual(row["occupancy_b"], 1)
        self.assertEqual(row["delta"], 0.5)
        self.assertEqual(row["samples_a"], 1005)
        self.assertEqual(row["repeatability_a"], 0)
        self.assertEqual(row["score"], 25)

    def test_flashing_and_volatile_counter_bits_rank_below_stable_signal(self):
        body = []
        for index, label in enumerate(("A", "B", "A", "B")):
            state = label == "B"
            payloads = [bytes([int(state), int(state and sample % 2 == 0), sample % 256])
                        for sample in range(600)]
            body += window(index * 10, label, payloads)
        rows, _ = analysis.analyze_files([self.capture(body)], "A", "B")
        stable = self.candidate(rows, byte=0, dlc=3)
        flashing = self.candidate(rows, byte=1, dlc=3)
        self.assertEqual(stable["score"], 100)
        self.assertEqual(flashing["delta"], 0.5)
        self.assertGreater(flashing["score"], 0)
        self.assertLess(flashing["score"], stable["score"])
        self.assertEqual(flashing["consistency_b"], 0.5)
        self.assertEqual(flashing["transition_rate_b"], 1)
        self.assertEqual(flashing["transitions_b"], 2 * 599)
        self.assertIn("within_state_variation", flashing["flags"])
        self.assertFalse(any(row["byte"] == 2 for row in rows))
        self.assertNotIn("counter", flashing["flags"])

    def test_counter_like_differences_are_visible_but_not_classified(self):
        body = []
        for index, label in enumerate(("A", "B", "A", "B")):
            values = range(20) if label == "A" else range(10, 30)
            body += window(index * 10, label, [bytes([value]) for value in values])
        rows, _ = analysis.analyze_files([self.capture(body)], "A", "B")
        self.assertTrue(rows)
        self.assertTrue(all(row["score"] < 100 for row in rows))
        self.assertTrue(any("within_state_variation" in row["flags"] for row in rows))
        self.assertFalse(any("counter" in row["flags"] or "checksum" in row["flags"] for row in rows))

    def test_settle_boundary_and_other_labels_end_epochs(self):
        body = (window(0, "A", [b"\x00"] * 6)
                + window(10, "B", [b"\x01"] * 6)
                + window(20, "other", [b"\xFF"] * 100)
                + window(30, "A", [b"\x00"] * 6)
                + window(40, "B", [b"\x01"] * 6))
        body += [frame(0.5, b"\x01"), frame(10.5, b"\x00"), frame(10, b"\x00"),
                 frame(1, b"\x00"), frame(11, b"\x01")]
        rows, _ = analysis.analyze_files([self.capture(body, end=50)], "A", "B")
        row = rows[0]
        self.assertEqual(row["delta"], 1)
        self.assertEqual(row["samples_a"], 13)
        self.assertEqual(row["samples_b"], 13)
        self.assertEqual(row["expected_epochs_a"], 2)
        self.assertEqual(row["expected_epochs_b"], 2)
        self.assertEqual(row["epochs_a"], 2)

    def test_missing_and_undersampled_epochs_report_coverage_not_zero(self):
        body = self.repeated()
        for timestamp, count, value in ((0, 6, 0), (10, 6, 1), (30, 2, 1)):
            body += [frame(timestamp + 2 + sample * 0.1, bytes([value]), 0x222)
                     for sample in range(count)]
        rows, warnings = analysis.analyze_files([self.capture(body)], "A", "B")
        row = self.candidate(rows, "222")
        self.assertEqual(row["occupancy_a"], 0)
        self.assertEqual(row["occupancy_b"], 1)
        self.assertEqual(row["delta"], 1)
        self.assertEqual(row["epochs_a"], 1)
        self.assertEqual(row["missing_epochs_a"], 1)
        self.assertEqual(row["undersampled_epochs_b"], 1)
        self.assertEqual(row["missing_epochs_b"], 0)
        self.assertEqual(row["observed_samples_b"], 8)
        self.assertEqual(row["samples_b"], 6)
        self.assertEqual(row["coverage_a"], 0.5)
        self.assertEqual(row["coverage_b"], 0.5)
        self.assertIn("insufficient_coverage", row["flags"])
        self.assertIn("limited_repeatability", row["flags"])
        self.assertIn("insufficient_coverage", " ".join(warnings))
        self.assertIsNone(row["repeatability_a"])

    def test_id_absent_in_one_state_has_null_metrics_and_zero_score(self):
        body = self.repeated()
        body += [frame(2 + sample * 0.1, b"\x01", 0x333) for sample in range(6)]
        rows, _ = analysis.analyze_files([self.capture(body)], "A", "B")
        row = self.candidate(rows, "333")
        self.assertEqual(row["occupancy_a"], 1)
        self.assertIsNone(row["occupancy_b"])
        self.assertIsNone(row["delta"])
        self.assertIsNone(row["abs_delta"])
        self.assertIsNone(row["consistency_b"])
        self.assertIsNone(row["transition_rate_b"])
        self.assertIsNone(row["transitions_b"])
        self.assertEqual(row["samples_b"], 0)
        self.assertEqual(row["missing_epochs_b"], 2)
        self.assertEqual(row["score"], 0)
        self.assert_scalar_rows(rows, analysis.CANDIDATE_FIELDS)

    def test_same_id_with_split_lengths_does_not_mix_samples_or_pad_bytes(self):
        body = self.repeated()
        body += [frame(2 + sample * 0.1, b"\x01\xFF") for sample in range(6)]
        rows, _ = analysis.analyze_files([self.capture(body)], "A", "B")
        row = self.candidate(rows, byte=1, dlc=2)
        self.assertIsNone(row["occupancy_b"])
        self.assertEqual(row["missing_epochs_b"], 2)
        self.assertEqual(self.candidate(rows)["delta"], 1)

    def test_single_epochs_have_limited_repeatability_even_with_many_samples(self):
        body = window(0, "A", [b"\x00"] * 300) + window(10, "B", [b"\x01"] * 300)
        row = analysis.analyze_files([self.capture(body, end=20)], "A", "B")[0][0]
        self.assertEqual(row["score"], 50)
        self.assertEqual(row["epochs_a"], 1)
        self.assertIsNone(row["repeatability_a"])
        self.assertIn("limited_repeatability", row["flags"])

    def test_multiple_files_have_distinct_clocks_and_epoch_evidence(self):
        paths = [self.capture(window(0, "A", [b"\x00"] * 6), end=10),
                 self.capture(window(0, "B", [b"\x01"] * 6), end=10),
                 self.capture(window(0, "A", [b"\x00"] * 60), end=10),
                 self.capture(window(0, "B", [b"\x01"] * 60), end=10)]
        rows, _ = analysis.analyze_files(paths, "A", "B")
        self.assertEqual(rows[0]["epochs_a"], 2)
        self.assertEqual(rows[0]["epochs_b"], 2)
        self.assertEqual(rows[0]["score"], 100)
        self.assertEqual(rows, analysis.analyze_files(list(reversed(paths)), "A", "B")[0])

    def test_order_backwards_does_not_invent_transition_counts(self):
        body = self.repeated()
        body[2], body[3] = body[3], body[2]
        rows, warnings = analysis.analyze_files([self.capture(body)], "A", "B")
        self.assertEqual(rows[0]["delta"], 1)
        self.assertIsNone(rows[0]["transitions_a"])
        self.assertIsNone(rows[0]["transition_rate_a"])
        self.assertEqual(rows[0]["transitions_b"], 0)
        self.assertIn("unordered_frames", rows[0]["flags"])
        self.assertIn("unordered_frames", " ".join(warnings))

    def test_no_difference_returns_no_candidates_cleanly(self):
        rows, warnings = analysis.analyze_files([self.capture(self.repeated(b"\x00", b"\x00"))],
                                               "A", "B")
        self.assertEqual(rows, [])
        self.assertIn("No candidates", " ".join(warnings))

    def test_missing_state_id_or_minimum_samples_raise_no_evidence(self):
        bodies = [[], [frame(1)], window(0, "A", [b"\x00"] * 6),
                  window(0, "A", [b"\x00"] * 6)
                  + window(10, "B", [b"\x01"] * 6, can_id=0x456),
                  window(0, "A", [b"\x00"] * 4) + window(10, "B", [b"\x01"] * 6),
                  self.repeated(b"", b"")]
        for body in bodies:
            with self.subTest(body=body), self.assertRaises(ValueError):
                analysis.analyze_files([self.capture(body)], "A", "B")
        with self.assertRaisesRegex(ValueError, "post-settle"):
            analysis.analyze_files([self.capture(self.repeated())], "A", "B", settle=100)

    def test_public_argument_validation(self):
        path = self.capture(self.repeated())
        for kwargs in ({"settle": -1}, {"settle": math.nan}, {"settle": math.inf},
                       {"settle": "1"}, {"settle": True}, {"min_samples": 0},
                       {"min_samples": 1.5}, {"min_samples": True},
                       {"allow_incomplete": 1}):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                analysis.analyze_files([path], "A", "B", **kwargs)
        for a, b in (("A", "A"), ("", "B"), (None, "B"), ("A", []), (" ", "B")):
            with self.subTest(a=a, b=b), self.assertRaises(ValueError):
                analysis.analyze_files([path], a, b)
        with self.assertRaises(ValueError):
            analysis.summarize_files([path], allow_incomplete=1)

    def test_one_sample_epochs_have_undefined_transition_rate(self):
        path = self.capture(window(0, "A", [b"\x00"]) + window(10, "B", [b"\x01"]))
        row = analysis.analyze_files([path], "A", "B", min_samples=1)[0][0]
        self.assertIsNone(row["transition_rate_a"])
        self.assertEqual(row["transitions_a"], 0)
        self.assertEqual(row["delta"], 1)

    def test_deterministic_score_tie_breaking_numeric_id_then_byte_bit(self):
        body = self.repeated()
        for timestamp, label in ((0, "A"), (10, "B"), (20, "A"), (30, "B")):
            body += [frame(timestamp + 2 + sample * 0.1,
                           b"\x00\x00" if label == "A" else b"\x03\x01", 0x010)
                     for sample in range(6)]
        rows, _ = analysis.analyze_files([self.capture(body)], "A", "B")
        self.assertEqual([(row["id"], row["byte"], row["bit"]) for row in rows],
                         [("010", 0, 0), ("010", 0, 1), ("010", 1, 0), ("123", 0, 0)])

    def test_import_remains_offline_without_importing_collector_or_ble(self):
        code = '''
import importlib.abc
import sys
class BlockHardware(importlib.abc.MetaPathFinder):
    def find_spec(self, fullname, path=None, target=None):
        if fullname.split('.')[0] in {'bleak', 'elm327_can_discover', 'socket'}:
            raise AssertionError('hardware/network import: ' + fullname)
sys.meta_path.insert(0, BlockHardware())
sys.path.insert(0, sys.argv[1])
import can_discovery_analysis
assert can_discovery_analysis.SUMMARY_FIELDS
'''
        result = subprocess.run([sys.executable, "-B", "-c", code,
                                 str(Path(analysis.__file__).parent)],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
