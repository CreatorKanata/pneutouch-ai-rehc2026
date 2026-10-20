"""Generate the 10/15 pneumatic test pieces as STL files.

Five pieces share one body chamber and differ only in the press face of a
second chamber connected through a 2.5 mm hole. They test whether the press
face mechanics (not the air channel) change the 40 SPS waveform shape.

Print orientation: floor on the bed (z = 0), press face is a vertical wall
facing +X, roofs are 45 degree gables, so no supports are needed. All edges
except the bed contact are rounded (ROUND_R) so the face is comfortable to
press, like the dinosaur. Every wall is at least MIN_WALL thick (TPU below
1.5 mm tears or gets pinholes).

    .venv/bin/python tools/make_test_pieces.py --set internal   # same skin, inside differs
    .venv/bin/python tools/make_test_pieces.py --set surface    # press face itself differs

Requires manifold3d and trimesh (pip install manifold3d trimesh).
"""
import argparse
import json
import math
from dataclasses import dataclass
from pathlib import Path

import manifold3d as mf

MIN_WALL = 1.5          # mm, user constraint for TPU
WALL = 1.5              # default wall
ROUND_R = 4.0           # edge rounding radius (not applied at the bed)
PLEAT_R = 1.5           # rounding of the bellows pleats
HOLE_D = 2.5            # partition hole, same as the dinosaur
BOSS_OD, BOSS_ID, BOSS_LEN = 5.7, 2.7, 12.0   # sensor tube boss (product-design.md)
BELOW = ROUND_R + 1.0   # profiles extend below the bed, then get trimmed flat

BODY = dict(x=32.0, y=32.0, z=24.0)   # inner size of the body chamber
PRESS_Y, PRESS_Z = 30.0, 28.0          # inner width/height of the press chamber
PRESS_X = 14.0                         # inner depth (volume variant uses less)
SEGMENTS = 32


@dataclass(frozen=True)
class Piece:
    name: str
    face: str          # flat | dome | bellows | ribs | gap
    face_t: float      # press face thickness (flat part of the skin)
    depth: float       # inner depth of the press chamber (X)
    note: str
    gap: float = 0.0   # gap behind the skin before the inner membrane (face == 'gap')

    @property
    def index(self):
        """1-based number taken from the name (tp3-..., ti4-...) -> count of ID dimples."""
        return int(''.join(ch for ch in self.name.split('-')[0] if ch.isdigit()))


# Set "surface": the press face itself differs (outer shape changes).
SURFACE_PIECES = (
    Piece('tp1-flat-1.5', 'flat', 1.5, PRESS_X, '基準: 平面 1.5 mm'),
    Piece('tp2-flat-2.5', 'flat', 2.5, PRESS_X, '硬さだけ変える: 平面 2.5 mm'),
    Piece('tp3-dome-1.5', 'dome', 1.5, PRESS_X, 'スナップ候補: 球面ドーム 底半径 10, 高さ 5, 壁 1.5'),
    Piece('tp4-bellows-1.5', 'bellows', 1.5, PRESS_X, '遅い復元候補: 蛇腹 3.5 山, 振幅 3, 壁 1.5'),
    Piece('tp5-flat-1.5-small', 'flat', 1.5, 5.0, '容積 1/3: 平面 1.5 mm, 奥行 5'),
)

# Set "internal": every outer surface is the same flat 1.5 mm skin; only the inside differs.
INTERNAL_PIECES = (
    Piece('ti1-plain', 'flat', 1.5, PRESS_X, '基準: 皮 1.5 mm、内部なし'),
    Piece('ti2-thick-3.0', 'flat', 3.0, PRESS_X, '内側から肉厚 3.0 mm（外形は同じ）'),
    Piece('ti3-ribs', 'ribs', 1.5, PRESS_X, '皮の裏に縦リブ 1.5 mm × 高さ 3、ピッチ 6'),
    Piece('ti4-gap-1.5', 'gap', 1.5, PRESS_X, '皮の裏に 1.5 mm の隙間と第 2 の膜 1.5 mm（隙間は床に通気）', gap=1.5),
    Piece('ti5-gap-3.0', 'gap', 1.5, PRESS_X, '皮の裏に 3.0 mm の隙間と第 2 の膜 1.5 mm（隙間は床に通気）', gap=3.0),
)
PIECE_SETS = {'surface': SURFACE_PIECES, 'internal': INTERNAL_PIECES}


# ---------------------------------------------------------------- 2-D helpers

def gable_profile(x0, x1, z_eave, zig=None):
    """Closed XZ polygon: floor (below the bed), +X wall (optionally zigzag), 45 deg gable."""
    ridge_z = z_eave + (x1 - x0) / 2.0
    pts = [(x0, -BELOW), (x1, -BELOW)]
    pts += zig if zig else [(x1, z_eave)]
    pts += [((x0 + x1) / 2.0, ridge_z), (x0, z_eave)]
    return pts


def rounded(cs, r):
    """Round convex and concave corners of a cross-section (closing then opening)."""
    if r <= 0:
        return cs
    return (cs.offset(r, mf.JoinType.Round, 2.0, SEGMENTS)
              .offset(-2.0 * r, mf.JoinType.Round, 2.0, SEGMENTS)
              .offset(r, mf.JoinType.Round, 2.0, SEGMENTS))


def rounded_rect(x0, x1, y0, y1, r):
    r = max(min(r, (x1 - x0) / 2.0 - 1.0, (y1 - y0) / 2.0 - 1.0), 0.0)   # keep a flat strip
    core = mf.CrossSection.square((x1 - x0 - 2 * r, y1 - y0 - 2 * r)).translate((x0 + r, y0 + r))
    return core.offset(r, mf.JoinType.Round, 2.0, SEGMENTS) if r > 0 else core


# ---------------------------------------------------------------- 3-D helpers

def extrude_xz(cs, y0, y1):
    """Extrude an XZ cross-section along Y from y0 to y1."""
    # extrude gives (x, z, y'); rotate about X by +90: (x, y, z) -> (x, -z, y)
    return cs.extrude(y1 - y0).rotate((90.0, 0.0, 0.0)).translate((0.0, y1, 0.0))


def extrude_yz(cs, x0, x1):
    """Extrude a YZ cross-section along X from x0 to x1."""
    solid = cs.extrude(x1 - x0).rotate((90.0, 0.0, 0.0)).rotate((0.0, 0.0, 90.0))
    return solid.translate((x0, 0.0, 0.0))


def rounded_body(profile, x0, x1, y0, y1, z_top, r, profile_r):
    """Extruded XZ profile with every edge rounded except at the bed."""
    solid = extrude_xz(rounded(mf.CrossSection([profile]), profile_r), y0, y1)
    xy = rounded_rect(x0, x1, y0, y1, r).extrude(z_top + BELOW).translate((0.0, 0.0, -BELOW))
    yz = extrude_yz(rounded_rect(y0, y1, -BELOW, z_top, r), x0 - 1.0, x1 + 1.0)
    return solid ^ xy ^ yz


def rounded_shell(profile, x0, x1, y0, y1, z_top, wall, r, profile_r):
    outer = rounded_body(profile, x0, x1, y0, y1, z_top, r, profile_r)
    inner_cs = rounded(mf.CrossSection([profile]), profile_r).offset(-wall, mf.JoinType.Round, 2.0, SEGMENTS)
    ri = max(r - wall, 0.0)
    cav = extrude_xz(inner_cs, y0 + wall, y1 - wall)
    xy = rounded_rect(x0 + wall, x1 - wall, y0 + wall, y1 - wall, ri).extrude(z_top + BELOW).translate((0.0, 0.0, -BELOW))
    yz = extrude_yz(rounded_rect(y0 + wall, y1 - wall, -BELOW, z_top, ri), x0 - 1.0, x1 + 1.0)
    cav = (cav ^ xy ^ yz).trim_by_plane((0.0, 0.0, 1.0), wall)
    return (outer - cav).trim_by_plane((0.0, 0.0, 1.0), 0.0)


# ---------------------------------------------------------------- the pieces

def body_shell():
    x1, y1 = BODY['x'] + 2 * WALL, BODY['y'] + 2 * WALL
    z_eave = WALL + BODY['z']
    profile = gable_profile(0.0, x1, z_eave)
    return rounded_shell(profile, 0.0, x1, 0.0, y1, z_eave + x1 / 2.0 + 1.0, WALL, ROUND_R, ROUND_R)


def bellows_zigzag(x_out, z_bottom, z_top, amplitude=3.0, pitch=8.0):
    pts, z, x_in, toggle = [(x_out, z_bottom)], z_bottom, x_out - amplitude, False
    while z + pitch / 2.0 <= z_top + 1e-6:
        z += pitch / 2.0
        toggle = not toggle
        pts.append((x_in if toggle else x_out, z))
    if abs(pts[-1][1] - z_top) > 1e-6:
        pts.append((pts[-1][0], z_top))
    return pts


def press_shell(piece, x_start, y0, y1):
    """Press chamber whose -X wall overlaps the body's +X wall exactly."""
    x_out = x_start + piece.depth + 2 * WALL
    z_eave = WALL + PRESS_Z
    z_top = z_eave + (x_out - x_start) / 2.0 + 1.0
    # a shallow chamber cannot take the full rounding radius; keep a flat strip of >= 1 mm
    r = min(ROUND_R, (x_out - x_start) / 2.0 - WALL - 1.0)
    if piece.face == 'bellows':
        profile = gable_profile(x_start, x_out, z_eave, bellows_zigzag(x_out, WALL, z_eave))
        shell = rounded_shell(profile, x_start, x_out, y0, y1, z_top, WALL, r, min(PLEAT_R, r))
    else:
        profile = gable_profile(x_start, x_out, z_eave)
        shell = rounded_shell(profile, x_start, x_out, y0, y1, z_top, WALL, r, r)
    if piece.face == 'flat' and piece.face_t > WALL:
        extra = piece.face_t - WALL
        plate = mf.Manifold.cube((extra + 0.5, y1 - y0 - 2 * WALL, PRESS_Z)).translate(
            (x_out - WALL - extra, y0 + WALL, WALL))
        shell = shell + (plate ^ shell.hull())
    if piece.face == 'dome':
        shell = add_dome(shell, x_out, (y0 + y1) / 2.0, WALL + PRESS_Z / 2.0)
    if piece.face == 'ribs':
        shell = shell + (ribs(x_out - WALL, y0 + WALL, y1 - WALL) ^ shell.hull())
    if piece.face == 'gap':
        shell = add_gap_membrane(shell, piece.gap, x_out - WALL, y0, y1, x_start)
    return shell, x_out


def ribs(x_in, y_lo, y_hi, height=3.0, pitch=6.0, thickness=WALL):
    """Vertical fins on the inside of the skin, running the full inner height."""
    fins = []
    y = y_lo + pitch / 2.0
    while y + thickness / 2.0 <= y_hi:
        fins.append(mf.Manifold.cube((height + 0.5, thickness, PRESS_Z)).translate(
            (x_in - height, y - thickness / 2.0, WALL)))
        y += pitch
    return mf.Manifold.batch_boolean(fins, mf.OpType.Add)


def add_gap_membrane(shell, gap, x_in, y0, y1, x_start, vent_d=1.2):
    """Second membrane behind the skin. The pocket between them vents through the floor,
    so the skin travels `gap` mm freely before it pushes the membrane into the chamber."""
    x_m = x_in - gap                      # membrane outer face (toward the skin)
    membrane = mf.Manifold.cube((WALL, y1 - y0, PRESS_Z + (x_in - x_start) + 2 * WALL)).translate((x_m - WALL, y0, 0.0))
    membrane = membrane ^ shell.hull()    # follow the rounded outline, no overhang
    vent = mf.Manifold.cylinder(WALL * 3, vent_d / 2.0, vent_d / 2.0, 32).translate(
        ((x_m + x_in) / 2.0, (y0 + y1) / 2.0, -WALL))
    assert x_m - WALL > x_start + WALL + 1.0, 'no room left for the main chamber'
    return (shell + membrane) - vent


def add_dome(shell, x_face, yc, zc, base_r=10.0, height=5.0, wall=WALL):
    """Replace the centre of the flat face with a spherical cap bulging in +X."""
    radius = (base_r ** 2 + height ** 2) / (2.0 * height)              # sphere radius
    window_r = math.sqrt((radius - wall) ** 2 - (radius - height) ** 2) - 0.5
    window = mf.Manifold.cylinder(wall * 3, window_r, window_r, 96).rotate(
        (0.0, 90.0, 0.0)).translate((x_face - 2 * wall, yc, zc))
    centre = (x_face - (radius - height), yc, zc)
    cap = (mf.Manifold.sphere(radius, 128) - mf.Manifold.sphere(radius - wall, 128)).translate(centre)
    cap = cap.trim_by_plane((1.0, 0.0, 0.0), x_face - wall)   # keep x >= x_face - wall
    outer_ring = math.sqrt(radius ** 2 - (radius - height - wall) ** 2)
    assert outer_ring < PRESS_Z / 2.0 - ROUND_R + wall, 'dome rim runs into the rounded edge'
    return (shell - window) + cap


def sensor_boss(yc, zc):
    boss = mf.Manifold.cylinder(BOSS_LEN + WALL, BOSS_OD / 2.0, BOSS_OD / 2.0, 64)
    bore = mf.Manifold.cylinder(BOSS_LEN + 2 * WALL, BOSS_ID / 2.0, BOSS_ID / 2.0, 64)
    boss = boss.rotate((0.0, 90.0, 0.0)).translate((-BOSS_LEN, yc, zc))
    bore = bore.rotate((0.0, 90.0, 0.0)).translate((-BOSS_LEN - 0.5, yc, zc))
    return boss, bore


def build(piece):
    body = body_shell()
    bx1 = BODY['x'] + 2 * WALL
    y0 = (BODY['y'] + 2 * WALL - (PRESS_Y + 2 * WALL)) / 2.0
    y1 = y0 + PRESS_Y + 2 * WALL
    press, x_out = press_shell(piece, bx1 - WALL, y0, y1)
    yc, zc = (BODY['y'] + 2 * WALL) / 2.0, WALL + PRESS_Z / 2.0
    hole = mf.Manifold.cylinder(WALL * 3, HOLE_D / 2.0, HOLE_D / 2.0, 48).rotate(
        (0.0, 90.0, 0.0)).translate((bx1 - 2 * WALL, yc, zc))
    boss, bore = sensor_boss(yc, WALL + BODY['z'] / 2.0)
    solid = (body + press + boss) - hole - bore
    solid = solid - id_dimples(piece.index, BODY['y'] + 2 * WALL)
    return solid, dict(x_out=x_out, overall=solid.bounding_box())


def id_dimples(count, y_face, d=2.5, depth=0.8, pitch=4.5, z=12.0):
    """`count` shallow dimples on the body's +Y side wall so pieces can be told apart."""
    dimples = [mf.Manifold.cylinder(depth + 1.0, d / 2.0, d / 2.0, 32).rotate((-90.0, 0.0, 0.0)).translate(
        (6.0 + i * pitch, y_face - depth, z)) for i in range(count)]
    return mf.Manifold.batch_boolean(dimples, mf.OpType.Add) if dimples else mf.Manifold()


def report_row(piece, solid, info):
    bb = info['overall']
    size = [round(bb[i + 3] - bb[i], 1) for i in range(3)]
    return dict(name=piece.name, face=piece.face, face_mm=piece.face_t, depth_mm=piece.depth,
                note=piece.note, size_mm=size, tpu_cm3=round(solid.volume() / 1000.0, 1),
                genus=solid.genus(), triangles=solid.num_tri())


def write_stl(solid, path):
    import numpy as np
    import trimesh
    mesh = solid.to_mesh()
    tm = trimesh.Trimesh(np.asarray(mesh.vert_properties)[:, :3], np.asarray(mesh.tri_verts), process=False)
    assert tm.is_watertight, f'{path}: not watertight'
    tm.export(path)
    return tm


def plot_sections(pieces_solids, path):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(1, len(pieces_solids), figsize=(4 * len(pieces_solids), 4.5))
    yc = (BODY['y'] + 2 * WALL) / 2.0
    for ax, (piece, solid) in zip(axes, pieces_solids):
        section = solid.rotate((-90.0, 0.0, 0.0)).slice(-yc)   # XZ plane at y = yc
        for poly in section.to_polygons():
            xs = [p[0] for p in poly] + [poly[0][0]]
            zs = [p[1] for p in poly] + [poly[0][1]]
            ax.fill(xs, zs, color='#c8d8f0', edgecolor='#204080', linewidth=0.8)
        ax.set_aspect('equal'); ax.set_title(piece.name, fontsize=9)
        ax.set_xlabel('X [mm]  (press face at +X)'); ax.set_ylabel('Z [mm]')
        ax.grid(alpha=0.3)
    fig.suptitle('XZ cross-section at the centre (y = %.1f mm); bed at z = 0' % yc)
    fig.tight_layout(); fig.savefig(path, dpi=110); plt.close(fig)


def plot_preview(pieces_solids, path):
    import numpy as np
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection
    fig = plt.figure(figsize=(4 * len(pieces_solids), 4.5))
    light = np.array([0.4, -0.6, 0.7])
    for i, (piece, solid) in enumerate(pieces_solids):
        mesh = solid.to_mesh()
        v = np.asarray(mesh.vert_properties)[:, :3]; f = np.asarray(mesh.tri_verts)
        tri = v[f]
        n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
        n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-12
        shade = np.clip(0.35 + 0.65 * np.abs(n @ light), 0, 1)
        ax = fig.add_subplot(1, len(pieces_solids), i + 1, projection='3d')
        pc = Poly3DCollection(tri, edgecolor='none')
        pc.set_facecolor(np.c_[shade * 0.6, shade * 0.72, shade * 0.9, np.ones_like(shade)])
        ax.add_collection3d(pc)
        lo, hi = v.min(axis=0), v.max(axis=0); c = (lo + hi) / 2; r = (hi - lo).max() / 2
        ax.set_xlim(c[0] - r, c[0] + r); ax.set_ylim(c[1] - r, c[1] + r); ax.set_zlim(0, 2 * r)
        ax.view_init(elev=25, azim=-50); ax.set_title(piece.name, fontsize=9); ax.set_axis_off()
    fig.suptitle('Test pieces, print orientation (press face toward +X, sensor boss toward -X)')
    fig.tight_layout(); fig.savefig(path, dpi=100); plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--out', default=None, help='default: 3d-models/test-pieces[-internal]')
    parser.add_argument('--set', choices=sorted(PIECE_SETS), default='internal',
                        help='surface: the press face differs; internal: same skin, inside differs')
    args = parser.parse_args()
    pieces = PIECE_SETS[args.set]
    out = Path(args.out or ('3d-models/test-pieces' if args.set == 'surface' else '3d-models/test-pieces-internal'))
    out.mkdir(parents=True, exist_ok=True)
    assert WALL >= MIN_WALL and all(p.face_t >= MIN_WALL for p in pieces)
    rows, solids = [], []
    for piece in pieces:
        solid, info = build(piece)
        write_stl(solid, out / f'{piece.name}.stl')
        rows.append(report_row(piece, solid, info)); solids.append((piece, solid))
    plot_sections(solids, out / 'sections.png')
    plot_preview(solids, out / 'preview.png')
    (out / 'pieces.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding='utf-8')
    for r in rows:
        print(f"{r['name']:22s} size={r['size_mm']} TPU={r['tpu_cm3']} cm3 genus={r['genus']} tris={r['triangles']}  {r['note']}")


if __name__ == '__main__':
    main()
