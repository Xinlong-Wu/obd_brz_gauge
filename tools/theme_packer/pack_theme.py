#!/usr/bin/env python3
"""
Theme Packer - Convert theme assets into binary partition image

Usage:
    python3 pack_theme.py <theme_dir> <output_bin>

Example:
    python3 pack_theme.py themes/boost_oil firmware/theme_boost_oil.bin

Theme directory structure:
    theme_dir/
    ├── theme_manifest.json  (required)
    ├── assets/
    │   ├── dial.png         (optional, 360x360)
    │   └── ring.png         (optional, 360x360 RGBA)
    └── layout.json          (optional, custom page layout)
"""

import json
import struct
import sys
from pathlib import Path
from typing import Dict, Optional

try:
    from PIL import Image
    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False
    print("Warning: Pillow not installed, image conversion will be skipped")
    print("Install with: pip install Pillow")

PARTITION_SIZE = 4 * 1024 * 1024  # 4MB, must match theme_0 in partitions.csv
MANIFEST_RESERVED_SIZE = 16 * 1024  # First 16KB reserved for manifest JSON (must match THEME_MANIFEST_MAX_SIZE in theme_loader.c)


def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    """Convert RGB888 to RGB565 format"""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def pack_image_rgb565(img_path: Path, offset: int, data: bytearray) -> int:
    """Pack RGB image as RGB565 (dial/ring stay 360x360; named assets any size)"""
    if not PIL_AVAILABLE:
        print(f"Skipping {img_path} (Pillow not installed)")
        return 0

    img = Image.open(img_path).convert('RGB')
    w, h = img.size
    if img_path.name in ("dial.png", "ring.png") and (w, h) != (360, 360):
        raise ValueError(f"Image {img_path} must be 360x360, got {img.size}")

    print(f"  Packing {img_path.name} as RGB565 ({w}x{h})...")
    pixels = img.load()

    for y in range(h):
        for x in range(w):
            r, g, b = pixels[x, y]
            rgb565 = rgb888_to_rgb565(r, g, b)
            pos = offset + (y * w + x) * 2
            struct.pack_into('<H', data, pos, rgb565)

    size = w * h * 2
    print(f"    Packed {size} bytes at offset 0x{offset:06X}")
    return size


def pack_image_rgba8888(img_path: Path, offset: int, data: bytearray) -> int:
    """Pack RGBA image as RGBA8888 (dial/ring stay 360x360; named assets any size)"""
    if not PIL_AVAILABLE:
        print(f"Skipping {img_path} (Pillow not installed)")
        return 0

    img = Image.open(img_path).convert('RGBA')
    w, h = img.size
    if img_path.name in ("dial.png", "ring.png") and (w, h) != (360, 360):
        raise ValueError(f"Image {img_path} must be 360x360, got {img.size}")

    print(f"  Packing {img_path.name} as RGBA8888 ({w}x{h})...")
    pixels = img.load()

    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            pos = offset + (y * w + x) * 4
            struct.pack_into('BBBB', data, pos, r, g, b, a)

    size = w * h * 4
    print(f"    Packed {size} bytes at offset 0x{offset:06X}")
    return size


def pack_raw_bytes(raw_path: Path, offset: int, data: bytearray) -> int:
    """Pack a raw binary asset verbatim (e.g. lv_font_bin)."""
    raw = raw_path.read_bytes()
    data[offset:offset + len(raw)] = raw
    print(f"  Packing {raw_path.name} verbatim ({len(raw)} bytes)")
    return len(raw)


def pack_layout_json(layout_path: Path, offset: int, data: bytearray) -> int:
    """Pack layout JSON (optional custom page layout)"""
    if not layout_path.exists():
        return 0

    print(f"  Packing {layout_path.name}...")
    layout_bytes = layout_path.read_bytes()

    if len(layout_bytes) > 64 * 1024:
        raise ValueError(
            f"Layout JSON too large: {len(layout_bytes)} bytes (max 64KB)")

    data[offset:offset + len(layout_bytes)] = layout_bytes
    print(f"    Packed {len(layout_bytes)} bytes at offset 0x{offset:06X}")
    return len(layout_bytes)


def validate_manifest(manifest: Dict) -> None:
    """Validate theme manifest structure"""
    required_top = ["schema_version", "theme", "colors"]
    for key in required_top:
        if key not in manifest:
            raise ValueError(f"Missing required field: {key}")

    theme = manifest["theme"]
    required_theme = ["id", "name", "version", "author"]
    for key in required_theme:
        if key not in theme:
            raise ValueError(f"Missing required field in theme: {key}")

    colors = manifest["colors"]
    required_colors = [
        "bg", "ring", "arc_track", "arc_indicator",
        "text_primary", "text_secondary", "needle", "panel"
    ]
    for key in required_colors:
        if key not in colors:
            raise ValueError(f"Missing required color: {key}")


def pack_theme(theme_dir: Path, output_bin: Path) -> None:
    """Main packer function"""
    print(f"Packing theme from: {theme_dir}")
    print(f"Output: {output_bin}")
    print("=" * 60)

    # Initialize partition data (all 0xFF)
    data = bytearray([0xFF] * PARTITION_SIZE)

    # Load and validate manifest
    manifest_path = theme_dir / "theme_manifest.json"
    if not manifest_path.exists():
        raise FileNotFoundError(f"Manifest not found: {manifest_path}")

    manifest = json.load(open(manifest_path, 'r'))
    validate_manifest(manifest)

    theme_meta = manifest['theme']
    print(f"\nTheme: {theme_meta['name']} v{theme_meta['version']}")
    print(f"  ID: {manifest['theme']['id']}")
    print(f"  Author: {manifest['theme']['author']}")

    # Track asset offsets
    current_offset = MANIFEST_RESERVED_SIZE  # Start after manifest area
    assets_info = {}

    # Pack dial background (360x360 RGB565)
    dial_path = theme_dir / "assets" / "dial.png"
    if dial_path.exists():
        size = pack_image_rgb565(dial_path, current_offset, data)
        if size > 0:
            assets_info["dial_background"] = {
                "offset": current_offset,
                "size": size,
                "format": "rgb565",
                "width": 360,
                "height": 360
            }
            current_offset += size
    else:
        print(f"\nWarning: dial.png not found, skipping")

    # Pack ring overlay (360x360 RGBA8888)
    ring_path = theme_dir / "assets" / "ring.png"
    if ring_path.exists():
        size = pack_image_rgba8888(ring_path, current_offset, data)
        if size > 0:
            assets_info["ring_overlay"] = {
                "offset": current_offset,
                "size": size,
                "format": "rgba8888",
                "width": 360,
                "height": 360
            }
            current_offset += size
    else:
        print(f"\nWarning: ring.png not found, skipping")

    # ---- Named assets (M3.4): anything in assets/ besides dial/ring ----
    #   foo.png        -> name "foo", rgb565 (any size)
    #   foo.rgba.png   -> name "foo", rgba8888 (any size)
    #   foo.lv_font_bin-> name "foo", lv_font_bin (verbatim)
    if (theme_dir / "assets").is_dir():
        for f in sorted((theme_dir / "assets").iterdir()):
            if not f.is_file():
                continue
            if f.name in ("dial.png", "ring.png"):
                continue   # legacy pair, packed above
            if f.name.endswith(".rgba.png"):
                name, fmt, packer = f.name[:-len(".rgba.png")], "rgba8888", pack_image_rgba8888
                meta_w = meta_h = None
            elif f.name.endswith(".png"):
                name, fmt, packer = f.name[:-len(".png")], "rgb565", pack_image_rgb565
                meta_w = meta_h = None
            elif f.name.endswith(".lv_font_bin"):
                name, fmt, packer = f.name[:-len(".lv_font_bin")], "lv_font_bin", pack_raw_bytes
                meta_w = meta_h = None
            else:
                print(f"  Skipping unrecognized asset {f.name}")
                continue
            if name in assets_info:
                raise ValueError(f"Duplicate asset name '{name}'")

            size = packer(f, current_offset, data)
            if size <= 0:
                continue
            entry = {"offset": current_offset, "size": size, "format": fmt}
            if PIL_AVAILABLE and fmt != "lv_font_bin":
                with Image.open(f) as im:
                    entry["width"], entry["height"] = im.size
            assets_info[name] = entry
            current_offset += size

    # ---- components.json (M3.4): theme-defined components, embedded verbatim ----
    components_path = theme_dir / "components.json"
    if components_path.exists():
        components = json.load(open(components_path, 'r'))
        if not isinstance(components, dict):
            raise ValueError("components.json must be a JSON object")
        manifest["components"] = components

    # ---- Multi-page layouts (M3.4): layouts/<page_id>.json, one entry each ----
    # The legacy single layout.json keeps working (page id "main_gauge").
    layout_files = []
    legacy = theme_dir / "layout.json"
    if legacy.exists():
        layout_files.append(("main_gauge", legacy))
    layouts_dir = theme_dir / "layouts"
    if layouts_dir.is_dir():
        for f in sorted(layouts_dir.glob("*.json")):
            layout_files.append((f.stem, f))

    if layout_files:
        if "pages" not in manifest:
            manifest["pages"] = {}
        theme_pages = manifest["pages"].setdefault("theme_pages", [])

        for page_id, path in layout_files:
            size = pack_layout_json(path, current_offset, data)
            if size <= 0:
                continue
            entry = next((e for e in theme_pages if e.get("id") == page_id), None)
            if entry is None:
                entry = {"id": page_id, "type": "custom_layout"}
                theme_pages.append(entry)
            entry["layout_data_offset"] = current_offset
            entry["layout_data_size"] = size
            current_offset += size

    # v2 features used -> bump schema so old firmware rejects loudly instead
    # of silently ignoring components/instances
    if manifest.get("schema_version", "1.0") == "1.0" and (
            "components" in manifest or
            any("instances" in json.loads((theme_dir / "layout.json").read_text())
                for _ in [0] if (theme_dir / "layout.json").exists()) or
            any("instances" in json.loads(f.read_text())
                for pid, f in layout_files)):
        manifest["schema_version"] = "2.0"

    # Add assets section to manifest
    if assets_info:
        manifest["assets"] = assets_info

    # Write final manifest to partition (first 16KB, see MANIFEST_RESERVED_SIZE)
    manifest_bytes = json.dumps(manifest, indent=2).encode('utf-8')
    if len(manifest_bytes) >= MANIFEST_RESERVED_SIZE:
        raise ValueError(
            f"Manifest too large: {len(manifest_bytes)} bytes (max {MANIFEST_RESERVED_SIZE})")

    data[0:len(manifest_bytes)] = manifest_bytes

    # Write output file
    output_bin.parent.mkdir(parents=True, exist_ok=True)
    output_bin.write_bytes(data)

    # Print summary
    print("\n" + "=" * 60)
    print("Packing complete!")
    print(f"  Output file: {output_bin}")
    print(
        f"  Total size: {len(data)} bytes ({len(data) / (1024*1024):.2f} MB)")
    print(f"  Manifest: {len(manifest_bytes)} bytes")
    print(f"  Assets: {len(assets_info)} items")
    print(
        f"  Used: {current_offset} bytes ({current_offset / (1024*1024):.2f} MB)")
    free_bytes = PARTITION_SIZE - current_offset
    print(f"  Free: {free_bytes} bytes ({free_bytes / (1024*1024):.2f} MB)")


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)

    theme_dir = Path(sys.argv[1])
    output_bin = Path(sys.argv[2])

    if not theme_dir.is_dir():
        print(f"Error: Theme directory not found: {theme_dir}")
        sys.exit(1)

    try:
        pack_theme(theme_dir, output_bin)
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
