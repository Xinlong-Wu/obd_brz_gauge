#!/usr/bin/env python3
"""Generate the factory bootmedia image in the raw layout the firmware reads.

Layout (must match main/app_obd_dsp/boot_media_mount.c):
  0x0000  manifest text (boot_block.txt), zero-padded to 0x1000
  0x1000  frame stream (boot_block.bin), raw bytes

The firmware boots the animation only if the manifest is parseable, so this
script fails the build on a broken manifest instead of shipping a silent
no-animation image. ``binary_size`` is injected when missing — without it the
player falls back to probing the whole partition and allocates ~6 MB of PSRAM
(boot_block_player.c load_stream), which does not fit the WS128's 2 MB.

Usage: gen_bootmedia.py SLOT_DIR OUT_IMG [--partition-size N]
"""

import argparse
import sys
from pathlib import Path

MANIFEST_SLOT_SIZE = 0x1000
REQUIRED_KEYS = ("canvas_width", "canvas_height", "grid_width", "grid_height",
                 "fps", "frame_count")


def fail(msg: str) -> None:
    print(f"gen_bootmedia: error: {msg}", file=sys.stderr)
    sys.exit(1)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("slot_dir", type=Path, help="bootmedia/slot_a source dir")
    ap.add_argument("out_img", type=Path, help="output image path")
    ap.add_argument("--partition-size", type=lambda s: int(s, 0),
                    default=0x5E0000, help="bootmedia partition size")
    args = ap.parse_args()

    manifest_path = args.slot_dir / "boot_block.txt"
    bin_path = args.slot_dir / "boot_block.bin"
    for p in (manifest_path, bin_path):
        if not p.is_file():
            fail(f"missing {p}")

    manifest_text = manifest_path.read_text()
    if not manifest_text.endswith("\n"):
        manifest_text += "\n"

    keys = {}
    for line in manifest_text.splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            keys[k.strip()] = v.strip()
    missing = [k for k in REQUIRED_KEYS if not keys.get(k)]
    if missing:
        fail(f"manifest missing required keys: {', '.join(missing)}")
    if "binary_size" not in keys:
        manifest_text += "binary_size={}\n".format(bin_path.stat().st_size)

    manifest_bytes = manifest_text.encode()
    if len(manifest_bytes) > MANIFEST_SLOT_SIZE:
        fail(f"manifest is {len(manifest_bytes)} bytes, exceeds the "
             f"{MANIFEST_SLOT_SIZE}-byte slot")
    # manifest_present() probes for a lowercase leading key character
    if not (ord("a") <= manifest_bytes[0] <= ord("z")):
        fail("manifest must start with a lowercase key (e.g. canvas_width=)")

    bin_bytes = bin_path.read_bytes()
    total = MANIFEST_SLOT_SIZE + len(bin_bytes)
    if total > args.partition_size:
        fail(f"image {total} bytes exceeds partition {args.partition_size}")

    args.out_img.write_bytes(manifest_bytes.ljust(MANIFEST_SLOT_SIZE, b"\x00")
                             + bin_bytes)
    print(f"gen_bootmedia: {args.out_img} = manifest {len(manifest_bytes)} B "
          f"(padded to {MANIFEST_SLOT_SIZE:#x}) + stream {len(bin_bytes)} B, "
          f"total {total:#x} / partition {args.partition_size:#x}")


if __name__ == "__main__":
    main()
