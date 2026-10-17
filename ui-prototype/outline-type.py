from pathlib import Path
import argparse, json, os, shutil, xml.etree.ElementTree as ET
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
ROOT=Path(__file__).parent
parser=argparse.ArgumentParser(description="Outline exported SVG text using local fonts.")
parser.add_argument("--cjk-font",type=Path,default=os.environ.get("UI_CJK_FONT"))
args=parser.parse_args()
candidates=[Path(args.cjk_font)] if args.cjk_font else [
 Path("/System/Library/Fonts/Hiragino Sans GB.ttc"),
 Path("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"),
 Path("/usr/share/fonts/truetype/noto/NotoSansSC-Regular.ttf"),
 Path("C:/Windows/Fonts/msyh.ttc")]
cjk_font=next((p for p in candidates if p.is_file()),None)
if cjk_font is None:
 parser.error("A CJK font is required; pass --cjk-font PATH or set UI_CJK_FONT.")
NS="http://www.w3.org/2000/svg"
ET.register_namespace("", NS)
foundations=json.loads((ROOT/"foundations.json").read_text(encoding="utf-8"))
FONT={(f["family"],f["weight"]):TTFont(ROOT/f["file"]) for f in foundations["fonts"]}
FONT[(foundations["typography"]["body"],500)]=TTFont(cjk_font,fontNumber=0)
DATA={k:(f.getGlyphSet(),f.getBestCmap(),f["head"].unitsPerEm,f["hmtx"].metrics) for k,f in FONT.items()}
def outline(file):
 tree=ET.parse(file)
 for parent in list(tree.iter()):
  for node in list(parent):
   if node.tag==f"{{{NS}}}style":
    parent.remove(node)
   if node.tag!=f"{{{NS}}}text": continue
   chars="".join(node.itertext())
   family=(node.get("font-family","sans-serif"),int(node.get("font-weight","500")))
   if family not in DATA: family=("sans-serif",500)
   size=float(node.get("font-size","26"))
   x=float(node.get("x","0"));y=float(node.get("y","0"))
   spacing=float(node.get("letter-spacing","0"))
   baseline=y+size*.8 if node.get("dominant-baseline")=="hanging" else y
   glyphs=[]
   for char in chars:
    k=family
    if ord(char) not in DATA[k][1]: k=("sans-serif",500)
    gs, cmap, upem, metrics=DATA[k]
    gname=cmap.get(ord(char),".notdef")
    glyphs.append((gs,gname,size/upem,metrics[gname][0]*size/upem))
   width=sum(g[3] for g in glyphs)+max(0,len(glyphs)-1)*spacing
   anchor=node.get("text-anchor","start")
   cursor=x-width/2 if anchor=="middle" else x-width if anchor=="end" else x
   group=ET.Element(f"{{{NS}}}g",{"aria-label":chars,"fill":node.get("fill","#F3F0E7")})
   if node.get("data-type-role"): group.set("data-type-role",node.get("data-type-role"))
   for gs,gname,scale,advance in glyphs:
    pen=SVGPathPen(gs)
    gs[gname].draw(TransformPen(pen,(scale,0,0,-scale,cursor,baseline)))
    d=pen.getCommands()
    if d: ET.SubElement(group,f"{{{NS}}}path",{"d":d})
    cursor+=advance+spacing
   idx=list(parent).index(node)
   parent.remove(node);parent.insert(idx,group)
 tree.write(file,encoding="unicode")
textdir=ROOT/"vectors-text";textdir.mkdir(exist_ok=True)
for f in (ROOT/"vectors").glob("*.svg"):
 if b"<text" in f.read_bytes():
  shutil.copyfile(f,textdir/f.name);outline(f)
for name in ["overview", "overview-alerts"]:
 poster=ROOT/(name+".svg")
 if b"<text" in poster.read_bytes():
  shutil.copyfile(poster,ROOT/(name+"-text.svg"));outline(poster)
print("Outlined the original typefaces for portable vector rendering.")
