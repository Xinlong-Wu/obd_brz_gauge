# Generic CAN broadcast signal discovery

English | [简体中文](CAN_DISCOVERY.md)

`tools/elm327_can_discover.py` is a standalone host tool: BLE monitoring → JSONL recording → offline inventory / state comparison. It does not depend on firmware vehicle profiles and is not limited to the MX-5 or predefined signals such as doors, lights, or roofs; experimenters choose their own labels. It produces correlation candidates, not automatic signal names, proven meanings, or generated firmware rules.

## Scope and safety

- v1 supports BLE ELM327 only, not WiFi, USB, or Bluetooth serial. `scan` / `capture` require `bleak`; `summary` / `analyze` use only the Python standard library.
- Only classic CAN data frames are supported: 11 / 29-bit IDs, DLC 0–8 (including zero-byte data frames). CAN FD / remote frames are unsupported; unsupported formats fail clearly rather than being guessed at.
- Park and secure the vehicle, follow its service manual, and keep people clear of moving mechanisms such as a roof. Do not operate a computer while driving. Disconnect phone apps / gauges using the same adapter; allow only one client at a time.
- No vehicle diagnostic requests are sent: there is no hidden `0100` probe, arbitrary-send interface, or actuator-control feature. However, an `OK` response to `ATCSM1` is **not electrical proof of silent operation**; clones may ignore it. Use verified listen-only hardware if strict no-ACK / no-transmission behavior is required.
- Raw captures contain device identifiers, timestamps, and vehicle activity. Treat them as private by default: store outside the repository, do not commit or upload them, and make a separate redacted copy before sharing.

## Installation and command entry point

Run from the repository root; a virtual environment is recommended:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install bleak
mkdir -p ~/can-captures
python3 tools/elm327_can_discover.py --help
python3 tools/elm327_can_discover.py scan --scan-timeout 5
```

All time options (`--scan-timeout`, `--duration`, `--settle`, `--timeout`, `--drain-timeout`) are in **seconds**. `scan` defaults to 5 seconds. For `capture`, `--timeout` defaults to `3` seconds for adapter command prompts; `--drain-timeout` must be positive and defaults to `30` seconds for background draining after stopping. These are separate timeouts, not interchangeable. `--buffer-mb` is an integer from `1`–`1024`, default `64`, in MiB (`1 MiB = 1024 × 1024 bytes`); the budget includes receive payloads and estimated per-item overhead, not an exact process RSS limit. `capture` defaults to `--protocol 6`. The supported protocols follow; there is no automatic bitrate discovery, custom bitrate, or automatic CAN pin switching:

```text
6 = 11-bit CAN, 500 kbit/s
7 = 29-bit CAN, 500 kbit/s
8 = 11-bit CAN, 250 kbit/s
9 = 29-bit CAN, 250 kbit/s
```

For `capture`, choose either `--name IOS-Vlink` or `--address UUID` (replace examples with the actual scanned name / address; macOS typically uses a UUID). JSONL and CSV outputs **never overwrite existing files**; the parent directory must already exist. Use a new filename for each repeat; there is no `--force`.

## Inventory before designing label experiments

Without performing the target action, begin with an optimized **10-second unfiltered pilot with a 64 MiB host buffer** to check connectivity, format, visible IDs, and throughput metrics. Do not guess a “body-system ID” in advance or filter out unknown candidates from the outset. ELM / BLE can still overflow within seconds, so even this pilot is not guaranteed complete. Inspect receive / processing rates, host-buffer backlog / high-water mark, and writer lag before choosing the next step:

```bash
python3 tools/elm327_can_discover.py capture \
  --name IOS-Vlink --protocol 6 --duration 10 --buffer-mb 64 \
  --label inventory --no-input --out ~/can-captures/inventory-01.jsonl
python3 tools/elm327_can_discover.py summary \
  ~/can-captures/inventory-01.jsonl --top 50 --out ~/can-captures/inventory-01.csv
```

`summary` inventories frame counts, receive frequency (Hz), varying bytes, and related statistics by ID / DLC. `--top` defaults to `50` and limits terminal display only; `--out` exports all results to CSV. Omit `--out` for terminal output only. Multi-file frequency divides by the sum of all monitoring windows, including files where an ID is absent, not just periods when it appeared. Host BLE receive timestamps **are not CAN wire transmission times**. Adapter buffering, BLE fragmentation / batching, and throughput limits affect observed frequency; it is not precise bus timing or a complete bus-load measurement.

## Host buffering and throughput diagnostics

The BLE event loop does minimal receive work: retain receive timestamps, enqueue bytes, and promptly recognize command prompts. A dedicated background thread parses frames and writes JSONL, rather than doing that work in the BLE callback. Local labels, commands, and other metadata enter the same writer FIFO and are written in enqueue order with received chunks, not through a separate write path that could reorder them.

`--buffer-mb` limits a byte budget including estimated per-item overhead, not a fixed item count or an unbounded queue. Exhausting the budget explicitly counts dropped receive chunks / bytes, marks the capture incomplete, and triggers stopping. Budget exhaustion does not use ring overwrite or discard already-queued early frames to retain the tail. Drain-timeout discards are described below. A larger buffer only absorbs temporary backlog: it cannot fix a sustained processing-rate deficit, extend physical BLE throughput, or enlarge the ELM's internal buffer capacity.

During capture, the terminal reports receive rate (KiB/s), processing rate (KiB/s), pending buffer / limit / high-water mark, and total parsed frames every `2` seconds. Periodic telemetry uses the existing `event` record with `code="telemetry"`, `message="host receive/writer statistics"`, and `incomplete=false`, not a new record type. That event's `incomplete=false` does not mean the entire capture is complete. Telemetry events and the final `summary` both include these cumulative counters / buffer-state / lag metrics:

```text
rx_bytes
processed_bytes
queue_drops
dropped_bytes
buffer_bytes
buffer_limit_bytes
peak_buffer_bytes
writer_lag_seconds
frames
```

Telemetry events also include `rx_bytes_per_second` and `processed_bytes_per_second` in bytes/s, divided by `1024` for terminal display in KiB/s; the final `summary` does not include these two interval-rate fields. `queue_drops` counts receive chunks rejected or discarded on drain timeout, not CAN frames; their payload bytes count toward `dropped_bytes`. Receive / processing byte counters measure adapter-text bytes received / processed by the host, not CAN wire payload volume. Terminal rates show current throughput; cumulative metrics help check the entire capture.

1. If processing stays slower than reception and pending buffer / writer lag grow, first investigate / optimize host parsing, disk writes, and other load; increase `--buffer-mb` if needed and repeat. Merely extending `--duration` amplifies the backlog.
2. ELM `BUFFER FULL` with low host backlog suggests a bottleneck before the host queue, such as adapter internals or BLE transport; these metrics alone cannot conclusively identify the component. More host buffering cannot recover upstream data already lost.
3. Perform these throughput checks first. If the adapter / link still cannot sustain unfiltered traffic, use hardware-filtered groups below and narrow progressively, repeating each experiment in a new file. Neither a larger host buffer nor zero local drops guarantees end-to-end lossless capture.

## Free-form label experiments

Change one condition at a time: stabilize at `baseline`, then `active`, then return to `baseline`. Repeat within a capture and across separate captures. Use a distinct `transition` label while changing state, then label the stable target state. Record ignition, engine, other loads, and action order to avoid mistaking time drift for the target signal. The following 120-second experiment uses a 128 MiB host buffer and a 60-second queue-drain deadline (not a guarantee that the process exits within 60 seconds; see the filesystem I/O limitation below). Check throughput as above first and add verified `--filter` / `--mask` or `--can-id` options below if needed; do not extend an unfiltered capture without checking.

```bash
python3 tools/elm327_can_discover.py capture \
  --name IOS-Vlink --protocol 6 --duration 120 --buffer-mb 128 --drain-timeout 60 \
  --label baseline --note 'repeat-01; parked; one condition at a time' \
  --out ~/can-captures/experiment-01.jsonl
```

During capture, type any label line into the terminal and press Enter (the following are sample labels, not shell commands):

```text
transition
active
transition
baseline
/quit
```

Labels and `--note` are **local records only, never sent to the vehicle**. `/quit` stops capture; `Ctrl-C` requests a graceful stop and finalization. EOF only disables label input: capture continues until `--duration` expires. `--no-input` disables interactive labels entirely, useful for inventory or separate fixed-state captures. On natural expiry, `/quit`, or `Ctrl-C`, the stop-monitor command is sent to the adapter immediately, before waiting for host backlog to drain. The BLE callback confirms the returned prompt directly, without delay from the background writer. The queued tail is then given `--drain-timeout` to drain before the final summary is written. On drain timeout, the capture is marked incomplete, unprocessed queued receive data is discarded with explicit `queue_drops` / `dropped_bytes` accounting, and already-written data is retained. Finalization must still wait for in-flight filesystem I/O to finish and the background thread to exit before safely writing the final summary. The tool cannot forcibly interrupt hung filesystem I/O, so `--drain-timeout` is not a hard process-exit deadline; an unfinished file is never falsely reported complete. Check final completeness after either natural expiry or a requested stop; an existing file is not proof of success.

`active` may represent any safely repeatable condition, not just a discrete switch. For continuous quantities, choose labels for multiple stable ranges and compare pairs. Manual labels have human reaction delay and cannot prove millisecond-scale causality.

## Offline comparison and revalidation

```bash
python3 tools/elm327_can_discover.py summary \
  ~/can-captures/experiment-01.jsonl ~/can-captures/experiment-02.jsonl
python3 tools/elm327_can_discover.py analyze \
  ~/can-captures/experiment-01.jsonl ~/can-captures/experiment-02.jsonl \
  --state-a baseline --state-b active --settle 1 --min-samples 5 \
  --top 20 --out ~/can-captures/candidates-01.csv
```

`experiment-02.jsonl` is a separate capture using the same procedure, not a copy of the first file. Combined analysis requires matching protocol and hardware-filter settings; inventory and compare different filter groups separately, rather than merging them into full-bus rates or state coverage. `summary` / `analyze` accept one or more files and do not connect to an adapter. `analyze` requires two distinct labels; defaults exclude the first `1` second of each segment (`--settle`), require `5` samples per file / label segment / ID / DLC (`--min-samples`), and display the top `20` candidates (`--top`); CSV still exports all results. Missing / insufficient samples produce coverage diagnostics, not invented zeros. `score` is a ranking heuristic, not a probability or confidence level. Bytes are indexed from **byte 0**; within a byte, **bit 0 is the LSB**, unlike the MSB-first numbering used in some external references.

Ranking is a **correlation clue** between states and byte / bit changes, not a known-signal table, unit / formula identification, or proof of meaning. Counters, checksum bytes, ignition changes, and synchronized loads can rank highly. To revalidate:

1. Recapture the candidate's exact ID, repeat `baseline → active → baseline` several times, and check reversibility and agreement across captures.
2. Add unrelated actions as controls to exclude voltage / load / time confounders; retain failures and counterexamples.
3. Distinguish transient events from persistent states: actual lamp flashing is not the same as a stalk / button request, and roof motion is not proof of a latched roof. An action changing several conditions cannot establish a bit's meaning by itself.
4. Consider firmware integration only after independent revalidation; generic tool candidates are not limited to existing firmware channels.

## Filtering and throughput limits

Hardware receive filters restrict **what is actually visible**, not just offline display. `--can-id HEX` and `--filter HEX --mask HEX` are mutually exclusive; the latter two must be supplied together. Use 3 / 8 hexadecimal digits for the selected protocol: `000`–`7FF` for 11-bit IDs, `00000000`–`1FFFFFFF` for 29-bit IDs. Here `123` illustrates exact-filter syntax only, not a known signal:

```bash
python3 tools/elm327_can_discover.py capture \
  --name IOS-Vlink --protocol 6 --duration 120 --label baseline \
  --can-id 123 --out ~/can-captures/exact-123-01.jsonl
python3 tools/elm327_can_discover.py capture \
  --address UUID --protocol 6 --duration 120 --label inventory --no-input \
  --filter 000 --mask 700 --out ~/can-captures/group-000-01.jsonl
```

Mask matching follows:

```text
(id & mask) == (filter & mask)
```

`--protocol 6 --filter 000 --mask 700` covers only `000`–`0FF`, **not all 11-bit IDs**. Keep `--mask 700` and separately capture groups with `--filter 100`, `200`, `300`, `400`, `500`, `600`, `700`; the last covers `700`–`7FF`. Use a new file and repeat the same label experiment for each group. Sequential captures are not a simultaneous whole-bus view. After the unfiltered pilot and host-throughput checks, if traffic still needs limiting, inventory groups before narrowing to exact IDs. Offline analysis cannot recover signals excluded by filters, isolated by a gateway, or never broadcast.

## Initialization, record format, and completeness

Capture resets the adapter and waits for it to settle, checks initialization responses individually, clears / sets hardware filters, then starts monitoring:

```text
ATZ
ATE0
ATL0
ATS0
ATH1
ATCAF0
ATD0
ATCFC0
ATCSM1
ATSPn
ATMA
```

`n` is the selected protocol. `ATZ` resets the adapter and reloads its saved settings; these settings are stored on the adapter, not in the vehicle. Before `ATMA`, unfiltered mode explicitly clears stale receive filters, exact mode sets `ATCRA`, and mask mode sets `ATCF` / `ATCM`; responses are checked individually. An ID outside the selected filter raises an error and stops capture: acknowledgment alone is not sufficient evidence that filtering works. `ATS0` replaces `ATS1` to remove text spaces and reduce transport volume. `ATH1` still preserves CAN headers, raw received bytes remain recorded, and the parser already supports the compact no-space format. `ATCAF0` preserves raw frames, `ATCFC0` disables automatic flow control, and `ATCSM1` requests silent monitoring. An unsupported / unacknowledged key command aborts rather than silently falling back to active queries. This command chain and `OK` responses still do not prove a clone's electrical behavior.

Output is JSONL, one JSON record per line, with schema `elm327_can_discover`, version `1`, and these record types:

```text
session
transport
command
command_response
rx
frame
marker
event
monitor_start
monitor_end
summary
```

Records retain session configuration, BLE channel metadata (`transport`), commands and acknowledgments (`command_response`), raw reception, parsed frames, manual labels, events, monitoring boundaries, periodic throughput metrics (`event` with `code="telemetry"`), and the final summary; these additional diagnostics are local records. Telemetry reuses the existing schema version `1` `event` type; the offline analyzer is unchanged and older captures do not need added telemetry events. `monitor_start` / `monitor_end` distinguish initialization from the actual monitoring window. The `monitor_end` timestamp is when reception actually closes, not final write time, even if the record is written after draining. Background draining after stopping therefore does not extend the listening period or the frequency-statistics denominator. Time is in seconds; receive time comes from the host BLE callback, not a hardware CAN timestamp. Renaming an old dump text file does not turn it into this schema.

BLE errors / disconnection, ELM `BUFFER FULL`, malformed traffic, out-of-filter IDs, chunk loss when the host byte budget is exhausted, or a background drain timeout all make the capture incomplete. Zero valid frames or missing finalization must not be treated as a complete capture either. `summary` / `analyze` **reject incomplete captures by default**. Use `--allow-incomplete` only for troubleshooting; the tool warns, and results are diagnostic only, not complete frequency statistics or evidence validating a signal. Retain existing data for troubleshooting, use throughput metrics to check host backlog first, then narrow the hardware filter if needed and recapture in a new file. Do not draw conclusions through frame loss.

## MX-5 NC / PRHT is only an example

An NC PRHT (power retractable hard top) can be studied through stable states and motion transitions, but a manual soft top has different mechanisms, sensors, and control modules; PRHT assumptions do not carry over. Model year / market / equipment may also change visible traffic. This guide supplies no unverified door, light, or roof bit maps and does not guarantee that any target state is broadcast on the adapter's connected network.

No broadcast result does not mean the state does not exist: first check protocol, filters, connected network, and gateway visibility. Do not assume an NC body network must run at 125 kbit/s or that the adapter can switch to MS-CAN pins. If enhanced module diagnostics are ultimately required, use service documentation for a separate **read-only** query investigation. That is an active-request path, not this tool's passive capture. v1 does not blind-scan DIDs or control mechanisms such as the roof.

References establish hypotheses to validate; verify each source's bit numbering separately:

- [NC CAN community notes](https://github.com/hatsunearu/nc-mx5-miata-canbus/blob/main/mazda_mx5_nc.md): existing powertrain / chassis notes include WIP and are not a complete body-signal map.
- [NC PRHT service reference: power_roof.pdf](https://www.mellens.net/mazda/Mazda-Miata-2006-2007/power_roof.pdf): mechanisms, sensors, and diagnostic procedures, not a validated CAN bit map; match it to the actual vehicle version.
- [Official ELM327 datasheet](https://www.elmelectronics.com/wp-content/uploads/2016/07/ELM327DS.pdf?download=1): protocol, monitor, and filter commands, not a substitute for testing a clone adapter.

## Development checks and existing tools

Run host tests from the repository root without a car / BLE connection:

```bash
python3 -B -m unittest discover -s tests -p 'test_*discover*.py' -v
```

Tests do not prove clone silence or real-vehicle signal mappings. Existing `tools/elm327_ble_dump.py` remains unchanged for interactive / active PID queries; do not use it on the adapter at the same time. Other tools are listed in [tools/README.en.md](../tools/README.en.md); see [VEHICLES.en.md](VEHICLES.en.md) for integration after real-vehicle validation.
