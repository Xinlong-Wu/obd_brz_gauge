#!/usr/bin/env python3
"""gen_capture_page.py — 取图控制页构建管线(压缩 + gzip,零第三方依赖)。

源:assets_src/web/capture.html(人类可读、可编辑、带语法高亮)
产物:压缩并 gzip 的单文件,由 main/CMakeLists.txt 经 EMBED_FILES 链入
固件,serving 时带 Content-Encoding: gzip 由浏览器解压。

压缩只做保守变换(去 HTML 注释、去行首尾空白、去空行)——不做可能破坏
内联 JS 的激进变换(如跨行合并);页面体积本就 ~2KB,可读性优先。

用法:python3 tools/gen_capture_page.py <in.html> <out.gz>
"""
import gzip
import re
import sys
from pathlib import Path

# 从脚本位置推算仓库根(IDF 的 requirements 阶段会以构建目录为上下文重复
# 执行本文件,CMake 传入的路径在该上下文不可信——gen_themes 同款约定)
ROOT = Path(__file__).resolve().parent.parent
DEFAULT_IN = ROOT / "assets_src" / "web" / "capture.html"
DEFAULT_OUT = ROOT / "main" / "app_obd_dsp" / "capture_page.html.gz"


def minify(text: str) -> str:
    # HTML 注释(页面不使用条件注释,安全)
    text = re.sub(r"<!--.*?-->", "", text, flags=re.S)
    lines = []
    for line in text.splitlines():
        line = line.strip()
        if line:
            lines.append(line)
    return "\n".join(lines) + "\n"


def main() -> int:
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_IN
    dst = Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_OUT
    data = minify(src.read_text(encoding="utf-8")).encode("utf-8")
    # mtime=0 → 输出确定性,源不变则产物逐字节不变(重复执行幂等)
    gz = gzip.compress(data, compresslevel=9, mtime=0)
    dst.write_bytes(gz)
    print(f"gen_capture_page: {len(data)} B -> {len(gz)} B gzipped ({dst.name})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
