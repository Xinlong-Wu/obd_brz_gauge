#!/usr/bin/env python3
"""
elm327_ble_dump.py — 主机直连 BLE ELM327 适配器，收发原始 OBD 命令/报文。

（读真实车的原始数据；仓库里 fake_elm327.py 是反方向——伪装适配器让 App 来连。）

依赖（仅此一个第三方包）:
    pip3 install bleak

适配器自动识别，与固件 main/bsp_obd_dsp/elm327_ble_client.c 同一套 UUID 优先级:
    FFF0: FFF1 写 / FFF2 通知   常见 ELM327 / VEEPEAK 等
    18F0: 2AF1 写 / 2AF0 通知   IOS-Vlink / Vgate iCar Pro
    FF12: FF15 写 / FF14 通知   Viecar 等
    NUS : Nordic UART 透传     部分克隆
    都没有时退化为「任意可写特征 + 任意可通知特征」并告警

用法:
    python3 tools/elm327_ble_dump.py --scan                # 只扫描周边 BLE 设备
    python3 tools/elm327_ble_dump.py                       # 扫描 → 自动连（多个候选让你选）
    python3 tools/elm327_ble_dump.py --name OBDII          # 按名字连
    python3 tools/elm327_ble_dump.py --address XX:XX:..    # 按地址连（macOS 上填扫描打印的 UUID）
    python3 tools/elm327_ble_dump.py --cmd "ATZ;ATE0;ATSP0;0100;010C"  # 跑完即退
    python3 tools/elm327_ble_dump.py --loop 010C --interval 1          # 轮询，Ctrl-C 停
    python3 tools/elm327_ble_dump.py --all                 # 查支持位图 → 全部支持 PID 轮一遍 → CSV
    python3 tools/elm327_ble_dump.py --all --csv dump.csv --interval 0.2
    python3 tools/elm327_ble_dump.py --raw                 # 任何模式下逐包打印原始字节

--all 流程：0100/0120/… 逐块查 mode 01 支持位图（顺带试 mode 09 车辆信息），
然后把每个支持的 PID 发一遍，原始响应 + 标准 PID 名/公式解码写 CSV
（默认 obd_dump_时间戳.csv，复用 analyze_proble.py 的 SAE J1979 表）。

连上后先发 --init（默认 "ATZ;ATE0;ATL0;ATS0;ATSP0"），然后:
    * 无 --cmd/--loop → 进交互模式，直接敲 ELM327 命令（010C / 0100 / ATH1 / ATDP ...），
      q 或 Ctrl-C 退出
注意: ELM327 命令以 CR 结尾（脚本自动补）；一条命令的响应以 '>' 提示符结束，
超时（--timeout）未见到 '>' 时把已收内容原样吐出。
"""

import argparse
import asyncio
import csv
import sys
import time
from datetime import datetime

BleakClient = BleakScanner = None


def _load_bleak():
    global BleakClient, BleakScanner
    if BleakClient is None:
        try:
            from bleak import BleakClient, BleakScanner
        except ImportError:
            sys.exit("缺依赖 bleak，请先执行: pip3 install bleak")
        globals()["BleakClient"] = BleakClient
        globals()["BleakScanner"] = BleakScanner

# 与固件 elm327_ble_client.c 一致的服务/写/通知 UUID（bleak 统一为 128 位小写形式）
def _u16(x):
    return f"0000{x:04x}-0000-1000-8000-00805f9b34fb"

KNOWN_SERVICES = [
    ("FFF0", _u16(0xFFF0), _u16(0xFFF1), _u16(0xFFF2)),
    ("18F0", _u16(0x18F0), _u16(0x2AF1), _u16(0x2AF0)),
    ("FF12", _u16(0xFF12), _u16(0xFF15), _u16(0xFF14)),
    ("NUS", "6e400001-b5a3-f393-e0a9-e50e24dcca9e",
            "6e400002-b5a3-f393-e0a9-e50e24dcca9e",
            "6e400003-b5a3-f393-e0a9-e50e24dcca9e"),
]

# 扫描结果里按名字挑出疑似 OBD 适配器（子串匹配）
NAME_KEYWORDS = ("OBD", "ELM", "V-LINK", "VLINK", "VGATE", "VIECAR", "VEEPEAK", "IOS-V", "CAR")


def find_chars(client):
    """按固件同款优先级在已连接客户端上找 (标签, 写特征, 通知特征)。"""
    for label, svc, wr, nt in KNOWN_SERVICES:
        for s in client.services:
            if s.uuid.lower() != svc:
                continue
            chars = s.characteristics
            w = next((c for c in chars if c.uuid.lower() == wr), None)
            n = next((c for c in chars if c.uuid.lower() == nt), None)
            if label == "FFF0" and n is None and w is not None:
                # 固件同款兼容：少数克隆没有 FFF2，通知走 FFF1
                n = w
            if w and n and ("notify" in n.properties or "indicate" in n.properties or n is w):
                return label, w, n
    all_chars = [c for s in client.services for c in s.characteristics]
    w = next((c for c in all_chars if "write" in c.properties
              or "write-without-response" in c.properties), None)
    n = next((c for c in all_chars if "notify" in c.properties or "indicate" in c.properties), None)
    return ("GENERIC", w, n) if w and n else (None, None, None)


class ElmSession:
    """攒响应：ELM327 用 '>' 提示符表示一条命令的响应结束。"""

    def __init__(self, raw=False):
        self.raw = raw
        self.buf = bytearray()
        self.got_prompt = asyncio.Event()

    def on_notify(self, _char, data: bytearray):
        if self.raw:
            print(f"    [RX {len(data):3d}B] {data.hex(' ').upper()}")
        self.buf += data
        if b">" in data:
            self.got_prompt.set()

    def arm(self):
        self.buf.clear()
        self.got_prompt.clear()

    async def wait_response(self, timeout):
        try:
            await asyncio.wait_for(self.got_prompt.wait(), timeout)
        except asyncio.TimeoutError:
            print(f"    (超时 {timeout}s 未见 '>'，输出已收内容)")
        out = bytes(self.buf)
        self.arm()
        return out


def print_response(resp: bytes):
    lines = [ln.strip() for ln in resp.decode("ascii", "replace").split("\r")]
    lines = [ln for ln in lines if ln]
    if not lines:
        print("    <无响应>")
        return
    for ln in lines:
        print(f"    {ln}")


async def send_command(client, w_char, sess, cmd, timeout):
    sess.arm()
    t0 = time.monotonic()
    data = (cmd.strip() + "\r").encode("ascii", "replace")
    await client.write_gatt_char(w_char, data, response="write" in w_char.properties)
    resp = await sess.wait_response(timeout)
    print(f"[{cmd.strip()}] ({time.monotonic() - t0:.2f}s)")
    print_response(resp)
    return resp


# --------------------------------------------------------------- --all 模式 ---

NOISE_MARKS = ("SEARCHING", "NO DATA", "BUS INIT", "BUSINIT", "STOPPED",
               "UNABLE", "ERROR", "?")


def extract_data(resp: bytes, want: str):
    """从原始响应取第一行 want 前缀（如 '410C'）的数据段；没有返回 None。"""
    for ln in resp.decode("ascii", "replace").split("\r"):
        s = "".join(ln.split()).upper()
        if not s or any(m in s for m in NOISE_MARKS):
            continue
        if s.startswith(want) and len(s) > len(want):
            try:
                return bytes.fromhex(s[len(want):])
            except ValueError:
                return None
    return None


def parse_support_bits(data: bytes):
    """支持位图 4 字节 → 相对基址（1..32）的支持集合，调用方负责加 base。"""
    if len(data) < 4:
        return set()
    val = int.from_bytes(data[:4], "big")
    return {i + 1 for i in range(32) if val & (1 << (31 - i))}


async def discover_supported(client, w_char, sess, mode, timeout, delay, results):
    """00/20/40/… 逐块查某 mode 的支持位图，返回支持的 PID 集合，响应记入 results。"""
    supported = set()
    base = 0
    while True:
        # 首次探测可能触发 ELM 的 SEARCHING...，放宽超时
        wait = max(timeout, 8.0) if base == 0 else timeout
        resp = await send_command(client, w_char, sess, f"{mode:02X}{base:02X}", wait)
        results[(mode, base)] = resp
        data = extract_data(resp, f"{mode + 0x40:02X}{base:02X}")
        if data is None:
            if base == 0:
                print(f"  mode {mode:02X}: 无有效应答（车不支持该 mode 或未连好）")
            break
        bits = parse_support_bits(data)
        supported |= {base + b for b in bits}
        if base == 0:
            print(f"  mode {mode:02X}: 支持 {len(supported)} 个 PID")
        if base + 0x20 in bits and base + 0x20 <= 0xE0:
            base += 0x20
            await asyncio.sleep(delay)
            continue
        break
    return supported


async def sweep_pids(client, w_char, sess, mode, pids, timeout, delay, results):
    """把给定 PID 全部发一遍，返回 {(mode,pid): 数据段}；位图 PID 发现阶段已记录，跳过。"""
    answered = {}
    for pid in sorted(pids):
        if pid % 0x20 == 0:
            continue
        resp = await send_command(client, w_char, sess, f"{mode:02X}{pid:02X}", timeout)
        results[(mode, pid)] = resp
        data = extract_data(resp, f"{mode + 0x40:02X}{pid:02X}")
        if data is not None:
            answered[(mode, pid)] = data
        await asyncio.sleep(delay)
    return answered


def write_dump_csv(path, results, answered):
    """原始响应 + 标准 PID 名/公式解码 → CSV。返回行数。"""
    try:
        import analyze_proble as ap  # 同目录：复用 SAE J1979 标准表与公式求值
    except ImportError:
        ap = None
    with open(path, "w", newline="", encoding="utf-8-sig") as f:
        out = csv.writer(f)
        out.writerow(["request", "pid", "name", "response_raw", "data_hex",
                      "decoded", "unit"])
        rows = 0
        for (mode, pid), resp in sorted(results.items()):
            req = f"{mode:02X}{pid:02X}"
            name = formula = unit = ""
            if ap is not None:
                info = ap.std_info(req)
                if info:
                    name, formula, unit = info[1], info[2], info[3]
            data = answered.get((mode, pid), b"")
            decoded = ""
            if data and ap is not None:
                val = ap.eval_formula(formula, data)
                if val is not None:
                    decoded = f"{val:g}"
            text = resp.decode("ascii", "replace").replace("\r", " | ").strip()
            out.writerow([req, f"{pid:02X}", name, text, data.hex().upper(),
                          decoded, unit])
            rows += 1
    return rows


async def run_cmd_list(client, w_char, sess, cmds, timeout):
    for cmd in cmds:
        cmd = cmd.strip()
        if not cmd:
            continue
        await send_command(client, w_char, sess, cmd, timeout)
        if cmd.upper() == "ATZ":
            await asyncio.sleep(1.0)  # 复位后适配器要缓一下


async def scan_rows(timeout=5.0):
    found = await BleakScanner.discover(timeout=timeout, return_adv=True)
    rows = []
    for dev, adv in found.values():
        name = dev.name or adv.local_name or "(无名)"
        rows.append({
            "known": any(k in name.upper() for k in NAME_KEYWORDS),
            "rssi": adv.rssi if adv.rssi is not None else -128,
            "name": name,
            "address": dev.address,
        })
    rows.sort(key=lambda r: (not r["known"], -r["rssi"]))
    return rows


def print_rows(rows):
    print(f"{'':>2}  {'RSSI':>4}  {'名称':<20}  地址")
    for i, r in enumerate(rows):
        mark = "*" if r["known"] else " "
        print(f"{mark}{i}  {r['rssi']:>4}  {r['name']:<20}  {r['address']}")
    print("(* = 名字疑似 OBD 适配器)")


async def pick_device(args):
    """返回 (address, 显示名)。"""
    if args.address:
        return args.address, args.address
    print(f"扫描 BLE 设备（{args.scan_timeout}s）...")
    rows = await scan_rows(args.scan_timeout)
    if not rows:
        sys.exit("没扫到任何 BLE 设备。确认适配器已插 OBD 口/上电，且电脑蓝牙已开。")
    print_rows(rows)
    if args.name:
        hit = next((r for r in rows if args.name.lower() in r["name"].lower()), None)
        if hit is None:
            sys.exit(f"没找到名字含 {args.name!r} 的设备，从上面列表挑一个用 --address 连。")
        return hit["address"], hit["name"]
    known = [r for r in rows if r["known"]]
    if len(known) == 1:
        print(f"自动选择: {known[0]['name']} [{known[0]['address']}]")
        return known[0]["address"], known[0]["name"]
    try:
        raw = await asyncio.to_thread(
            input, "选择序号（回车=第一个疑似项）: ")
        idx = int(raw) if raw.strip() else rows.index(known[0]) if known else 0
        return rows[idx]["address"], rows[idx]["name"]
    except (ValueError, IndexError):
        sys.exit("序号无效，重跑再来。")


async def interactive(client, w_char, sess, args):
    print("交互模式：直接敲 ELM327 命令（如 010C、ATH1、ATDP），q 退出")
    while True:
        try:
            line = (await asyncio.to_thread(input, "elm> ")).strip()
        except (EOFError, KeyboardInterrupt):
            print()
            return
        if line.lower() in ("q", "quit", "exit"):
            return
        if not line:
            continue
        try:
            await send_command(client, w_char, sess, line, args.timeout)
        except Exception as exc:  # 连接断了等
            print(f"发送失败: {exc}")
            return


async def main():
    p = argparse.ArgumentParser(description="主机直连 BLE ELM327，收发原始 OBD 命令/报文")
    p.add_argument("--scan", action="store_true", help="只扫描，不连接")
    p.add_argument("--scan-timeout", type=float, default=5.0, help="扫描时长秒数（默认 5）")
    p.add_argument("--address", help="按地址连（macOS 上是扫描打印的 UUID）")
    p.add_argument("--name", help="按名字连（子串匹配，如 OBDII、Vgate）")
    p.add_argument("--init", default="ATZ;ATE0;ATL0;ATS0;ATSP0",
                   help="连上后先发的初始化命令，分号分隔；传空串跳过")
    p.add_argument("--cmd", help="依次执行分号分隔的命令后退出")
    p.add_argument("--loop", help="轮询一条命令（如 010C），Ctrl-C 停")
    p.add_argument("--interval", type=float, default=None,
                   help="请求间隔秒数（--loop 默认 1，--all 默认 0.1）")
    p.add_argument("--count", type=int, default=0, help="轮询次数，0=无限（默认 0）")
    p.add_argument("--timeout", type=float, default=3.0, help="单条命令响应超时秒数（默认 3）")
    p.add_argument("--all", action="store_true",
                   help="查支持位图 → 把全部支持 PID 轮一遍（mode 01+09）→ 写 CSV")
    p.add_argument("--csv", help="--all 的输出 CSV 路径（默认 obd_dump_时间戳.csv）")
    p.add_argument("--raw", action="store_true", help="逐包打印收到的原始字节（十六进制）")
    args = p.parse_args()
    _load_bleak()

    if args.cmd and args.loop:
        p.error("--cmd 与 --loop 二选一")
    if args.all and (args.cmd or args.loop):
        p.error("--all 与 --cmd/--loop 二选一")

    if args.scan:
        rows = await scan_rows(args.scan_timeout)
        if not rows:
            sys.exit("没扫到任何 BLE 设备。")
        print_rows(rows)
        return

    address, name = await pick_device(args)
    async with BleakClient(address, timeout=10.0) as client:
        label, w_char, n_char = find_chars(client)
        if w_char is None:
            sys.exit("适配器上没找到可用的 写/通知 特征对，把服务列表发到 issue：\n"
                     + "\n".join(f"  {s.uuid}: {[c.uuid for c in s.characteristics]}"
                                 for s in client.services))
        print(f"已连接 {name} [{address}]")
        print(f"服务 {label}: 写 {w_char.uuid} / 通知 {n_char.uuid}")

        sess = ElmSession(raw=args.raw)
        try:
            await client.start_notify(n_char, sess.on_notify)
        except Exception as exc:
            sys.exit(f"订阅通知失败（{n_char.uuid}）: {exc}")

        if args.init:
            await run_cmd_list(client, w_char, sess, args.init.split(";"), args.timeout)

        if args.all:
            interval = args.interval if args.interval is not None else 0.1
            results = {}
            print("== 查询 mode 01 支持位图 ==")
            sup01 = await discover_supported(client, w_char, sess, 0x01,
                                             args.timeout, interval, results)
            if not sup01:
                sys.exit("mode 01 无有效应答。检查连接/协议：可 --cmd 'ATDP' 看当前协议，"
                         "或换 --init（如 ATSP6）。")
            print("== 查询 mode 09 支持位图（车辆信息）==")
            sup09 = await discover_supported(client, w_char, sess, 0x09,
                                             args.timeout, interval, results)
            print(f"== 轮询全部支持 PID（间隔 {interval}s）==")
            ans01 = await sweep_pids(client, w_char, sess, 0x01, sup01,
                                     args.timeout, interval, results)
            ans09 = {}
            if sup09:
                ans09 = await sweep_pids(client, w_char, sess, 0x09, sup09,
                                         args.timeout, interval, results)
            miss01 = sorted(p for p in sup01 if p % 0x20 and (1, p) not in ans01)
            miss09 = sorted(p for p in sup09 if p % 0x20 and (9, p) not in ans09)
            print(f"mode 01: 支持 {len(sup01)}，有效应答 {len(ans01)}"
                  + (f"，无应答: {' '.join(f'{p:02X}' for p in miss01)}" if miss01 else ""))
            if sup09:
                print(f"mode 09: 支持 {len(sup09)}，有效应答 {len(ans09)}"
                      + (f"，无应答: {' '.join(f'{p:02X}' for p in miss09)}" if miss09 else ""))
            path = args.csv or datetime.now().strftime("obd_dump_%Y%m%d_%H%M%S.csv")
            rows = write_dump_csv(path, results, ans01 | ans09)
            print(f"已写出 {rows} 行 → {path}")
        elif args.cmd:
            await run_cmd_list(client, w_char, sess, args.cmd.split(";"), args.timeout)
        elif args.loop:
            interval = args.interval if args.interval is not None else 1.0
            print(f"轮询 {args.loop}，间隔 {interval}s，Ctrl-C 停")
            n = 0
            while args.count == 0 or n < args.count:
                try:
                    await send_command(client, w_char, sess, args.loop, args.timeout)
                except Exception as exc:
                    print(f"发送失败: {exc}")
                    return
                n += 1
                await asyncio.sleep(interval)
        else:
            await interactive(client, w_char, sess, args)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n退出")
