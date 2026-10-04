#!/usr/bin/env python3
"""Migrate geometry literals to 720-master UIS() calls (P1).

Replaces pure-integer pixel arguments of geometry LVGL calls with
``UIS(<value * 2>)`` (720-master). Only geometry is touched — angles,
opacities, delays, counts, zoom factors are never wrapped. Call sites
whose target argument is an expression/identifier are reported for
manual migration.

Run from the repo root:
    python3 tools/migrate_ui_literals.py            # apply
    python3 tools/migrate_ui_literals.py --dry-run  # report only
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# function name -> set of 0-based argument indices (after the object arg) that
# carry geometry pixels
SCALE_FUNCS = {
    "lv_obj_set_size": {1, 2},
    "lv_obj_set_width": {1},
    "lv_obj_set_height": {1},
    "lv_obj_set_pos": {1, 2},
    "lv_obj_set_x": {1},
    "lv_obj_set_y": {1},
    # lv_obj_align: (obj, align) has no coords; (obj, align, x, y) scales last two
    "lv_obj_align": "last2",
    "lv_obj_align_to": {3, 4},
    "lv_obj_set_style_radius": {1},
    "lv_obj_set_style_pad_all": {1},
    "lv_obj_set_style_pad_top": {1},
    "lv_obj_set_style_pad_bottom": {1},
    "lv_obj_set_style_pad_left": {1},
    "lv_obj_set_style_pad_right": {1},
    "lv_obj_set_style_pad_hor": {1},
    "lv_obj_set_style_pad_ver": {1},
    "lv_obj_set_style_border_width": {1},
    "lv_obj_set_style_outline_width": {1},
    "lv_obj_set_style_arc_width": {1},
    "lv_obj_set_style_line_width": {1},
    "lv_obj_set_style_text_letter_space": {1},
    "lv_obj_set_style_text_line_space": {1},
}

# never scaled — reported for awareness only (units are not pixels)
EXCLUDED_FUNCS = {
    "lv_img_set_zoom": "zoom units (256 = 1x), not pixels",
    "lv_chart_set_div_line_count": "grid counts, not pixels",
    "lv_arc_set_bg_angles": "angles",
    "lv_arc_set_angles": "angles",
    "lv_arc_set_start_angle": "angles",
    "lv_arc_set_end_angle": "angles",
    "lv_arc_set_rotation": "angles",
}

INT_RE = re.compile(r"^[-+]?\d+$")

TARGET_FILES = [
    "main/export_path/screens/*.c",
    "main/export_path/*.c",
    "main/theme_engine/theme_loader.c",
]


def split_args(span):
    """Split a balanced-paren argument span by top-level commas."""
    args, depth, cur, i = [], 0, [], 0
    while i < len(span):
        c = span[i]
        if c in "([{":
            depth += 1
            cur.append(c)
        elif c in ")]}":
            depth -= 1
            cur.append(c)
        elif c == "," and depth == 0:
            args.append("".join(cur))
            cur = []
        else:
            cur.append(c)
        i += 1
    args.append("".join(cur))
    return args


def find_matching_paren(text, open_idx):
    depth = 0
    for i in range(open_idx, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return i
    raise ValueError(f"unbalanced parens at {open_idx}")


def migrate_text(text, rel, report, apply):
    """One pass over the whole file; returns (new_text, n_replacements)."""
    names = sorted(set(SCALE_FUNCS) | set(EXCLUDED_FUNCS), key=len, reverse=True)
    pattern = re.compile(r"\b(" + "|".join(re.escape(n) for n in names) + r")\s*\(")
    out, pos, n_rep = [], 0, 0

    for m in list(pattern.finditer(text)):
        if m.start() < pos:
            continue  # inside an already-processed call
        name = m.group(1)
        open_idx = m.end() - 1
        close_idx = find_matching_paren(text, open_idx)
        span = text[open_idx + 1:close_idx]
        args = split_args(span)
        if name in EXCLUDED_FUNCS:
            report.append(f"[excluded] {rel}:{text[:m.start()].count(chr(10)) + 1} "
                          f"{name}({span[:70]}) — {EXCLUDED_FUNCS[name]}")
            continue
        indices = SCALE_FUNCS[name]
        if indices == "last2":
            if len(args) < 4:
                continue  # lv_obj_align(obj, align) form, no coords
            indices = {len(args) - 2, len(args) - 1}
        changed = False
        for idx in sorted(indices):
            if idx >= len(args):
                continue
            arg = args[idx].strip()
            if "UIS(" in arg:
                continue
            if INT_RE.match(arg):
                val = int(arg)
                if val == 0:
                    continue
                if apply:
                    args[idx] = args[idx].replace(arg, f"UIS({val * 2})", 1)
                changed = True
            else:
                report.append(f"[manual] {rel}:{text[:m.start()].count(chr(10)) + 1} "
                              f"{name}(...) arg{idx} = {arg} — expression, migrate by hand")
        if changed:
            n_rep += 1
            out.append((text[pos:m.start()], name, open_idx, close_idx, list(args)))
            pos = close_idx
    if not apply or n_rep == 0:
        return text, n_rep

    # rebuild with replaced spans (last write wins via segments)
    pieces, last = [], 0
    for prefix, name, open_idx, close_idx, args in out:
        pieces.append(text[last:open_idx + 1])
        pieces.append(",".join(args))
        last = close_idx
    pieces.append(text[last:])
    return "".join(pieces), n_rep


def add_include(text, rel):
    # quote-includes resolve against the includer's own directory first, so
    # relative paths work unchanged for firmware main, simulator and tests
    if "/screens/" in rel:
        header = "../ui_res.h"
    elif "/theme_engine/" in rel:
        header = "../export_path/ui_res.h"
    else:
        header = "ui_res.h"
    if header in text:
        return text
    lines = text.split("\n")
    last_inc = 0
    for i, line in enumerate(lines[:80]):
        if line.startswith("#include"):
            last_inc = i + 1
    if last_inc == 0:
        for i, line in enumerate(lines[:40]):
            if not line.startswith("//") and line.strip():
                last_inc = i
                break
    lines.insert(last_inc, f'#include "{header}"')
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    ns = ap.parse_args()

    report, total = [], 0
    for glob in TARGET_FILES:
        for path in sorted(ROOT.glob(glob)):
            rel = str(path.relative_to(ROOT))
            text = path.read_text()
            new, n = migrate_text(text, rel, report, apply=not ns.dry_run)
            if n and not ns.dry_run:
                new = add_include(new, rel)
                path.write_text(new)
            total += n
            if n:
                print(f"{rel}: {n} calls migrated")

    print(f"\n{'WOULD migrate' if ns.dry_run else 'migrated'} {total} calls total")
    if report:
        print(f"\n{len(report)} sites need manual attention:")
        for line in report:
            print("  " + line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
