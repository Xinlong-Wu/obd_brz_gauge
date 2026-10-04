#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""sim_regress.py — 模拟器截图回归(M0 测试地基)。

以固定 seed + 虚拟时钟无头跑一组场景,把产出的 BMP 与金图(PNG,入库于
tests/goldens/)逐像素对比;超阈值的像素占比大于上限即失败,退出码非 0。
CI 里由 .github/workflows/ci.yml 的 unit-sim job 调用。

用法:
  python3 tools/sim_regress.py                        # 跑回归
  python3 tools/sim_regress.py --update-goldens       # 重新生成金图(改 UI 后)
  python3 tools/sim_regress.py --filter tour          # 只跑名字含 tour 的场景
  python3 tools/sim_regress.py --bin simulator/build/obd_gauge_sim

依赖:仅 Pillow(对比用);模拟器二进制需已构建(SDL2 headless 即可)。
"""

import argparse
import os
import shutil
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# 每个场景:sim 命令行 + 期望产出的截图文件名列表。
# 公共参数:固定 seed、虚拟时钟、无面板、跳过开机动画 → 完全确定性。
COMMON = ["--no-boot", "--no-panel", "--seed", "42", "--clock", "virtual"]
SCENARIOS = [
    # 默认页(TEMP)在点火/热机阶段的默认外观
    ("boot_default", COMMON + ["--bound", "--frames", "500",
                               "--screenshot", "{out}/boot_default.bmp"],
     ["boot_default.bmp"]),
    # 三个编译期主题槽位
    ("theme_slot_amber", COMMON + ["--bound", "--theme-slot", "1", "--frames", "500",
                                   "--screenshot", "{out}/theme_slot_amber.bmp"],
     ["theme_slot_amber.bmp"]),
    ("theme_slot_ocean", COMMON + ["--bound", "--theme-slot", "2", "--frames", "500",
                                   "--screenshot", "{out}/theme_slot_ocean.bmp"],
     ["theme_slot_ocean.bmp"]),
    # 运行时主题(theme.bin 伪分区)
    ("theme_runtime_boost_oil", COMMON + ["--bound",
                                          "--theme", "theme_store/boost_oil_example/theme.bin",
                                          "--frames", "500",
                                          "--screenshot", "{out}/theme_runtime_boost_oil.bmp"],
     ["theme_runtime_boost_oil.bmp"]),
    # 默认开机路径:BLE 扫描页(2 台假适配器已出现)
    ("ble_scan_page", COMMON + ["--frames", "700",
                                "--screenshot", "{out}/ble_scan_page.bmp"],
     ["ble_scan_page.bmp"]),
    # 从表配对入口(FIND MASTER 列表)
    ("slave_role", COMMON + ["--role", "slave", "--frames", "700",
                             "--screenshot", "{out}/slave_role.bmp"],
     ["slave_role.bmp"]),
    # v2 组件编排主题(先打包 themes/example_v2_component 再预览)
    ("theme_v2_component", COMMON + ["--bound",
                                     "--theme", "{workdir}/v2demo.bin",
                                     "--frames", "600",
                                     "--screenshot", "{out}/theme_v2_component.bmp"],
     ["theme_v2_component.bmp"]),
    # 整环巡览:8 次交替滑动,每页一张
    ("tour", COMMON + ["--bound", "--frames", "0", "--tour", "8",
                       "--shots-dir", "{out}"],
     ["tour_%03d.bmp" % i for i in range(8)]),
    # 脚本化点击:点进设置页滚轮(交互路径冒烟)
    ("tap_settings", COMMON + ["--bound", "--tap", "180,180,400", "--frames", "900",
                               "--screenshot", "{out}/tap_settings.bmp"],
     ["tap_settings.bmp"]),
]

# 对比阈值:单像素任一通道差 > PER_PIXEL_TOL 记为坏点;
# 坏点占比 > MAX_BAD_FRACTION 判失败。留 0.5% 余量给反锯齿边缘。
PER_PIXEL_TOL = 16
MAX_BAD_FRACTION = 0.005


def run_scenario(bin_path, name, args, workdir):
    """跑一个场景,返回 (成功?, 产出说明)。"""
    argv = [bin_path] + [a.replace("{out}", workdir).replace("{workdir}", os.path.dirname(workdir)) for a in args]
    env = dict(os.environ)
    env["SDL_VIDEODRIVER"] = "dummy"
    proc = subprocess.run(argv, cwd=REPO_ROOT, env=env,
                          stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                          timeout=600)
    if proc.returncode != 0:
        print("  [FAIL] %s: sim exited %d\n%s" % (name, proc.returncode,
                                                  proc.stderr.decode(errors="replace")[-500:]))
        return False
    return True


def compare_bmp_png(ref_png, out_bmp, diff_png):
    """BMP vs 金图 PNG 对比。返回 (成功?, 坏点占比)。"""
    from PIL import Image, ImageChops

    ref = Image.open(ref_png).convert("RGB")
    out = Image.open(out_bmp).convert("RGB")
    if ref.size != out.size:
        print("  [FAIL] size mismatch: golden %s vs output %s" % (ref.size, out.size))
        return False, 1.0

    diff = ImageChops.difference(ref, out)
    # 任一通道差超阈值的像素 → 白点;convert("L") 取 max?L 是加权和——改用
    # 逐通道阈值后再合并更精确,但对"是否有显著差异"而言灰度直方图足够。
    bad = diff.point(lambda v: 255 if v > PER_PIXEL_TOL else 0).convert("L")
    hist = bad.histogram()
    bad_px = sum(hist[255:])
    total = ref.size[0] * ref.size[1]
    frac = bad_px / total

    if frac > MAX_BAD_FRACTION and diff_png:
        # 保存热力差图(超阈值像素标红)供 CI artifact 排查
        overlay = out.copy()
        from PIL import ImageDraw
        mask = bad.point(lambda v: 255 if v >= 255 else 0)
        red = Image.new("RGB", ref.size, (255, 0, 0))
        overlay.paste(red, (0, 0), mask)
        overlay.save(diff_png)
    return frac <= MAX_BAD_FRACTION, frac


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", default=os.path.join("simulator", "build", "obd_gauge_sim"),
                    help="模拟器二进制路径(相对仓库根或绝对)")
    ap.add_argument("--goldens", default=os.path.join("tests", "goldens"),
                    help="金图目录")
    ap.add_argument("--workdir", default="/tmp/obd_sim_regress",
                    help="本次运行的输出目录")
    ap.add_argument("--diffdir", default="/tmp/obd_sim_regress_diff",
                    help="失败差图输出目录(CI artifact 用)")
    ap.add_argument("--filter", default="",
                    help="只跑名字包含该子串的场景")
    ap.add_argument("--update-goldens", action="store_true",
                    help="用本次输出刷新金图(UI 有意变更后使用)")
    args = ap.parse_args()

    bin_path = args.bin if os.path.isabs(args.bin) else os.path.join(REPO_ROOT, args.bin)
    if not os.path.exists(bin_path):
        print("sim binary not found: %s(先 cmake --build simulator/build)" % bin_path)
        return 2

    scenarios = [(n, a, f) for (n, a, f) in SCENARIOS if args.filter in n]
    if not scenarios:
        print("no scenario matches filter: %s" % args.filter)
        return 2

    if os.path.isdir(args.workdir):
        shutil.rmtree(args.workdir)
    os.makedirs(args.workdir)

    # v2 组件主题场景依赖打包产物:先离线打包(与固件同一 packer)
    v2bin = os.path.join(args.workdir, "v2demo.bin")
    pack = subprocess.run(
        [sys.executable,
         os.path.join(REPO_ROOT, "tools", "theme_packer", "pack_theme.py"),
         os.path.join(REPO_ROOT, "themes", "example_v2_component"), v2bin],
        cwd=REPO_ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if pack.returncode != 0:
        print("theme packer failed:\n%s" % pack.stderr.decode(errors="replace")[-400:])
        return 2
    if args.update_goldens:
        os.makedirs(args.goldens, exist_ok=True)

    failed = 0
    for name, argv, files in scenarios:
        print("scenario: %s" % name)
        out_dir = os.path.join(args.workdir, name)
        os.makedirs(out_dir, exist_ok=True)
        if not run_scenario(bin_path, name, argv, out_dir):
            failed += 1
            continue

        if args.update_goldens:
            golden_dir = os.path.join(args.goldens, name)
            from PIL import Image
            os.makedirs(golden_dir, exist_ok=True)
            for f in files:
                src = os.path.join(out_dir, f)
                if not os.path.exists(src):
                    print("  [FAIL] missing output %s" % f)
                    failed += 1
                    continue
                Image.open(src).save(os.path.join(golden_dir, f + ".png"))
            print("  updated %d goldens" % len(files))
            continue

        for f in files:
            golden = os.path.join(args.goldens, name, f + ".png")
            out_bmp = os.path.join(out_dir, f)
            if not os.path.exists(golden):
                print("  [FAIL] golden missing: %s(--update-goldens 生成)" % golden)
                failed += 1
                continue
            if not os.path.exists(out_bmp):
                print("  [FAIL] output missing: %s" % f)
                failed += 1
                continue
            os.makedirs(args.diffdir, exist_ok=True)
            ok, frac = compare_bmp_png(golden, out_bmp,
                                       os.path.join(args.diffdir, "%s__%s.png" % (name, f)))
            status = "ok" if ok else "FAIL"
            print("  [%s] %s (bad-pixel %.4f%%, limit %.2f%%)"
                  % (status, f, frac * 100.0, MAX_BAD_FRACTION * 100.0))
            if not ok:
                failed += 1

    if args.update_goldens:
        print("goldens updated. Remember to commit tests/goldens/.")
        return 0
    print("\n%d/%d scenarios failed" % (failed, len(scenarios)))
    if failed:
        print("diff overlays (if any): %s" % args.diffdir)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
