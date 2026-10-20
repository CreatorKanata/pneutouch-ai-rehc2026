"""Batched linearly-implicit Euler integrator for a pneumatic Network.

Channel time constants range from nanoseconds (2.5 mm holes) to milliseconds,
so explicit RK4 at dt = 0.2 ms would be unstable. Each step solves the
backward-Euler system with channel/leak flows linearised about the current
state (one Newton step), which is L-stable and exact for linear parts.
"""
from dataclasses import dataclass
from typing import Optional, Sequence

import numpy as np

from . import physics

DT = 2e-4


@dataclass(frozen=True)
class Press:
    """Finger press on one zone: smooth ramp up, hold, smooth ramp down [s, N]."""
    zone: int
    start: float
    rise: float
    hold: float
    release: float
    force: float


@dataclass(frozen=True)
class Result:
    time: np.ndarray                 # (T,)
    sensor_pa: np.ndarray            # (B, T)
    chamber_pa: Optional[np.ndarray]  # (B, n, T) when requested


def smoothstep(u):
    u = np.clip(u, 0.0, 1.0)
    return u * u * (3.0 - 2.0 * u)


def force_at(t, start, rise, hold, release, force):
    """Vectorised force profile over batch arrays at scalar time t."""
    up = smoothstep((t - start) / rise)
    down = smoothstep((t - start - rise - hold) / release)
    return force * (up - down)


def _batch_arrays(presses):
    keys = ('start', 'rise', 'hold', 'release', 'force')
    return {k: np.array([getattr(p, k) for p in presses], dtype=float) for k in keys}


def _stack(values):
    """(m,) for scalar wall parameters, (B, m) when each is a per-batch (B,) array."""
    return np.stack([np.asarray(v, dtype=float) for v in values], axis=-1)


def _static(net):
    walls = net.walls
    n, m = len(net.chambers), len(walls)
    incidence = np.zeros((len(net.channels), n))
    for e, ch in enumerate(net.channels):
        incidence[e, ch.a], incidence[e, ch.b] = 1.0, -1.0
    owner = np.zeros((n, m))
    for j, w in enumerate(walls):
        owner[w.chamber, j] = 1.0
    return dict(
        cg=np.array([physics.gas_compliance(c.volume_m3) for c in net.chambers]),
        incidence=incidence, owner=owner,
        ch=np.array([w.chamber for w in walls]),
        area=_stack(w.area for w in walls), kw=_stack(w.k for w in walls),
        c_in=_stack(w.c_in for w in walls), c_out=_stack(w.c_out for w in walls),
        r=np.array([c.r for c in net.channels]), k=np.array([c.k for c in net.channels]))


def _channel_terms(s, p):
    """Return the channel Laplacian (B,n,n) and its affine rhs correction (B,n)."""
    dp = p @ s['incidence'].T
    q = physics.flow(dp, s['r'], s['k'])
    g = physics.conductance(q, s['r'], s['k'])
    lap = (s['incidence'].T[None] * g[:, None, :]) @ s['incidence']
    return lap, (q - g * dp) @ s['incidence']


def _leak_terms(leak, p, n):
    diag, corr = np.zeros_like(p), np.zeros_like(p)
    if leak is None:
        return diag, corr
    pl = p[:, leak.chamber]
    q = physics.flow(pl, leak.r, leak.k)
    g = physics.conductance(q, leak.r, leak.k)
    diag[:, leak.chamber] = g
    corr[:, leak.chamber] = q - g * pl
    return diag, corr


def simulate(net, presses: Sequence[Press], duration, record_dt=0.025, dt=DT, record_all=False):
    """Integrate a batch of independent presses on the same network.

    Wall and leak parameters may be scalars or (B,) arrays (one per press);
    the latter lets calibration evaluate many candidate parameter sets at once.
    """
    s = _static(net)
    b, n, m = len(presses), len(net.chambers), len(net.walls)
    prof = _batch_arrays(presses)
    onehot = np.zeros((b, m))
    onehot[np.arange(b), [net.wall_index(p.zone) for p in presses]] = 1.0
    every = int(round(record_dt / dt))
    if abs(every * dt - record_dt) > 1e-9:
        raise ValueError('record_dt must be an integer multiple of dt')
    steps = int(round(duration / dt))
    nrec = steps // every + 1
    p, x = np.zeros((b, n)), np.zeros((b, m))
    out = np.zeros((b, n, nrec))
    for step in range(1, steps + 1):
        f = force_at(step * dt, **prof)[:, None] * onehot
        drive = f - s['kw'] * x - s['area'] * p[:, s['ch']]
        c = np.where(drive >= 0.0, s['c_in'], s['c_out'])
        alpha = 1.0 / (1.0 + dt * s['kw'] / c)
        diag = s['cg'] / dt + (s['area']**2 * alpha / c) @ s['owner'].T
        rhs = s['cg'] / dt * p + (s['area'] * ((alpha - 1.0) * x / dt + alpha * f / c)) @ s['owner'].T
        leak_diag, leak_corr = _leak_terms(net.leak, p, n)
        diag, rhs = diag + leak_diag, rhs - leak_corr
        if len(net.channels):
            lap, corr = _channel_terms(s, p)
            mat = lap + diag[:, :, None] * np.eye(n)
            p = np.linalg.solve(mat, (rhs - corr)[..., None])[..., 0]
        else:
            p = rhs / diag
        x = alpha * (x + dt * (f - s['area'] * p[:, s['ch']]) / c)
        if step % every == 0:
            out[:, :, step // every] = p
    time = np.arange(nrec) * record_dt
    return Result(time=time, sensor_pa=out[:, net.sensor, :].copy(),
                  chamber_pa=out if record_all else None)
