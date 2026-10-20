"""Calibrated "current dinosaur" baseline (see calibrate.py and parameters.json)."""
from dataclasses import dataclass, replace, asdict
from typing import Tuple

# Chamber volumes [cm^3]; approximate, override via --volumes.
VOLUMES_CM3: Tuple[Tuple[str, float], ...] = (
    ('back', 25.0), ('tail', 8.0), ('rh', 6.0), ('rf', 6.0), ('lf', 6.0), ('lh', 6.0), ('head', 12.0))

# Real direct tail press (2026-09-20, 25 ms/sample), delta counts / 1000.
CALIBRATION_TRACE_KCOUNTS = (
    12, 12, 13, 15, 19, 27, 43, 67, 116, 206, 344, 487, 593, 639, 610, 538, 479, 411, 359, 312,
    271, 249, 218, 185, 150, 118, 97, 79, 60, 39, 28, 19, 5, -7, -27, -51, -83, -159, -423,
    -1102, -1918, -2156, -1599, -928, -561, -365, -246, -165, -110, -73, -50, -35, -24, -17,
    -13, -9, -8, -7, -5, -5, -4, -2)
CALIBRATION_SOURCE = 'docs/analysis/2026-09-20-live-calibration/pressure-tail-20260920.csv'
CALIBRATION_PEAK_COUNTS = 639000.0


@dataclass(frozen=True)
class Baseline:
    leak_r: float          # sensor-chamber leak, laminar part [Pa s/m^3]
    leak_k: float          # sensor-chamber leak, orifice part [Pa s^2/m^6]
    wall_k: float          # wall stiffness [N/m]
    wall_c: float          # wall damping while pressed inward [N s/m]
    c_out_ratio: float     # damping ratio while the wall springs back out
    wall_area: float       # effective pressed area [m^2]
    f0: float              # nominal finger force [N]
    press_rise: float      # fitted timing of the calibration press [s]
    press_hold: float
    press_release: float
    counts_per_pa: float = 350.0
    noise_counts: float = 3000.0
    volumes_cm3: Tuple[Tuple[str, float], ...] = VOLUMES_CM3
    calibration_residual: float = float('nan')

    def volume_m3(self, name):
        return dict(self.volumes_cm3)[name] * 1e-6

    @property
    def total_volume_m3(self):
        return sum(v for _, v in self.volumes_cm3) * 1e-6

    def with_(self, **changes):
        return replace(self, **changes)

    def as_dict(self):
        return asdict(self)


# Output of `pneumatic_sim.py calibrate --seed 0` (lumped single chamber + leak).
# Weighted RMS residual (each lobe normalised by its own peak) = 0.041.
# With symmetric wall damping (c_out_ratio fixed at 1) the best residual is 0.124.
BASELINE = Baseline(
    leak_r=3.939e7, leak_k=1.405e12, wall_k=255.1, wall_c=0.1869 * 255.1, c_out_ratio=0.04681,
    wall_area=1.274e-4, f0=16.67, press_rise=0.0822, press_hold=0.6489, press_release=0.1298,
    calibration_residual=0.041)
SYMMETRIC_DAMPING_RESIDUAL = 0.124
