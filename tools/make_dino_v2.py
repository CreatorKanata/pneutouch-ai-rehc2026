"""Dinosaur v2: same outer shape, a vented "gap sleeve" inside each leg.

The 10/21 test pieces showed that a free travel ("あそび") behind the skin, made by
a second membrane with a vented pocket, changes the pressure waveform robustly
(ti1 / ti4 / ti5 separated 97 %). This script adds that structure inside the four
legs of the existing dinosaur STL without touching the outer surface, except one
1.2 mm vent hole per leg on the inner thigh.

For each leg (found automatically from a horizontal slice):
  * a sleeve: a thin tube (WALL) following the leg's inner wall at distance GAP,
    from the sole up to just below the leg partition, built from stacked slices
  * a top flange that closes the annular pocket under the partition
  * a vent from the pocket to the outside, pointing toward the body centre line

    .venv/bin/python tools/make_dino_v2.py --gap 2.5 --out 3d-models/dino-air-v2-leg-gap.stl

Requires manifold3d, trimesh, numpy, matplotlib.
"""
import argparse
import json
from pathlib import Path

import numpy as np
import manifold3d as mf
import trimesh

WALL = 1.5
SEG = 2.0            # height of each stacked sleeve segment [mm]
VENT_D = 1.2
SEGMENTS = 48


def load(path):
    m = trimesh.load(path)
    mesh = mf.Mesh(vert_properties=np.asarray(m.vertices, dtype=np.float32),
                   tri_verts=np.asarray(m.faces, dtype=np.uint32))
    solid = mf.Manifold(mesh)
    assert solid.status() == mf.Error.NoError, solid.status()
    return solid


def components(solid, z):
    """List of (outer polygon, hole polygons, centroid, area) for every closed region at height z."""
    out = []
    for cs in solid.slice(z).decompose():
        polys = cs.to_polygons()
        if not polys:
            continue
        polys = sorted(polys, key=lambda p: -abs(poly_area(p)))
        outer, holes = polys[0], polys[1:]
        pts = np.array(outer)
        out.append(dict(outer=outer, holes=holes, centroid=pts.mean(0), area=abs(poly_area(outer)), z=z))
    return out


def poly_area(p):
    pts = np.array(p)
    x, y = pts[:, 0], pts[:, 1]
    return 0.5 * float(np.dot(x, np.roll(y, -1)) - np.dot(y, np.roll(x, -1)))


def find_legs(solid, z_probe, max_area, y_max):
    legs = [c for c in components(solid, z_probe) if c['area'] < max_area and c['centroid'][1] < y_max and c['holes']]
    assert len(legs) == 4, f'expected 4 legs at z={z_probe}, found {len(legs)}'
    return sorted(legs, key=lambda c: (round(c['centroid'][1] / 20), c['centroid'][0]))


def leg_at(solid, z, centroid, max_area):
    """The leg component whose centroid is closest to `centroid` at height z, or None."""
    cands = [c for c in components(solid, z) if c['area'] < max_area and c['holes']]
    if not cands:
        return None
    c = min(cands, key=lambda c: np.linalg.norm(c['centroid'] - centroid))
    return c if np.linalg.norm(c['centroid'] - centroid) < 8.0 else None


def leg_extent(solid, leg, max_area, z_lo=2.0, z_hi=60.0, step=0.5):
    """Lowest and highest z at which this leg still has its own closed cavity."""
    zs = np.arange(z_lo, z_hi, step)
    ok = [leg_at(solid, z, leg['centroid'], max_area) is not None for z in zs]
    first = zs[ok.index(True)]
    last = zs[len(ok) - 1 - ok[::-1].index(True)]
    return float(first), float(last)


def cs_of(poly):
    """CrossSection from one loop with counter-clockwise winding (holes come out clockwise)."""
    pts = list(poly)
    if poly_area(pts) < 0:
        pts = pts[::-1]
    return mf.CrossSection([pts])


def ring(inner_poly, gap, wall):
    cs = cs_of(inner_poly)
    return cs.offset(-gap, mf.JoinType.Round, 2.0, SEGMENTS) - cs.offset(-gap - wall, mf.JoinType.Round, 2.0, SEGMENTS)


def flange(outer_poly, inner_poly, gap, wall):
    """Annulus from (just inside the outer skin) to the sleeve's inner face: closes the pocket."""
    skin = cs_of(outer_poly).offset(-0.3, mf.JoinType.Round, 2.0, SEGMENTS)
    core = cs_of(inner_poly).offset(-gap - wall, mf.JoinType.Round, 2.0, SEGMENTS)
    return skin - core


def extrude_at(cs, z0, h):
    return cs.extrude(h).translate((0.0, 0.0, z0))


def build_leg_insert(solid, leg, gap, max_area, body_x):
    z0, z1 = leg_extent(solid, leg, max_area)
    parts = []
    top_flange_z = z1 - WALL                       # sleeve stops here; the cone ceiling ends at z1

    def inner_at(z):
        c = leg_at(solid, min(max(z, z0 + 1.0), z1 - 0.5), leg['centroid'], max_area)
        return max(c['holes'], key=lambda p: abs(poly_area(p)))

    # Every segment is the union of the rings at its bottom and top slice, so consecutive
    # segments always overlap in plan (a conservative loft: overhang <= lateral change per SEG).
    # The first segment starts inside the sole wall so the sleeve stands on the sole.
    z = z0 - 1.5
    while z < top_flange_z:
        z_top = min(z + SEG, top_flange_z)
        r = ring(inner_at(z), gap, WALL) + ring(inner_at(z_top), gap, WALL)
        parts.append(extrude_at(r, z, z_top - z + 0.2))
        z = z_top
    # Pocket ceiling as a 45-degree cone (self-supporting): the sleeve's outer face walks
    # outward by `step` per `step` of height until it reaches into the skin (-0.3 mm).
    inner_top = inner_at(z1 - 0.5)
    cs_top = cs_of(inner_top)
    core = cs_top.offset(-gap - WALL, mf.JoinType.Round, 2.0, SEGMENTS)
    step = 0.5
    g = gap
    z = top_flange_z - (gap + 0.3)                 # start the cone low enough to end at z1
    while g > -0.3:
        g_next = max(g - step, -0.3)
        outer_face = cs_top.offset(-g_next, mf.JoinType.Round, 2.0, SEGMENTS)
        parts.append(extrude_at(outer_face - core, z, step + 0.2))
        z += step
        g = g_next
    insert = mf.Manifold.batch_boolean(parts, mf.OpType.Add)
    # vent: from the pocket outward, toward the body centre line (inner thigh)
    cx, cy = leg['centroid']
    direction = np.sign(body_x - cx) or 1.0
    zmid = (z0 + z1) / 2.0
    c_mid = leg_at(solid, zmid, leg['centroid'], max_area)
    inner_mid = max(c_mid['holes'], key=lambda p: abs(poly_area(p)))
    pts = np.array(inner_mid)
    half_width = float(np.max(np.abs(pts[:, 0] - cx)))
    start = cx + direction * (half_width - gap + 0.3)           # just outside the sleeve
    length = gap + WALL + 3.0
    vent = mf.Manifold.cylinder(length, VENT_D / 2.0, VENT_D / 2.0, 24).rotate((0.0, 90.0 * direction, 0.0))
    vent = vent.translate((start, cy, zmid))
    pocket_volume = None
    return insert, vent, dict(z_bottom=z0, z_top=z1, centroid=[float(cx), float(cy)], vent_x=float(start),
                              vent_dir=float(direction), pocket_volume_cm3=pocket_volume)


def support_check(solid, info, step=0.5, margin=0.6, radius=16.0):
    """Max fraction of material at any layer that has nothing within `margin` mm below it
    (inside the leg's footprint). ~0 means every layer is supported; >0.3 means a floating part."""
    cx, cy = info['centroid']
    worst = (0.0, None)
    window = mf.CrossSection.circle(radius, 64).translate((cx, cy))
    for z in np.arange(info['z_bottom'] - 1.0, info['z_top'] + 1.5, step):
        layer = solid.slice(z) ^ window
        if layer.area() < 1e-6:
            continue
        below = (solid.slice(z - step) ^ window).offset(margin, mf.JoinType.Round, 2.0, 16)
        unsupported = (layer - below).area() / layer.area()
        if unsupported > worst[0]:
            worst = (float(unsupported), float(z))
    return worst


def section_figure(solid, legs_info, path, gap):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(2, 4, figsize=(18, 9))
    for col, info in enumerate(legs_info):
        cx, cy = info['centroid']; zm = (info['z_bottom'] + info['z_top']) / 2.0
        ax = axes[0, col]
        for poly in solid.slice(zm).to_polygons():
            pts = np.vstack([np.asarray(poly), np.asarray(poly)[:1]]); ax.plot(pts[:, 0], pts[:, 1], lw=0.8, color='#204080')
        ax.set_xlim(cx - 16, cx + 16); ax.set_ylim(cy - 16, cy + 16); ax.set_aspect('equal'); ax.grid(alpha=.3)
        ax.set_title(f'leg {col+1}: horizontal at z={zm:.1f}')
        ax = axes[1, col]
        r = solid.rotate((90.0, 0.0, 0.0))     # (x,y,z)->(x,-z,y): slice at height y gives (x, -z)
        for poly in r.slice(cy).to_polygons():
            pts = np.vstack([np.asarray(poly), np.asarray(poly)[:1]]); ax.plot(pts[:, 0], -pts[:, 1], lw=0.8, color='#204080')
        ax.set_xlim(cx - 16, cx + 16); ax.set_ylim(0, info['z_top'] + 12); ax.set_aspect('equal'); ax.grid(alpha=.3)
        ax.set_title(f'leg {col+1}: vertical at y={cy:.1f}')
    fig.suptitle(f'Dinosaur v2: gap sleeve (gap {gap} mm, wall {WALL} mm) inside each leg, vent toward the body centre')
    fig.tight_layout(); fig.savefig(path, dpi=90); plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--src', default='3d-models/dino-air-7segments.stl')
    parser.add_argument('--out', default='3d-models/dino-air-v2-leg-gap.stl')
    parser.add_argument('--gap', type=float, default=2.5)
    parser.add_argument('--z-probe', type=float, default=12.0)
    parser.add_argument('--max-leg-area', type=float, default=900.0)
    parser.add_argument('--y-max', type=float, default=125.0)
    args = parser.parse_args()
    solid = load(args.src)
    bb = solid.bounding_box(); body_x = (bb[0] + bb[3]) / 2.0
    legs = find_legs(solid, args.z_probe, args.max_leg_area, args.y_max)
    inserts, vents, infos = [], [], []
    for leg in legs:
        ins, vent, info = build_leg_insert(solid, leg, args.gap, args.max_leg_area, body_x)
        inserts.append(ins); vents.append(vent); infos.append(info)
    v1_volume = solid.volume()
    v2 = (solid + mf.Manifold.batch_boolean(inserts, mf.OpType.Add)) - mf.Manifold.batch_boolean(vents, mf.OpType.Add)
    assert v2.status() == mf.Error.NoError
    mesh = v2.to_mesh()
    tm = trimesh.Trimesh(np.asarray(mesh.vert_properties)[:, :3], np.asarray(mesh.tri_verts), process=False)
    assert tm.is_watertight
    out = Path(args.out); out.parent.mkdir(parents=True, exist_ok=True); tm.export(out)
    for info in infos:
        info['max_unsupported_fraction'], info['at_z'] = support_check(v2, info)
    section_figure(v2, infos, out.with_suffix('.png'), args.gap)
    report = dict(src=args.src, gap_mm=args.gap, wall_mm=WALL, vent_d_mm=VENT_D, legs=infos,
                  tpu_cm3_v1=round(v1_volume / 1000, 1), tpu_cm3_v2=round(v2.volume() / 1000, 1),
                  genus=v2.genus(), triangles=v2.num_tri())
    out.with_suffix('.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2, ensure_ascii=False))


if __name__ == '__main__':
    main()
