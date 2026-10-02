"""Compose the air-chamber figure (16:9, 1760x990) on top of the generated illustration.

Base: images/protopedia/air-chambers-illustration-original.png (1672x941, no text).
The view shows the dinosaur's LEFT side (head to the left), so the right legs are
hidden behind; their chambers, holes and path segments are drawn dashed.
Misplaced holes in the generated picture are covered with clone patches.
Run: python tools/make_chamber_figure.py  (then rsvg-convert to PNG).
"""
import base64
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE = ROOT / "images" / "protopedia" / "air-chambers-illustration-original.png"
OUT = ROOT / "images" / "protopedia" / "air-chambers.svg"
IW, IH = 1672, 941

FONT = "Hiragino Sans, Hiragino Kaku Gothic ProN, Hiragino Kaku Gothic Pro, sans-serif"
INK, MUTE, LINE, PATH = "#1a202c", "#4a5568", "#2d3748", "#d53f8c"

# order from the sensor -> (name, fill, text color, hidden)
ORDER = [
    ("しっぽ", "#f6ad55", "#7b341e", False),
    ("右後ろ足", "#68d391", "#22543d", True),
    ("右前足", "#9ae6b4", "#22543d", True),
    ("左前足", "#faf089", "#744210", False),
    ("左後ろ足", "#fbd38d", "#7b341e", False),
    ("背中", "#63b3ed", "#2a4365", False),
    ("頭・首", "#d6bcfa", "#44337a", False),
]

s = []
add = s.append
add(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1760 990" width="1760" height="990" font-family="{FONT}">')
add('''<defs>
  <marker id="ah" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto"><path d="M0,0 L10,5 L0,10 z" fill="#2d3748"/></marker>
  <marker id="ah-path" viewBox="0 0 10 10" refX="7" refY="5" markerWidth="3.2" markerHeight="3.2" orient="auto"><path d="M0,0 L10,5 L0,10 z" fill="#d53f8c"/></marker>
</defs>''')
add('<rect width="1760" height="990" fill="#ffffff"/>')

# ------------------------------------------------------------------ title
add(f'<text x="50" y="68" font-size="40" font-weight="bold" fill="{INK}">7つの空気室は、どうつながっている？</text>')
add(f'<text x="50" y="110" font-size="22" fill="{MUTE}">部屋は直径2.5mmの穴で一列につながり、圧力センサーはしっぽの先に1個だけ</text>')
add('<text x="1710" y="68" font-size="20" fill="#718096" text-anchor="end">Created by Kanata the Kid Creator</text>')

# ------------------------------------------------------------------ left panel: illustration
add('<rect x="50" y="140" width="1070" height="800" rx="18" fill="#f7fafc" stroke="#cbd5e0" stroke-width="2"/>')
add(f'<text x="80" y="180" font-size="22" font-weight="bold" fill="{INK}">横から見た断面（頭が左、しっぽが右。見えているのは左側の足）</text>')

SCALE = 0.63
OX, OY = 58, 200
data = base64.b64encode(BASE.read_bytes()).decode()
add(f'<g transform="translate({OX},{OY}) scale({SCALE})">')
add(f'<defs><image id="base" x="0" y="0" width="{IW}" height="{IH}" href="data:image/png;base64,{data}"/></defs>')
add('<use href="#base"/>')

# clone patches over the misplaced holes: show source pixel (c + s) at c
patches = [
    ((383, 318), (-28, 26), 26),
    ((760, 639), (-34, -22), 25),
    ((936, 589), (-34, -12), 25),
    ((475, 693), (0, -46), 31),
    ((1021, 541), (-36, 16), 25),
]
for i, ((cx, cy), (sx, sy), r) in enumerate(patches):
    add(f'<clipPath id="p{i}"><circle cx="{cx}" cy="{cy}" r="{r}"/></clipPath>')
    add(f'<g clip-path="url(#p{i})"><use href="#base" x="{-sx}" y="{-sy}"/></g>')
# smooth the wall junction where a hole used to be (clone alone leaves a stub)
add('<circle cx="760" cy="641" r="12" fill="#d9d9d9" stroke="#c8c8c8" stroke-width="2"/>')
add('<circle cx="1021" cy="541" r="14" fill="#d9d9d9" stroke="#c4c4c4" stroke-width="3"/>')
# erase the drawn tube and sensor cube
add('<rect x="1527" y="780" width="150" height="140" fill="#ffffff"/>')

def hole(x, y, hidden=False):
    if hidden:
        add(f'<circle cx="{x}" cy="{y}" r="15" fill="#ffffff" fill-opacity="0.75" stroke="{LINE}" stroke-width="4" stroke-dasharray="6 5"/>')
    else:
        add(f'<circle cx="{x}" cy="{y}" r="15" fill="#ffffff" stroke="{LINE}" stroke-width="4"/>')
        add(f'<circle cx="{x}" cy="{y}" r="5" fill="{LINE}"/>')

# air path (pink): white halo, then solid / dashed segments, drawn before the holes
segs = [
    ("M1548,842 C1430,760 1250,610 1040,575 L1007,575", False),                  # sensor -> tail -> hidden hole
    ("M1007,575 C940,558 860,548 713,546", True),                               # right hind (hidden)
    ("M713,546 C640,520 560,515 466,532", True),                                # right front (hidden)
    ("M466,532 C520,600 580,615 641,575", False),                               # left front
    ("M641,575 C700,615 780,620 812,513", False),                               # left hind -> back
    ("M812,513 C700,440 520,400 356,356", False),                               # back -> neck wall
    ("M356,356 C300,300 250,230 215,175", False),                               # neck -> head
]
for d, hidden in segs:
    add(f'<path d="{d}" fill="none" stroke="#ffffff" stroke-width="13" stroke-linecap="round" stroke-opacity="0.85"/>')
for d, hidden in segs:
    dash = ' stroke-dasharray="14 12"' if hidden else ""
    end = ' marker-end="url(#ah-path)"' if d.startswith("M356") else ""
    add(f'<path d="{d}" fill="none" stroke="{PATH}" stroke-width="7" stroke-linecap="round"{dash}{end}/>')

# holes (6 walls)
hole(1007, 575, True)   # tail | right hind (far side)
hole(713, 546, True)    # right hind | right front (far side)
hole(466, 532, True)    # right front | left front (wall between left and right)
hole(641, 575)          # left front | left hind
hole(812, 513)          # left hind | back
hole(356, 356)          # back | neck

# sensor box at the tail tip
add(f'<line x1="1522" y1="818" x2="1560" y2="850" stroke="{LINE}" stroke-width="6" stroke-linecap="round"/>')
add(f'<rect x="1530" y="850" width="140" height="70" rx="10" fill="#2d3748"/>')
add('<text x="1600" y="882" font-size="22" font-weight="bold" fill="#ffffff" text-anchor="middle">圧力センサー</text>')
add('<text x="1600" y="908" font-size="18" fill="#e2e8f0" text-anchor="middle">1個だけ</text>')

def badge(x, y, n, r=24):
    add(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{INK}" stroke="#ffffff" stroke-width="4"/>')
    add(f'<text x="{x}" y="{y+9}" font-size="27" font-weight="bold" fill="#ffffff" text-anchor="middle">{n}</text>')

def label(x, y, text, anchor="middle"):
    add(f'<text x="{x}" y="{y}" font-size="26" font-weight="bold" fill="{INK}" stroke="#ffffff" stroke-width="6" paint-order="stroke" text-anchor="{anchor}">{text}</text>')

badge(1300, 700, 1); label(1300, 750, "しっぽ")
badge(1010, 790, 2); label(1045, 798, "右後ろ足（奥）", "start")
badge(560, 790, 3); label(595, 798, "右前足（奥）", "start")
badge(330, 560, 4); label(330, 610, "左前足")
badge(850, 660, 5); label(850, 710, "左後ろ足")
badge(560, 330, 6); label(560, 300, "背中")
badge(230, 290, 7); label(230, 340, "頭・首")
add('</g>')

add(f'<text x="80" y="880" font-size="15" fill="{MUTE}">● 数字 = センサーから数えた順番　<tspan fill="{PATH}" font-weight="bold">ピンクの線</tspan> = 空気の通り道　○ = 隔壁の穴（φ2.5mm）</text>')
add(f'<text x="80" y="906" font-size="15" fill="{MUTE}">点線 = 反対側（右側）にあって見えない部屋・穴・通り道</text>')

# ------------------------------------------------------------------ right panel: chain + why
PX, PW = 1150, 560
add(f'<rect x="{PX}" y="140" width="{PW}" height="800" rx="18" fill="#ffffff" stroke="#cbd5e0" stroke-width="2"/>')
add(f'<text x="{PX+30}" y="180" font-size="22" font-weight="bold" fill="{INK}">空気の通り道（センサーから順に）</text>')

cx = PX + PW / 2
y = 222
add(f'<rect x="{cx-120}" y="{y-20}" width="240" height="40" rx="8" fill="#2d3748"/>')
add(f'<text x="{cx}" y="{y+7}" font-size="18" font-weight="bold" fill="#ffffff" text-anchor="middle">圧力センサー</text>')
step = 60
for i, (name, fill, tc, hidden) in enumerate(ORDER):
    y0 = y + 20
    y += step
    add(f'<line x1="{cx}" y1="{y0}" x2="{cx}" y2="{y-20}" stroke="{LINE}" stroke-width="3" marker-end="url(#ah)"/>')
    my = (y0 + y - 20) / 2
    add(f'<line x1="{cx-30}" y1="{my}" x2="{cx-8}" y2="{my}" stroke="{LINE}" stroke-width="4"/>')
    add(f'<line x1="{cx+8}" y1="{my}" x2="{cx+30}" y2="{my}" stroke="{LINE}" stroke-width="4"/>')
    if i == 0:
        add(f'<text x="{cx+40}" y="{my+5}" font-size="14" fill="{MUTE}">穴 φ2.5mm（隔壁の厚さ 1.5mm）</text>')
    dash = ' stroke-dasharray="7 5"' if hidden else ""
    add(f'<rect x="{cx-120}" y="{y-20}" width="240" height="40" rx="8" fill="{fill}" stroke="{LINE}" stroke-width="2"{dash}/>')
    add(f'<circle cx="{cx-95}" cy="{y}" r="15" fill="{INK}"/>')
    add(f'<text x="{cx-95}" y="{y+6}" font-size="17" font-weight="bold" fill="#ffffff" text-anchor="middle">{i+1}</text>')
    add(f'<text x="{cx+12}" y="{y+7}" font-size="18" font-weight="bold" fill="{tc}" text-anchor="middle">{name}{"（奥）" if hidden else ""}</text>')
    add(f'<text x="{cx+135}" y="{y+6}" font-size="14" fill="{MUTE}">穴 {i}個</text>')
add(f'<text x="{cx+135}" y="{y+40}" font-size="13" fill="{MUTE}" text-anchor="middle">↑ センサーまでに通る穴の数</text>')

wy = y + 90
add(f'<line x1="{PX+30}" y1="{wy-40}" x2="{PX+PW-30}" y2="{wy-40}" stroke="#e2e8f0" stroke-width="2"/>')
add(f'<text x="{PX+30}" y="{wy}" font-size="21" font-weight="bold" fill="{INK}">なぜ押した場所がわかるのか</text>')
points = [
    "押した場所からセンサーまでに通る穴の数が 0〜6個と違う",
    "部屋の大きさ・形・壁のやわらかさも場所ごとに違う",
    "だから、同じように押しても圧力の伝わり方（波形）が変わる",
    "この波形の違いを Solist-AI が覚えて、押した場所を当てる",
]
for i, p in enumerate(points):
    yy = wy + 38 + i * 32
    add(f'<circle cx="{PX+40}" cy="{yy-6}" r="5" fill="#dd6b20"/>')
    add(f'<text x="{PX+56}" y="{yy}" font-size="16" fill="{INK}">{p}</text>')
add(f'<text x="{PX+30}" y="{wy+38+4*32+6}" font-size="13" fill="{MUTE}">※ 恐竜の中に電子部品や配線はありません。センサーは外側に1個だけです。</text>')

add("</svg>")
OUT.write_text("\n".join(s))
print("wrote", OUT)
