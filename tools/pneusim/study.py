"""Randomised press campaigns per scenario and the separability metrics."""
from dataclasses import dataclass

import numpy as np

from . import features, scenarios, simulate

QUIET_S = 0.5
EVENT_S = 4.0
TIMING = {'rise': (0.10, 0.30), 'hold': (0.5, 1.0), 'release': (0.05, 0.15), 'force': (0.5, 1.5)}


@dataclass(frozen=True)
class Campaign:
    time: np.ndarray       # (T,)
    zones: np.ndarray      # (B,)
    counts: np.ndarray     # (B, T) delta counts incl. noise
    chamber_pa: object     # (B, n, T) or None


def random_presses(rng, base, per_zone, timing=TIMING):
    def u(key, n):
        return rng.uniform(*timing[key], n)
    zones = np.repeat(np.arange(1, 8), per_zone)
    n = len(zones)
    return [simulate.Press(int(z), QUIET_S, r, h, rel, f * base.f0)
            for z, r, h, rel, f in zip(zones, u('rise', n), u('hold', n), u('release', n), u('force', n))]


def run_campaign(net, base, rng, per_zone=40, record_dt=0.025, record_all=False, noise=True):
    presses = random_presses(rng, base, per_zone)
    result = simulate.simulate(net, presses, EVENT_S, record_dt=record_dt, record_all=record_all)
    counts = result.sensor_pa * base.counts_per_pa
    if noise:
        counts = counts + rng.normal(0.0, base.noise_counts, counts.shape)
    return Campaign(result.time, np.array([p.zone for p in presses]), counts, result.chamber_pa)


def feature_table(campaign):
    rows = [features.event_features(campaign.time, y, int(z))
            for y, z in zip(campaign.counts, campaign.zones)]
    valid = np.array([r is not None for r in rows])
    x = np.log(np.array([r for r in rows if r is not None])) if valid.any() else np.zeros((0, 12))
    return x, valid


def separability(campaign):
    x, valid = feature_table(campaign)
    zones = campaign.zones[valid]
    groups = np.array([scenarios.ZONE_GROUP[int(z)] for z in zones])
    hl = np.isin(groups, (3, 4))
    eta = features.eta_squared(x, zones) if len(zones) else np.zeros(12)
    return dict(
        acc7=features.loo_accuracy(x, zones), acc4=features.loo_accuracy(x, groups),
        acc_head_legs=features.loo_accuracy(x[hl], groups[hl]),
        invalid_rate=float(1.0 - valid.mean()),
        mean_peak_counts=float(np.mean(campaign.counts.max(axis=1))),
        mean_trough_counts=float(np.mean(-campaign.counts.min(axis=1))),
        n_valid=int(valid.sum()), eta_squared=dict(zip(features.FEATURES, map(float, eta))),
        x=x, zones=zones)


def rise_time(t, y):
    """10-90 % rise time of the first positive lobe (None if not resolved)."""
    peak = int(np.argmax(y))
    a = y[peak]
    lo = features.analysis.crossing(t, y, 0.1 * a, 0, peak, True, True)
    hi = features.analysis.crossing(t, y, 0.9 * a, 0, peak, True, True)
    return None if lo is None or hi is None else hi - lo


def high_rate_table(campaign, net):
    """Per zone: sensor rise time and sensor-vs-pressed-chamber 50 % lag [ms]."""
    out = {}
    for z in range(1, 8):
        ix = np.flatnonzero(campaign.zones == z)
        pressed = net.walls[net.wall_index(z)].chamber
        rises, lags = [], []
        for i in ix:
            y = campaign.chamber_pa[i, net.sensor]
            src = campaign.chamber_pa[i, pressed]
            r = rise_time(campaign.time, y)
            ts = features.analysis.crossing(campaign.time, y, 0.5 * y.max(), 0, int(np.argmax(y)), True)
            tp = features.analysis.crossing(campaign.time, src, 0.5 * src.max(), 0, int(np.argmax(src)), True)
            if r is not None:
                rises.append(r * 1000)
            if ts is not None and tp is not None:
                lags.append((ts - tp) * 1000)
        out[z] = dict(rise_ms_mean=float(np.mean(rises)), rise_ms_sd=float(np.std(rises)),
                      lag_ms_mean=float(np.mean(lags)), lag_ms_sd=float(np.std(lags)))
    return out
