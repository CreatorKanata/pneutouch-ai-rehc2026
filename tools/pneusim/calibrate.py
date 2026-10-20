"""Coarse random search fitting the lumped model to one real direct tail press.

The model is scenarios.lumped(): the whole air volume as one chamber with all
seven walls and the sensor leak (series_current's 2.5 mm holes equalise in
about a millisecond during the press, so this approximates it at 40 SPS).
"""
import numpy as np

from . import defaults, simulate
from .network import Chamber, Leak, Network, Wall

FINE_DT = 0.001       # record step used for sub-sample alignment [s]
SAMPLE_DT = 0.025
PRESS_START = 0.3
DURATION = 2.6
SHIFTS = np.arange(0, 121)   # candidate alignment offsets in FINE_DT steps
TARGET_DV = 3e-6             # rough displaced volume implied by the trough [m^3]
F0_RANGE = (1.0, 20.0)       # plausible fingertip force [N]

# name: (low, high) for log-uniform sampling
RANGES = {
    'leak_r': (1e7, 3e9), 'leak_k': (1e10, 1e15), 'wall_k': (200.0, 2e4),
    'tau_in': (0.01, 1.0), 'c_out_ratio': (0.005, 1.0), 'wall_area': (3e-5, 1e-3),
    'press_rise': (0.05, 0.4), 'press_hold': (0.2, 1.0), 'press_release': (0.02, 0.2),
}


def real_trace():
    return np.array(defaults.CALIBRATION_TRACE_KCOUNTS, dtype=float) * 1000.0


def lobe_weights(real):
    """Normalise the positive lobe by its peak and the release lobe by its trough."""
    first_negative = int(np.argmax(real < 0))
    return np.where(np.arange(len(real)) < first_negative, 1.0 / real.max(), 1.0 / -real.min())


def _network(cand, base):
    """Lumped chamber whose wall/leak fields are (B,) arrays from `cand`."""
    c_in = cand['tau_in'] * cand['wall_k']
    walls = tuple(Wall(0, z, cand['wall_area'], cand['wall_k'], c_in, c_in * cand['c_out_ratio'])
                  for z in range(1, 8))
    return Network(chambers=(Chamber('all', base.total_volume_m3),), channels=(), walls=walls,
                   leak=Leak(0, cand['leak_r'], cand['leak_k']), sensor=0)


def simulate_candidates(cand, base):
    presses = [simulate.Press(1, PRESS_START, r, h, rel, f) for r, h, rel, f in
               zip(cand['press_rise'], cand['press_hold'], cand['press_release'], cand['f0'])]
    result = simulate.simulate(_network(cand, base), presses, DURATION, record_dt=FINE_DT)
    return result.sensor_pa * base.counts_per_pa


def score(sim_counts, real, fixed_scale=False):
    """Best alignment per candidate: (weighted RMS residual, LS scale, shift index)."""
    w2 = lobe_weights(real)**2
    stride = int(round(SAMPLE_DT / FINE_DT))
    idx = SHIFTS[:, None] + stride * np.arange(len(real))[None, :]
    seg = sim_counts[:, idx]                                   # (B, S, N)
    scale = np.ones(seg.shape[:2]) if fixed_scale else \
        np.sum(w2 * seg * real, axis=2) / np.maximum(np.sum(w2 * seg * seg, axis=2), 1e-30)
    scale = np.clip(scale, 0.0, None)
    rms = np.sqrt(np.mean(w2 * (scale[..., None] * seg - real)**2, axis=2))
    best = np.argmin(rms, axis=1)
    rows = np.arange(len(best))
    return rms[rows, best], scale[rows, best], best


def _sample(rng, n):
    cand = {k: np.exp(rng.uniform(np.log(lo), np.log(hi), n)) for k, (lo, hi) in RANGES.items()}
    dv = TARGET_DV * np.exp(rng.uniform(np.log(0.3), np.log(3.0), n))
    return _rescaled({**cand, 'f0': np.ones(n)}, dv * cand['wall_k'] / cand['wall_area'])


def _perturb(rng, best, n, sigma, fixed):
    picks = rng.integers(0, len(best['f0']), n)
    out = {}
    for k, v in best.items():
        jitter = 0.0 if k in fixed else rng.normal(0.0, sigma, n)
        lo, hi = RANGES.get(k, F0_RANGE if k == 'f0' else (1e-12, 1e12))
        out[k] = np.clip(v[picks] * np.exp(jitter), lo, hi)
    return out


def _take(cand, ix):
    return {k: v[ix] for k, v in cand.items()}


def _rescaled(cand, scale):
    """Apply the LS amplitude scale to F0, keeping F0 in a plausible finger range.

    Whatever F0 cannot absorb goes into the wall stiffness (k -> k/excess with
    tau_in fixed), which preserves the static displaced volume A F / k.
    """
    wanted = cand['f0'] * scale
    f0 = np.clip(wanted, *F0_RANGE)
    excess = wanted / f0
    return {**cand, 'f0': f0, 'wall_k': cand['wall_k'] / excess}


def fit(base=defaults.BASELINE, seed=0, coarse=3000, rounds=4, per_round=1500, fixed=None):
    """Random search + local refinement. `fixed` maps parameter -> held value."""
    rng = np.random.default_rng(seed)
    real = real_trace()
    fixed = dict(fixed or {})
    cand = {**_sample(rng, coarse), **{k: np.full(coarse, v) for k, v in fixed.items()}}
    rms, scale, _ = score(simulate_candidates(cand, base), real)
    cand = _rescaled(cand, scale)
    sigma = 0.35
    for _ in range(rounds):
        elite = _take(cand, np.argsort(rms)[:20])
        trial = _perturb(rng, elite, per_round, sigma, fixed)
        trial_rms, trial_scale, _ = score(simulate_candidates(trial, base), real)
        trial = _rescaled(trial, trial_scale)
        cand = {k: np.r_[elite[k], trial[k]] for k in cand}
        rms = np.r_[np.sort(rms)[:20], trial_rms]
        sigma *= 0.6
    # Exact re-evaluation of the leaders at their rescaled force (no free scale).
    leaders = _take(cand, np.argsort(rms)[:40])
    sim = simulate_candidates(leaders, base)
    true_rms, _, shift = score(sim, real, fixed_scale=True)
    i = int(np.argmin(true_rms))
    best = {k: float(v[i]) for k, v in leaders.items()}
    return best, float(true_rms[i]), sim[i], int(shift[i])


def to_baseline(best, rms, base=defaults.BASELINE):
    return base.with_(leak_r=best['leak_r'], leak_k=best['leak_k'], wall_k=best['wall_k'],
                      wall_c=best['tau_in'] * best['wall_k'], c_out_ratio=best['c_out_ratio'],
                      wall_area=best['wall_area'], f0=best['f0'], press_rise=best['press_rise'],
                      press_hold=best['press_hold'], press_release=best['press_release'],
                      calibration_residual=rms)


def aligned(sim_fine, shift, n):
    stride = int(round(SAMPLE_DT / FINE_DT))
    return sim_fine[SHIFTS[shift] + stride * np.arange(n)]
