"""通用 CAN JSONL 离线分析；只依赖标准库，不导入采集器或 BLE 库。

Public API::

    summarize_files(paths, allow_incomplete=False) -> (rows, warnings)
    analyze_files(paths, state_a, state_b, settle=1.0, min_samples=5,
                  allow_incomplete=False) -> (rows, warnings)

Only elm327_can_discover version 1 sessions are accepted. Invalid input or no
usable evidence raises ValueError. With allow_incomplete=True, damaged records
are explicitly reported and skipped, and incomplete captures are reported.
A valid session header and unambiguous monitor/marker boundaries remain required.
A missing monitor_end uses the last valid frame timestamp as a conservative end;
a missing monitor_start cannot be recovered. Duplicate JSON keys are corruption.

Two streaming passes read metadata/markers first, then aggregate frames; frames
are never retained. Each pass is hashed so a changing file cannot silently mix
two captures. Memory scales with IDs, marker epochs and a bounded payload set,
not the number of frames. Paths are deduplicated by rejection and sorted for
repeatable output. Each file has its own session-relative clock. Multiple files
must have the same session.protocol and session.filters command list (ignoring
command whitespace/case); summarize/analyze differing visibility groups
separately. Unspecified filters only match other unspecified filters.

Coordinates are byte0 = first payload byte, bit0 = least significant bit (LSB).
IDs are uppercase hex strings without 0x, padded to 3/8 digits for 11/29-bit CAN.
Protocol 6/8 uses 11-bit IDs; 7/9/A uses 29-bit IDs. DLC is classic CAN, 0..8.
No vehicle/signal maps or counter/checksum classifiers are used.

Summary columns:
  monitor_seconds sums valid monitor coverage over ALL input files, including
  files where an ID is absent; host_receive_hz is frames / monitor_seconds.
  This is an observed host receive rate, NOT a CAN wire rate. unique_payloads
  is exact until UNIQUE_PAYLOAD_LIMIT; when capped it is only a lower bound.
  varying_bytes is a comma-separated list of zero-based byte indices;
  varying_mask is compact hex, with 1 bits wherever payload bits varied.
  first/last_payload follow sorted file order and timestamps within each file.
  label_counts is a JSON object encoded as a CSV-safe string, before settling;
  frames before any marker are counted in unlabelled_frames, not a fake label.

Candidate columns:
  Epochs are [marker.t + settle, next_marker.t), clipped to monitor coverage.
  Other labels end an epoch normally but contribute no A/B samples. Epochs
  with no positive post-settle duration are excluded. At least min_samples for
  this ID AND DLC in this file/epoch is required. Every eligible epoch has equal
  weight, regardless of its frame rate; no cross-epoch transition is counted.
  occupancy is the mean of epoch bit occupancies; delta = B - A (signed).
  consistency is mean(max(p, 1-p)) within epochs, ranging from 0.5 to 1.
  repeatability is 1 - range(epoch occupancies), or None without >=2 epochs.
  transition_rate is the equally weighted mean of within-epoch changes/(n-1).
  Transitions follow timestamp order within a stream; if it runs backwards,
  transition metrics for that state are None, never a misleading count.
  samples counts eligible samples; observed_samples also includes short epochs.
  missing_epochs counts absent ID/DLC epochs; undersampled_epochs counts short
  nonempty epochs. Coverage is eligible / expected positive-duration epochs.
  Missing state occupancy/delta is None, never an invented zero. Such rows are
  coverage diagnostics; at least one ID/DLC must have eligible A AND B evidence.
  score is a transparent heuristic in [0,100]: abs(delta) times mean consistency,
  minimum state coverage, (0.5 + 0.5 * minimum repeatability), and a transition
  penalty (1 - 0.5 * mean transition_rate). Unknown repeatability uses zero;
  unknown transition rate has no penalty. It is NOT a confidence/probability.
  Stable differences rank above flashing differences, but flashing candidates
  remain visible. Zero-delta rows are omitted unless coverage is insufficient.
  All candidates have unverified_semantics; correlation does not identify a
  signal's meaning, and volatile bits need not be counters/checksums.

Every row has exactly the exported column list's keys and JSON-safe scalar
values. Nested label counts are encoded as JSON text for CSV writers.
"""

from bisect import bisect_right
from collections import Counter
from dataclasses import dataclass, field
import hashlib
import json
import math
from pathlib import Path
import re


SUMMARY_FIELDS = [
    "id", "dlc", "frames", "files_seen", "files_total", "monitor_seconds",
    "host_receive_hz", "unique_payloads", "unique_payloads_capped",
    "varying_bytes", "varying_mask", "first_payload", "last_payload",
    "label_counts", "unlabelled_frames",
]
CANDIDATE_FIELDS = [
    "id", "dlc", "byte", "bit", "mask", "state_a", "state_b",
    "occupancy_a", "occupancy_b", "delta", "abs_delta",
    "consistency_a", "consistency_b", "repeatability_a", "repeatability_b",
    "transition_rate_a", "transition_rate_b", "transitions_a", "transitions_b",
    "samples_a", "samples_b", "observed_samples_a", "observed_samples_b",
    "epochs_a", "epochs_b", "expected_epochs_a", "expected_epochs_b",
    "missing_epochs_a", "missing_epochs_b", "undersampled_epochs_a",
    "undersampled_epochs_b", "coverage_a", "coverage_b", "score", "flags",
]
UNIQUE_PAYLOAD_LIMIT = 1024
_PROTOCOL_BITS = {"6": 11, "7": 29, "8": 11, "9": 29, "A": 29}
_HEX_PAYLOAD = re.compile(r"(?:[0-9A-F]{2}){0,8}\Z")
_IGNORED_TYPES = {"rx", "command", "command_response", "transport"}
_RECORD_TYPES = {
    "session", "monitor_start", "marker", "frame", "event", "monitor_end",
    "summary",
} | _IGNORED_TYPES


def _number(value, name):
    if type(value) not in (int, float):
        raise ValueError(f"{name} must be a finite nonnegative number")
    try:
        result = float(value)
    except OverflowError:
        raise ValueError(f"{name} must be a finite nonnegative number") from None
    if not math.isfinite(result) or result < 0:
        raise ValueError(f"{name} must be a finite nonnegative number")
    return result


def _integer(value, name, low, high):
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f"{name} must be an integer in [{low}, {high}]")


def _text(value, name, nonempty=False):
    if not isinstance(value, str) or (nonempty and not value.strip()):
        raise ValueError(f"{name} must be a {'nonempty ' if nonempty else ''}string")


def _boolean(value, name):
    if type(value) is not bool:
        raise ValueError(f"{name} must be a boolean")


def _problem(warnings, allow_incomplete, message):
    if not allow_incomplete:
        raise ValueError(message)
    warnings.append(f"allow_incomplete: {message}")


def _json_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def _json_constant(value):
    raise ValueError(f"nonstandard JSON constant {value}")


def _session_bits(record):
    if record.get("type") != "session":
        raise ValueError("first record must be a session header (headerless logs unsupported)")
    if record.get("schema") != "elm327_can_discover":
        raise ValueError("session schema must be elm327_can_discover")
    if type(record.get("version")) is not int or record["version"] != 1:
        raise ValueError("unsupported session version (expected integer 1)")
    protocol = record.get("protocol")
    if not isinstance(protocol, str) or protocol not in _PROTOCOL_BITS:
        raise ValueError("unsupported CAN protocol (expected 6, 7, 8, 9 or A)")
    if "filters" in record:
        filters = record["filters"]
        if not isinstance(filters, list) or not all(isinstance(item, str) for item in filters):
            raise ValueError("session.filters must be a list of command strings")
    if "t" in record:
        _number(record["t"], "session.t")
    return _PROTOCOL_BITS[protocol]


def _validate_record(record, bits):
    kind = record.get("type")
    if not isinstance(kind, str) or kind not in _RECORD_TYPES:
        raise ValueError("unknown or missing record type")
    if kind not in _IGNORED_TYPES or "t" in record:
        _number(record.get("t"), f"{kind}.t")
    if kind == "frame":
        _integer(record.get("id"), "frame.id", 0, (1 << bits) - 1)
        _integer(record.get("dlc"), "frame.dlc", 0, 8)
        data = record.get("data")
        if not isinstance(data, str) or _HEX_PAYLOAD.fullmatch(data) is None:
            raise ValueError("frame.data must be uppercase compact hex (0..8 bytes)")
        if len(data) != 2 * record["dlc"]:
            raise ValueError("frame.data length does not match frame.dlc")
    elif kind == "marker":
        _text(record.get("label"), "marker.label", nonempty=True)
    elif kind == "event":
        _text(record.get("code"), "event.code", nonempty=True)
        _text(record.get("message"), "event.message")
        _boolean(record.get("incomplete"), "event.incomplete")
    elif kind == "summary":
        _boolean(record.get("complete"), "summary.complete")
        _boolean(record.get("stop_confirmed"), "summary.stop_confirmed")
        _integer(record.get("frames"), "summary.frames", 0, 2**63 - 1)


def _records(path, allow_incomplete, warnings, digest):
    """损坏行绝不静默忽略；有效 header 是恢复分析的前提。"""
    bits = None
    try:
        with path.open("rb") as source:
            for line_number, raw in enumerate(source, 1):
                digest.update(raw)
                if not raw.strip():
                    continue
                location = f"{path}:{line_number}"
                try:
                    record = json.loads(raw.decode("utf-8"),
                                        object_pairs_hook=_json_object,
                                        parse_constant=_json_constant)
                    if not isinstance(record, dict):
                        raise ValueError("record must be a JSON object")
                except (ValueError, UnicodeError, RecursionError) as error:
                    _problem(warnings, allow_incomplete,
                             f"{location}: malformed JSON/record skipped: {error}")
                    continue
                if bits is None:
                    try:
                        bits = _session_bits(record)
                    except ValueError as error:
                        raise ValueError(f"{location}: {error}") from None
                elif record.get("type") == "session":
                    raise ValueError(f"{location}: duplicate session header")
                else:
                    try:
                        _validate_record(record, bits)
                    except ValueError as error:
                        _problem(warnings, allow_incomplete,
                                 f"{location}: invalid record skipped: {error}")
                        continue
                yield line_number, record
    except OSError as error:
        raise ValueError(f"{path}: cannot read capture: {error}") from error
    if bits is None:
        raise ValueError(f"{path}: missing valid session header")


@dataclass
class _Log:
    path: Path
    bits: int
    start: float
    end: float
    markers: list
    digest: bytes
    allow_incomplete: bool
    settings: tuple
    marker_times: list = field(init=False)

    def __post_init__(self):
        self.marker_times = [timestamp for timestamp, _ in self.markers]

    def epoch_at(self, timestamp):
        return bisect_right(self.marker_times, timestamp) - 1


def _scan(path, allow_incomplete, warnings):
    digest = hashlib.sha256()
    bounds = {}
    markers = []
    summary = None
    bits = None
    frame_count = 0
    first_frame, last_frame = math.inf, -math.inf
    latest = 0.0
    for line_number, record in _records(path, allow_incomplete, warnings, digest):
        kind = record["type"]
        location = f"{path}:{line_number}"
        if kind == "session":
            bits = _session_bits(record)
            filters = tuple("".join(command.split()).upper()
                            for command in record["filters"]) if "filters" in record else None
            settings = (record["protocol"], filters)
        elif kind == "summary":
            if summary is not None:
                raise ValueError(f"{location}: duplicate summary")
            summary = record
        elif kind in ("monitor_start", "monitor_end"):
            if kind in bounds:
                raise ValueError(f"{location}: duplicate {kind}")
            bounds[kind] = float(record["t"])
        elif kind == "marker":
            markers.append((float(record["t"]), record["label"]))
        elif kind == "frame":
            frame_count += 1
            first_frame = min(first_frame, record["t"])
            last_frame = max(last_frame, record["t"])
        elif kind == "event" and record["incomplete"]:
            _problem(warnings, allow_incomplete,
                     f"{location}: incomplete event {record['code']}: {record['message']}")
        if kind != "summary" and "t" in record:
            latest = max(latest, record["t"])
    start, end = _monitor_bounds(path, bounds, last_frame, allow_incomplete, warnings)
    _check_summary(path, summary, frame_count, latest, allow_incomplete, warnings)
    if first_frame < start or last_frame > end:
        _problem(warnings, allow_incomplete,
                 f"{path}: frames outside monitor coverage [{start}, {end}] excluded")
    markers.sort()
    if any(left[0] == right[0] for left, right in zip(markers, markers[1:])):
        raise ValueError(f"{path}: ambiguous markers at the same timestamp")
    return _Log(path, bits, start, end, markers, digest.digest(), allow_incomplete, settings)


def _monitor_bounds(path, bounds, last_frame, allow_incomplete, warnings):
    if "monitor_start" not in bounds:
        raise ValueError(f"{path}: missing monitor_start; cannot establish valid coverage")
    start = bounds["monitor_start"]
    end = bounds.get("monitor_end")
    if end is None:
        _problem(warnings, allow_incomplete,
                 f"{path}: missing monitor_end; using last valid frame as conservative end")
        end = last_frame
    if not math.isfinite(end) or end <= start:
        raise ValueError(f"{path}: monitor duration must be positive and finite")
    return start, end


def _check_summary(path, summary, frame_count, latest, allow_incomplete, warnings):
    if summary is None:
        _problem(warnings, allow_incomplete, f"{path}: missing summary (truncated capture)")
        return
    if not summary["complete"]:
        _problem(warnings, allow_incomplete, f"{path}: summary complete=false")
    if not summary["stop_confirmed"]:
        _problem(warnings, allow_incomplete, f"{path}: summary stop_confirmed=false")
    if summary["frames"] != frame_count:
        _problem(warnings, allow_incomplete,
                 f"{path}: summary frames={summary['frames']} != valid frames={frame_count}")
    if summary["t"] < latest:
        _problem(warnings, allow_incomplete,
                 f"{path}: summary timestamp precedes capture records")


def _load(paths, allow_incomplete):
    _boolean(allow_incomplete, "allow_incomplete")
    if isinstance(paths, (str, bytes, Path)):
        raise ValueError("paths must be an iterable of capture paths, not one path")
    try:
        normalized = [Path(path).resolve() for path in paths]
    except (OSError, TypeError, ValueError) as error:
        raise ValueError(f"invalid capture paths: {error}") from error
    if not normalized:
        raise ValueError("no capture files supplied")
    if len(set(normalized)) != len(normalized):
        raise ValueError("duplicate capture paths would double-count evidence")
    warnings = []
    logs = [_scan(path, allow_incomplete, warnings) for path in sorted(normalized)]
    if any(log.settings != logs[0].settings for log in logs[1:]):
        raise ValueError("incompatible session protocol/filter settings; summarize/analyze "
                         "each protocol/filter group separately to preserve capture coverage")
    return logs, warnings


def _frames(log):
    digest = hashlib.sha256()
    for _, record in _records(log.path, log.allow_incomplete, [], digest):
        if record["type"] == "frame" and log.start <= record["t"] <= log.end:
            yield (float(record["t"]), record["id"], record["dlc"],
                   bytes.fromhex(record["data"]))
    if digest.digest() != log.digest:
        raise ValueError(f"{log.path}: capture changed between analysis passes")


def _id_text(key):
    bits, can_id, _ = key
    return f"{can_id:0{3 if bits == 11 else 8}X}"


@dataclass
class _Summary:
    frames: int = 0
    files: set = field(default_factory=set)
    unique: set = field(default_factory=set)
    capped: bool = False
    mask: bytearray = field(default_factory=bytearray)
    reference: bytes = b""
    first: bytes = b""
    last: bytes = b""
    first_position: tuple = None
    last_position: tuple = None
    labels: Counter = field(default_factory=Counter)
    unlabelled: int = 0

    def add(self, file_index, timestamp, payload, label):
        position = (file_index, timestamp)
        if self.frames == 0:
            self.reference = payload
            self.mask = bytearray(len(payload))
        for byte, value in enumerate(payload):
            self.mask[byte] |= value ^ self.reference[byte]
        if self.first_position is None or position < self.first_position:
            self.first_position, self.first = position, payload
        if self.last_position is None or position >= self.last_position:
            self.last_position, self.last = position, payload
        self.frames += 1
        self.files.add(file_index)
        if not self.capped and payload not in self.unique:
            if len(self.unique) < UNIQUE_PAYLOAD_LIMIT:
                self.unique.add(payload)
            else:
                self.capped = True
        if label is None:
            self.unlabelled += 1
        else:
            self.labels[label] += 1


def summarize_files(paths, allow_incomplete=False):
    """按 ID/DLC 汇总有效监视范围；返回标量行和显式警告，不执行硬件操作。"""
    logs, warnings = _load(paths, allow_incomplete)
    try:
        duration = math.fsum(log.end - log.start for log in logs)
    except OverflowError:
        raise ValueError("summed monitor duration is not finite") from None
    groups = {}
    for file_index, log in enumerate(logs):
        for timestamp, can_id, dlc, payload in _frames(log):
            epoch = log.epoch_at(timestamp)
            label = log.markers[epoch][1] if epoch >= 0 else None
            key = (log.bits, can_id, dlc)
            if key not in groups:
                groups[key] = _Summary()
            groups[key].add(file_index, timestamp, payload, label)
    if not groups:
        raise ValueError("no valid CAN frames in monitor coverage")
    rows = []
    for key in sorted(groups, key=lambda key: (key[1], key[0], key[2])):
        stats = groups[key]
        rate = stats.frames / duration
        if not math.isfinite(rate):
            raise ValueError("host receive rate is not finite for this monitor duration")
        rows.append(dict(zip(SUMMARY_FIELDS, [
            _id_text(key), key[2], stats.frames, len(stats.files), len(logs), duration,
            rate, len(stats.unique), stats.capped,
            ",".join(str(byte) for byte, mask in enumerate(stats.mask) if mask),
            stats.mask.hex().upper(), stats.first.hex().upper(), stats.last.hex().upper(),
            json.dumps(dict(stats.labels), sort_keys=True, ensure_ascii=True), stats.unlabelled,
        ])))
    warnings.append("host_receive_hz uses summed monitor coverage and host-receive timestamps; "
                    "it is not the CAN wire rate or a CAN wire timestamp.")
    return rows, warnings


@dataclass
class _Epoch:
    """每个文件/marker/ID/DLC 只保留计数，不保存帧或展开后的 bit 样本。"""
    width: int
    samples: int = 0
    ones: list = field(init=False)
    changes: list = field(init=False)
    previous: bytes = None
    previous_t: float = -math.inf
    ordered: bool = True

    def __post_init__(self):
        self.ones = [0] * self.width
        self.changes = [0] * self.width

    def add(self, timestamp, payload):
        if timestamp < self.previous_t:
            self.ordered = False
        for byte, value in enumerate(payload):
            changed = value ^ self.previous[byte] if self.previous is not None else 0
            for bit in range(8):
                offset = byte * 8 + bit
                self.ones[offset] += (value >> bit) & 1
                self.changes[offset] += (changed >> bit) & 1
        self.samples += 1
        self.previous, self.previous_t = payload, timestamp


def _windows(logs, states, settle):
    windows = {}
    expected = Counter()
    for file_index, log in enumerate(logs):
        for epoch, (timestamp, label) in enumerate(log.markers):
            end = log.markers[epoch + 1][0] if epoch + 1 < len(log.markers) else log.end
            start = max(timestamp + settle, log.start)
            if label in states and start < min(end, log.end):
                windows[file_index, epoch] = (label, start)
                expected[label] += 1
    for state in states:
        if not expected[state]:
            raise ValueError(f"no positive-duration post-settle marker epochs for state {state!r}")
    return windows, expected


def _collect_epochs(logs, windows):
    groups = {}
    for file_index, log in enumerate(logs):
        for timestamp, can_id, dlc, payload in _frames(log):
            epoch_key = (file_index, log.epoch_at(timestamp))
            window = windows.get(epoch_key)
            if window is None or timestamp < window[1]:
                continue
            key = (log.bits, can_id, dlc)
            epochs = groups.setdefault(key, {})
            if epoch_key not in epochs:
                epochs[epoch_key] = _Epoch(dlc * 8)
            epochs[epoch_key].add(timestamp, payload)
    return groups


def _state_evidence(epochs, windows, state, expected, min_samples):
    observed = [epoch for key, epoch in epochs.items() if windows[key][0] == state]
    eligible = [epoch for epoch in observed if epoch.samples >= min_samples]
    return {
        "eligible": eligible,
        "samples": sum(epoch.samples for epoch in eligible),
        "observed_samples": sum(epoch.samples for epoch in observed),
        "epochs": len(eligible),
        "expected_epochs": expected,
        "missing_epochs": expected - len(observed),
        "undersampled_epochs": len(observed) - len(eligible),
        "coverage": len(eligible) / expected,
    }


def _mean(values):
    return math.fsum(values) / len(values) if values else None


def _bit_evidence(evidence, offset):
    epochs = evidence["eligible"]
    occupancies = [epoch.ones[offset] / epoch.samples for epoch in epochs]
    ordered = all(epoch.ordered for epoch in epochs)
    rates = [epoch.changes[offset] / (epoch.samples - 1)
             for epoch in epochs if epoch.samples > 1] if ordered else []
    return {
        "occupancy": _mean(occupancies),
        "consistency": _mean([max(value, 1 - value) for value in occupancies]),
        "repeatability": 1 - (max(occupancies) - min(occupancies)) if len(epochs) >= 2 else None,
        "transition_rate": _mean(rates),
        "transitions": sum(epoch.changes[offset] for epoch in epochs) if epochs and ordered else None,
    }


def _candidate(key, offset, states, evidence):
    a, b = [_bit_evidence(item, offset) for item in evidence]
    delta = b["occupancy"] - a["occupancy"] if a["occupancy"] is not None and b["occupancy"] is not None else None
    coverage = min(item["coverage"] for item in evidence)
    if delta == 0 and coverage == 1:
        return None
    flags = ["unverified_semantics"]
    if coverage < 1:
        flags.append("insufficient_coverage")
    if min(item["epochs"] for item in evidence) < 2:
        flags.append("limited_repeatability")
    if any(not epoch.ordered for item in evidence for epoch in item["eligible"]):
        flags.append("unordered_frames")
    if any(item["consistency"] is not None and item["consistency"] < 1 for item in (a, b)):
        flags.append("within_state_variation")
    score = 0.0
    if delta is not None:
        repeatability = min(item["repeatability"] or 0.0 for item in (a, b))
        transition = _mean([item["transition_rate"] for item in (a, b)
                            if item["transition_rate"] is not None]) or 0.0
        score = (100 * abs(delta) * (a["consistency"] + b["consistency"]) / 2
                 * coverage * (0.5 + 0.5 * repeatability) * (1 - 0.5 * transition))
    row = {
        "id": _id_text(key), "dlc": key[2], "byte": offset // 8, "bit": offset % 8,
        "mask": f"{1 << (offset % 8):02X}", "state_a": states[0], "state_b": states[1],
        "delta": delta, "abs_delta": abs(delta) if delta is not None else None,
        "score": round(score, 6), "flags": ";".join(flags),
    }
    for suffix, bit, counts in zip(("a", "b"), (a, b), evidence):
        row.update((f"{name}_{suffix}", value) for name, value in bit.items())
        row.update((f"{name}_{suffix}", value) for name, value in counts.items() if name != "eligible")
    return {name: row[name] for name in CANDIDATE_FIELDS}


def analyze_files(paths, state_a, state_b, settle=1.0, min_samples=5,
                  allow_incomplete=False):
    """按时间戳划分 A/B 窗口、等权比较每位；结果只是候选关联，不是语义解码。"""
    _text(state_a, "state_a", nonempty=True)
    _text(state_b, "state_b", nonempty=True)
    if state_a == state_b:
        raise ValueError("state_a and state_b must be distinct labels")
    settle = _number(settle, "settle")
    _integer(min_samples, "min_samples", 1, 2**63 - 1)
    logs, warnings = _load(paths, allow_incomplete)
    states = (state_a, state_b)
    windows, expected = _windows(logs, states, settle)
    groups = _collect_epochs(logs, windows)
    rows = []
    comparable = False
    for key in sorted(groups):
        evidence = [_state_evidence(groups[key], windows, state, expected[state], min_samples)
                    for state in states]
        if key[2] > 0 and all(item["epochs"] for item in evidence):
            comparable = True
        for offset in range(key[2] * 8):
            row = _candidate(key, offset, states, evidence)
            if row is not None:
                rows.append(row)
    if not comparable:
        raise ValueError("no A/B evidence: no payload-bearing ID/DLC has at least "
                         f"{min_samples} samples in an epoch of each state")
    rows.sort(key=lambda row: (-row["score"], int(row["id"], 16), len(row["id"]),
                               row["byte"], row["bit"], row["dlc"]))
    warnings.append("unverified_semantics: A/B correlation is not verified signal semantics; "
                    "byte0 is the first byte, bit0 is LSB. Timestamps are host-receive, "
                    "not CAN wire timestamps; scores do not classify counters/checksums.")
    if any("insufficient_coverage" in row["flags"] for row in rows):
        warnings.append("insufficient_coverage: missing/undersampled ID/DLC epochs are "
                        "reported, not filled with zero-valued bit samples.")
    if any("limited_repeatability" in row["flags"] for row in rows):
        warnings.append("limited_repeatability: fewer than two eligible epochs of at least "
                        "one state; repeatability is unmeasured, not verified.")
    if any(not epoch.ordered for epochs in groups.values() for epoch in epochs.values()):
        warnings.append("unordered_frames: some ID/DLC epoch timestamps run backwards; "
                        "affected transition counts/rates are unavailable.")
    if not rows:
        warnings.append("No candidates: no nonzero A/B bit occupancy differences with "
                        "the available eligible epochs.")
    return rows, warnings
