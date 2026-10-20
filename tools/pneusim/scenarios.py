"""Candidate internal structures. Each builder returns an immutable Network."""
from types import MappingProxyType

from .network import Chamber, Channel, Leak, Network, Wall

ZONE_CHAMBER = MappingProxyType({1: 'tail', 2: 'back', 3: 'rh', 4: 'lh', 5: 'rf', 6: 'lf', 7: 'head'})
ZONE_NAMES = MappingProxyType({1: 'TAIL', 2: 'BACK', 3: 'RH leg', 4: 'LH leg', 5: 'RF leg',
                               6: 'LF leg', 7: 'HEAD'})
ZONE_GROUP = MappingProxyType({1: 1, 2: 2, 3: 3, 4: 3, 5: 3, 6: 3, 7: 4})  # TAIL/BACK/LEGS/HEAD
GROUP_NAMES = MappingProxyType({1: 'TAIL', 2: 'BACK', 3: 'LEGS', 4: 'HEAD'})

SERIES_ORDER = ('tail', 'rh', 'rf', 'lf', 'lh', 'back', 'head')   # sensor sits on tail
STAR_HUB = 'back'
# (d [mm], L [mm]) from the body hub; legs in series order RH, RF, LF, LH.
STAR_GRADED = MappingProxyType({'tail': (2.0, 8.0), 'rh': (1.6, 10.0), 'rf': (1.5, 15.0),
                                'lf': (1.4, 20.0), 'lh': (1.3, 25.0), 'head': (1.2, 35.0)})


def _design(k=None, c=None, snap=None):
    """Per-chamber wall design: {chamber: (k multiplier, c multiplier, c_out ratio or None)}.

    None for the c_out ratio keeps the calibrated baseline.c_out_ratio.
    """
    names = ('tail', 'back', 'rh', 'lh', 'rf', 'lf', 'head')
    k, c, snap = k or {}, c or {}, snap or {}
    return MappingProxyType({n: (k.get(n, 1.0), c.get(n, 1.0), snap.get(n)) for n in names})


# tau_in = c / k is the wall's inward (sink-in) time constant. Legs by zone 3/4/5/6 = RH/LH/RF/LF.
_TAU4 = {'tail': 0.25, 'back': 0.5, 'rh': 1.0, 'lh': 1.0, 'rf': 1.0, 'lf': 1.0, 'head': 4.0}
_TAU7 = {**_TAU4, 'rh': 0.7, 'lh': 1.0, 'rf': 1.4, 'lf': 2.0}
_SNAP4 = {'tail': 0.02, 'back': 0.1, 'rh': 0.05, 'lh': 0.05, 'rf': 0.05, 'lf': 0.05, 'head': 0.3}
_SNAP7 = {**_SNAP4, 'rh': 0.03, 'lh': 0.05, 'rf': 0.08, 'lf': 0.12}
# Stiffness only: k and c scaled together, so tau_in is unchanged (homogeneous material).
_K_ONLY = {'tail': 0.5, 'back': 0.7, 'rh': 0.85, 'lh': 1.0, 'rf': 1.2, 'lf': 1.4, 'head': 2.0}

MECH_DESIGNS = MappingProxyType({
    'mech_k_only': _design(k=_K_ONLY, c=_K_ONLY),
    'mech_tau4': _design(c=_TAU4),
    'mech_tau4_snap': _design(c=_TAU4, snap=_SNAP4),
    'mech_tau7_snap': _design(c=_TAU7, snap=_SNAP7),
    'mech_tau4_sealed': _design(c=_TAU4),
})
SEALED = frozenset({'mech_tau4_sealed'})
ALIASES = MappingProxyType({'mechanical_encoding': 'mech_tau4'})

FEATURE_SCENARIOS = ('series_current', 'series_thin', 'star_graded', 'star_orifice') + tuple(MECH_DESIGNS)
HIGH_RATE_SCENARIOS = MappingProxyType({'star_graded_1khz': 'star_graded'})


def _chambers(base):
    return tuple(Chamber(name, vol * 1e-6) for name, vol in base.volumes_cm3)


def _walls(base, chambers, multipliers=None, k_scale=1.0):
    names = [c.name for c in chambers]
    mult = multipliers or {}

    def wall(zone, name):
        km, cm, snap = mult.get(name, (1.0, 1.0, None))
        c = base.wall_c * cm
        ratio = base.c_out_ratio if snap is None else snap
        return Wall(chamber=names.index(name), zone=zone, area=base.wall_area,
                    k=base.wall_k * km * k_scale, c_in=c, c_out=c * ratio)
    return tuple(wall(z, ZONE_CHAMBER[z]) for z in sorted(ZONE_CHAMBER))


def _network(base, chambers, channels, sensor_name, walls, sealed=False):
    names = [c.name for c in chambers]
    sensor = names.index(sensor_name)
    leak = None if sealed else Leak(sensor, base.leak_r, base.leak_k)
    return Network(chambers=chambers, channels=tuple(channels), walls=walls, leak=leak, sensor=sensor)


def series(base, d_mm, length_mm, multipliers=None, k_scale=1.0, sealed=False):
    chambers = _chambers(base)
    names = [c.name for c in chambers]
    pairs = zip(SERIES_ORDER[:-1], SERIES_ORDER[1:])
    channels = [Channel(names.index(a), names.index(b), d_mm * 1e-3, length_mm * 1e-3) for a, b in pairs]
    return _network(base, chambers, channels, SERIES_ORDER[0],
                    _walls(base, chambers, multipliers, k_scale), sealed)


def star(base, geometry, k_scale=1.0):
    """geometry: mapping leaf chamber -> (d_mm, L_mm) from the hub."""
    chambers = _chambers(base)
    names = [c.name for c in chambers]
    hub = names.index(STAR_HUB)
    channels = [Channel(hub, names.index(leaf), d * 1e-3, length * 1e-3)
                for leaf, (d, length) in geometry.items()]
    return _network(base, chambers, channels, STAR_HUB, _walls(base, chambers, k_scale=k_scale))


def lumped(base):
    """Single chamber holding the whole air volume, all seven walls on it.

    Close to series_current at 40 SPS: its 2.5 mm holes equalise within about
    a millisecond during the press, although orifice losses reach several kPa
    during the fast release snap-back. Used by the calibration search.
    """
    chamber = (Chamber('all', base.total_volume_m3),)
    walls = tuple(Wall(0, w.zone, w.area, w.k, w.c_in, w.c_out)
                  for w in _walls(base, _chambers(base)))
    return Network(chambers=chamber, channels=(), walls=walls,
                   leak=Leak(0, base.leak_r, base.leak_k), sensor=0)


def custom(base, topology, hole_d_mm, hole_l_mm, wall_k_scale=1.0):
    if topology == 'series':
        return series(base, hole_d_mm, hole_l_mm, k_scale=wall_k_scale)
    if topology == 'star':
        leaves = [n for n, _ in base.volumes_cm3 if n != STAR_HUB]
        return star(base, {leaf: (hole_d_mm, hole_l_mm) for leaf in leaves}, k_scale=wall_k_scale)
    raise ValueError(f'unknown topology {topology!r}')


def build(name, base):
    builders = {
        'series_current': lambda: series(base, 2.5, 1.5),
        'series_thin': lambda: series(base, 1.2, 25.0),
        'star_graded': lambda: star(base, STAR_GRADED),
        'star_orifice': lambda: star(base, {leaf: (0.6, 5.0) for leaf in STAR_GRADED}),
    }
    key = ALIASES.get(name, HIGH_RATE_SCENARIOS.get(name, name))
    if key in MECH_DESIGNS:  # series_current connectivity, per-zone wall mechanics
        return series(base, 2.5, 1.5, MECH_DESIGNS[key], sealed=key in SEALED)
    if key not in builders:
        raise ValueError(f'unknown scenario {name!r}')
    return builders[key]()
