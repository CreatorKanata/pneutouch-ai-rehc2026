"""Draw the press/release waveform and the 12 features as an SVG (16:9, 1760x990).

The curve is synthetic but shaped like the real demo screenshot. Crossing times,
areas and slopes are computed from the curve the same way the firmware does, so
every arrow sits where the firmware would measure it.
"""
import math
import pathlib

OUT = pathlib.Path(__file__).resolve().parents[1] / "images" / "protopedia" / "waveform-features.svg"

# ---------------------------------------------------------------- waveform
def agauss(t, c, sl, sr):
    s = sl if t < c else sr
    return math.exp(-((t - c) / s) ** 2)

def sig(x):
    return 1.0 / (1.0 + math.exp(-x))

def delta(t):
    press = 2.45e6 * agauss(t, 0.19, 0.12, 0.10)
    hold = 1.35e6 * sig((t - 0.30) / 0.035) * (1 - sig((t - 0.60) / 0.045))
    release = -4.0e6 * agauss(t, 1.05, 0.14, 0.17)
    return press + hold + release

T0, T1 = -0.8, 1.9
N = 1081
ts = [T0 + (T1 - T0) * i / (N - 1) for i in range(N)]
vs = [delta(t) for t in ts]

# ---------------------------------------------------------------- firmware-like features
def crossing(level, lo, hi, negative, up, last):
    found = None
    for i in range(lo, hi):
        a, b = vs[i], vs[i + 1]
        if negative:
            a, b = -a, -b
        hit = (a < level <= b) if up else (a > level >= b)
        if hit:
            found = ts[i] + (level - a) / (b - a) * (ts[i + 1] - ts[i])
            if not last:
                break
    return found

ipeak = max(range(N), key=lambda i: vs[i])
itrough = min(range(ipeak, N), key=lambda i: vs[i])
A, B = vs[ipeak], -vs[itrough]
levels = [0.1, 0.5, 0.9]
pa = [crossing(l * A, 0, ipeak, False, True, True) for l in levels]
pd = [crossing(l * A, ipeak, itrough, False, False, False) for l in levels]
na = [crossing(l * B, ipeak, itrough, True, True, True) for l in levels]
nd = [crossing(l * B, itrough, N - 1, True, False, False) for l in levels]

# steepest rise between 10% crossing and peak
islope = max(range(N - 1), key=lambda i: (vs[i + 1] - vs[i]) if pa[0] <= ts[i] <= ts[ipeak] else -1e30)

feat = {
    "rise": pa[2] - pa[0], "decay": pd[0] - pd[2], "width": pd[1] - pa[1],
    "rel_rise": na[2] - na[0], "recovery": nd[0] - nd[2], "rel_width": nd[1] - na[1],
    "B/A": B / A,
}

# ---------------------------------------------------------------- chart mapping
CX0, CX1 = 110, 1040          # x range of chart
CY0, CY1 = 250, 720           # y range of chart (top, bottom)
VMAX, VMIN = 3.2e6, -4.6e6

def X(t):
    return CX0 + (t - T0) / (T1 - T0) * (CX1 - CX0)

def Y(v):
    return CY0 + (VMAX - v) / (VMAX - VMIN) * (CY1 - CY0)

Y0 = Y(0)

def path_of(lo_t, hi_t, close_to_base=False):
    pts = [(X(t), Y(v)) for t, v in zip(ts, vs) if lo_t <= t <= hi_t]
    d = "M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in pts)
    if close_to_base:
        d += f" L{pts[-1][0]:.1f},{Y0:.1f} L{pts[0][0]:.1f},{Y0:.1f} Z"
    return d

FONT = "Hiragino Sans, Hiragino Kaku Gothic ProN, Hiragino Kaku Gothic Pro, sans-serif"
POS, NEG, BASE, INK, MUTE = "#dd6b20", "#2b6cb0", "#718096", "#1a202c", "#4a5568"

s = []
add = s.append
add(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1760 990" width="1760" height="990" font-family="{FONT}">')
add('''<defs>
  <marker id="ah" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto-start-reverse">
    <path d="M0,0 L10,5 L0,10 z" fill="context-stroke"/></marker>
  <marker id="ah-pos" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="#dd6b20"/></marker>
  <marker id="ah-neg" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="#2b6cb0"/></marker>
  <marker id="ah-ink" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="#1a202c"/></marker>
</defs>''')
add('<rect width="1760" height="990" fill="#ffffff"/>')

# title
add(f'<text x="50" y="68" font-size="40" font-weight="bold" fill="{INK}">押して、離すと、どんな波形になる？</text>')
add(f'<text x="50" y="110" font-size="22" fill="{MUTE}">1回のタッチの圧力波形と、そこから取り出す12個の特徴量</text>')
add('<text x="1710" y="68" font-size="20" fill="#718096" text-anchor="end">Created by Kanata the Kid Creator</text>')

# chart panel
add('<rect x="50" y="150" width="1040" height="650" rx="18" fill="#f7fafc" stroke="#cbd5e0" stroke-width="2"/>')
add(f'<text x="80" y="190" font-size="22" font-weight="bold" fill="{INK}">このタッチの波形（背中を約1秒押して離した例）</text>')

# phase bands
bands = [
    (T0, pa[0], "#edf2f7", "押す前", BASE),
    (pa[0], pd[0], "#feebc8", "押す → 押している", POS),
    (pd[0], nd[0], "#bee3f8", "離す → もどる", NEG),
    (nd[0], T1, "#edf2f7", "元どおり", BASE),
]
for lo, hi, fill, label, color in bands:
    add(f'<rect x="{X(lo):.1f}" y="{CY0}" width="{X(hi)-X(lo):.1f}" height="{CY1-CY0}" fill="{fill}" opacity="0.55"/>')
    add(f'<text x="{(X(lo)+X(hi))/2:.1f}" y="{CY0-8}" font-size="18" font-weight="bold" fill="{color}" text-anchor="middle">{label}</text>')

# axes
add(f'<line x1="{CX0}" y1="{Y0:.1f}" x2="{CX1}" y2="{Y0:.1f}" stroke="{BASE}" stroke-width="2" stroke-dasharray="6 5"/>')
add(f'<text x="{CX0+6}" y="{Y0+22:.1f}" font-size="15" fill="{BASE}">基準値 = 0（押す前 0.8〜0.3 秒の中央値）</text>')
for tt in (-0.5, 0.0, 0.5, 1.0, 1.5):
    add(f'<line x1="{X(tt):.1f}" y1="{CY1}" x2="{X(tt):.1f}" y2="{CY1+8}" stroke="{BASE}" stroke-width="2"/>')
    add(f'<text x="{X(tt):.1f}" y="{CY1+30}" font-size="16" fill="{MUTE}" text-anchor="middle">{tt:+.1f}s</text>')
add(f'<text x="{(CX0+CX1)/2}" y="{CY1+58}" font-size="16" fill="{MUTE}" text-anchor="middle">押下検出からの時間（40回/秒で計測）　↑ 圧力の変化（ADC counts）</text>')

# shaded areas (10% to 10%)
add(f'<path d="{path_of(pa[0], pd[0], True)}" fill="{POS}" opacity="0.18"/>')
add(f'<path d="{path_of(na[0], nd[0], True)}" fill="{NEG}" opacity="0.18"/>')

# level lines
for lv, lab in zip(levels, ("10%", "50%", "90%")):
    y = Y(lv * A)
    add(f'<line x1="{X(pa[0])-20:.1f}" y1="{y:.1f}" x2="{X(pd[0])+20:.1f}" y2="{y:.1f}" stroke="{POS}" stroke-width="1.2" stroke-dasharray="4 4"/>')
    add(f'<text x="{X(pd[0])+26:.1f}" y="{y+5:.1f}" font-size="13" fill="{POS}">{lab}</text>')
    y = Y(-lv * B)
    add(f'<line x1="{X(na[0])-20:.1f}" y1="{y:.1f}" x2="{X(nd[0])+20:.1f}" y2="{y:.1f}" stroke="{NEG}" stroke-width="1.2" stroke-dasharray="4 4"/>')
    add(f'<text x="{X(na[0])-50:.1f}" y="{y+5:.1f}" font-size="13" fill="{NEG}">{lab}</text>')

# the curve
add(f'<path d="{path_of(T0, T1)}" fill="none" stroke="#2f855a" stroke-width="4.5" stroke-linejoin="round" stroke-linecap="round"/>')

# trigger line
add(f'<line x1="{X(0):.1f}" y1="{CY0}" x2="{X(0):.1f}" y2="{CY1}" stroke="#b7791f" stroke-width="2" stroke-dasharray="5 4"/>')
add(f'<text x="{X(0)-8:.1f}" y="{CY1-30}" font-size="15" font-weight="bold" fill="#b7791f" text-anchor="end">押下検出</text>')
add(f'<text x="{X(0)-8:.1f}" y="{CY1-11}" font-size="13" fill="#b7791f" text-anchor="end">(+50,000 を2回連続)</text>')

def num(x, y, n, color):
    add(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="13" fill="{color}"/>')
    add(f'<text x="{x:.1f}" y="{y+5:.1f}" font-size="15" font-weight="bold" fill="#ffffff" text-anchor="middle">{n}</text>')

def harrow(t_lo, t_hi, y, color, marker):
    add(f'<line x1="{X(t_lo):.1f}" y1="{y:.1f}" x2="{X(t_hi):.1f}" y2="{y:.1f}" stroke="{color}" stroke-width="2.5" marker-start="url(#{marker})" marker-end="url(#{marker})"/>')

# ---- positive lobe annotations
yA_top = Y(A)
# A: height arrow to the right of peak? put it left of the 90% crossing region: use x at peak
xp = X(ts[ipeak])
add(f'<line x1="{xp+0:.1f}" y1="{Y0:.1f}" x2="{xp:.1f}" y2="{yA_top:.1f}" stroke="{INK}" stroke-width="2.5" marker-start="url(#ah-ink)" marker-end="url(#ah-ink)"/>')
add(f'<text x="{xp+8:.1f}" y="{(Y0+yA_top)/2+6:.1f}" font-size="17" font-weight="bold" fill="{INK}">A</text>')
# 1 rise (10->90) arrow above-left
yr = Y(0.9 * A) - 24
harrow(pa[0], pa[2], yr, POS, "ah-pos")
num(X(pa[0]) - 22, yr, 1, POS)
# 2 decay (90->10) arrow
harrow(pd[2], pd[0], yr, POS, "ah-pos")
num(X(pd[0]) + 22, yr, 2, POS)
# 3 width at 50%
yw = Y(0.5 * A)
num(X(pd[1]) + 26, yw, 3, POS)
# 4 area
num((X(pa[0]) + X(pd[0])) / 2, Y(0.22 * A), 4, POS)
# 5 slope: tangent segment at steepest point
x1, y1 = X(ts[islope]), Y(vs[islope])
dx = X(ts[islope + 1]) - x1
dy = Y(vs[islope + 1]) - y1
k = 38 / math.hypot(dx, dy)
add(f'<line x1="{x1-dx*k:.1f}" y1="{y1-dy*k:.1f}" x2="{x1+dx*k:.1f}" y2="{y1+dy*k:.1f}" stroke="{INK}" stroke-width="3"/>')
num(x1 - 34, y1 + 10, 5, POS)
# 6 asymmetry = 2 / 1 -> label near top
num(X(pd[0]) + 58, yr, 6, POS)
add(f'<text x="{X(pd[0])+76:.1f}" y="{yr+5:.1f}" font-size="13" fill="{POS}">= ②÷①</text>')

# ---- negative lobe annotations
yB_bot = Y(-B)
xt = X(ts[itrough])
add(f'<line x1="{xt:.1f}" y1="{Y0:.1f}" x2="{xt:.1f}" y2="{yB_bot:.1f}" stroke="{INK}" stroke-width="2.5" marker-start="url(#ah-ink)" marker-end="url(#ah-ink)"/>')
add(f'<text x="{xt+8:.1f}" y="{(Y0+yB_bot)/2+6:.1f}" font-size="17" font-weight="bold" fill="{INK}">B</text>')
yr2 = Y(-0.9 * B) + 26
harrow(na[0], na[2], yr2, NEG, "ah-neg")
num(X(na[0]) - 22, yr2, 7, NEG)
harrow(nd[2], nd[0], yr2, NEG, "ah-neg")
num(X(nd[0]) + 22, yr2, 8, NEG)
yw2 = Y(-0.5 * B)
num(X(nd[1]) + 26, yw2, 9, NEG)
num((X(na[0]) + X(nd[0])) / 2, Y(-0.25 * B), 10, NEG)
# 11, 12: ratios -> place between lobes near baseline
num(X(1.50), Y0 - 48, 11, INK)
num(X(1.68), Y0 - 48, 12, INK)
add(f'<text x="{X(1.59):.1f}" y="{Y0-76:.1f}" font-size="14" fill="{INK}" text-anchor="middle">A と B をくらべる</text>')

# width arrows at 50%
harrow(pa[1], pd[1], yw, POS, "ah-pos")
harrow(na[1], nd[1], yw2, NEG, "ah-neg")

# ---------------------------------------------------------------- right panel: the 12 features
PX = 1120
add(f'<rect x="{PX}" y="150" width="590" height="650" rx="18" fill="#ffffff" stroke="#cbd5e0" stroke-width="2"/>')
add(f'<text x="{PX+30}" y="190" font-size="22" font-weight="bold" fill="{INK}">波形から取り出す12個の特徴量</text>')
add(f'<text x="{PX+30}" y="216" font-size="15" fill="{MUTE}">大きさそのものではなく「形」を表す数にする</text>')

def row(y, n, color, name, desc):
    add(f'<circle cx="{PX+46}" cy="{y-6}" r="13" fill="{color}"/>')
    add(f'<text x="{PX+46}" y="{y-1}" font-size="15" font-weight="bold" fill="#ffffff" text-anchor="middle">{n}</text>')
    add(f'<text x="{PX+70}" y="{y}" font-size="18" font-weight="bold" fill="{INK}">{name}</text>')
    add(f'<text x="{PX+300}" y="{y}" font-size="15" fill="{MUTE}">{desc}</text>')

def head(y, color, text):
    add(f'<rect x="{PX+30}" y="{y-20}" width="530" height="28" rx="6" fill="{color}" opacity="0.15"/>')
    add(f'<text x="{PX+42}" y="{y}" font-size="16" font-weight="bold" fill="{color}">{text}</text>')

y = 252
head(y, POS, "押したとき（正の山、高さ A）")
for n, name, desc in [
    (1, "立ち上がり時間", "10% → 90% に上がる時間"),
    (2, "減衰時間", "90% → 10% に下がる時間"),
    (3, "半値幅", "50% を超えている時間"),
    (4, "面積 ÷ A", "山の面積を高さで割った「等価幅」"),
    (5, "最大傾き ÷ A", "いちばん急な上り方"),
    (6, "非対称度", "減衰時間 ÷ 立ち上がり時間"),
]:
    y += 34
    row(y, n, POS, name, desc)
y += 42
head(y, NEG, "離したとき（負の谷、深さ B）")
for n, name, desc in [
    (7, "立ち上がり時間", "谷の 10% → 90% に下がる時間"),
    (8, "回復時間", "90% → 10% に戻る時間"),
    (9, "半値幅", "50% より深い時間"),
    (10, "面積 ÷ B", "谷の面積を深さで割った「等価幅」"),
]:
    y += 34
    row(y, n, NEG, name, desc)
y += 42
head(y, INK, "押すと離すのバランス")
for n, name, desc in [
    (11, "B ÷ A", "谷の深さ ÷ 山の高さ"),
    (12, "谷の面積 ÷ 山の面積", "押しこみと戻りの釣り合い"),
]:
    y += 34
    row(y, n, INK, name, desc)

# ---------------------------------------------------------------- bottom flow
by = 835
boxes = [
    ("圧力の波形", "40回/秒・約2.7秒分", "#2f855a"),
    ("12個の特徴量", "時間・幅・傾き・比", POS),
    ("対数 → 正規化", "bfloat16 に変換", MUTE),
    ("Solist-AI", "チップ上で推論", "#b7791f"),
    ("部位を判定", "HEAD / BACK / LEGS / TAIL を表示", NEG),
]
bw, gap = 300, 36
x = 50
for i, (t1, t2, c) in enumerate(boxes):
    add(f'<rect x="{x}" y="{by}" width="{bw}" height="100" rx="12" fill="#ffffff" stroke="{c}" stroke-width="3"/>')
    add(f'<text x="{x+bw/2}" y="{by+45}" font-size="21" font-weight="bold" fill="{c}" text-anchor="middle">{t1}</text>')
    add(f'<text x="{x+bw/2}" y="{by+76}" font-size="15" fill="{MUTE}" text-anchor="middle">{t2}</text>')
    if i < len(boxes) - 1:
        add(f'<line x1="{x+bw+6}" y1="{by+50}" x2="{x+bw+gap-6}" y2="{by+50}" stroke="{MUTE}" stroke-width="3" marker-end="url(#ah-ink)"/>')
    x += bw + gap

add("</svg>")
OUT.write_text("\n".join(s))
print("A=%.0f B=%.0f" % (A, B))
for k, v in feat.items():
    print(f"{k}: {v*1000:.0f} ms" if k != "B/A" else f"{k}: {v:.2f}")
