# pneusim: lumped pneumatic simulator for the PneuTouch dinosaur

Question it answers: which internal structure makes the single-sensor 40 SPS
pressure waveform most distinguishable by touch location, scored with the
firmware's 12 features (`tools/analyze_pressure.py` = `pressure_features.c`).
All results are simulated; they rank designs, they do not predict field accuracy.

## Model

- Chambers: gauge pressure p, adiabatic gas compliance C = V / (gamma p0).
- Walls (one per zone): quasi-static piston `c x' = F - k x - A p`, displacing
  `A x` into its chamber. Damping is `c_in` while sinking in and
  `c_out = c_out_ratio * c_in` while springing back (snap-back).
  `tau_in = c_in / k` is the sink-in time constant.
- Channels and the sensor leak: `dp = R Q + K Q|Q|`, Poiseuille R with 1.2 d
  end correction plus orifice K (Cd 0.6), solved in closed form.
- Finger: smooth ramp up / hold / ramp down, randomised strength and speed.
- Sensor: chamber pressure x `counts_per_pa` + Gaussian noise, sampled at 40 SPS.
- Integrator: linearly-implicit (backward) Euler, dt = 0.2 ms, batched over
  presses. Explicit RK4 is unstable because 2.5 mm holes have ns time constants.
- Baseline (`defaults.py`) is fitted to one real direct tail press
  (`calibrate.py`, residual 0.041 per-lobe normalised RMS).

## Run

```
.venv/bin/python tools/pneumatic_sim.py run --out docs/analysis/2026-10-15-pneumatic-sim/
.venv/bin/python tools/pneumatic_sim.py custom --topology star --hole-d 0.8 --hole-L 10 --volumes back=30
.venv/bin/python tools/pneumatic_sim.py calibrate     # ~40 s; prints values for defaults.py
.venv/bin/python -m unittest tests.test_pneumatic_sim
```

`run` writes `summary.md`, `parameters.json`, `calibration.png` and
`waveforms-*.png` / `features-*.png` per scenario (about 2.5 min at 40 events/zone;
`--per-zone 30` is faster).

## Adding a scenario

- Wall mechanics only (series_current connectivity): add an entry to
  `MECH_DESIGNS` in `scenarios.py` built with
  `_design(k={...}, c={...}, snap={...})`. Multipliers are per chamber
  (`tail back rh lh rf lf head`). Snap is the absolute c_out ratio, and leaving it
  out keeps the baseline value. Add the name to `SEALED` to remove the leak.
- New connectivity: add a builder lambda in `build()` using `series()` or
  `star()`, or construct a `network.Network` directly. Each of the 7 zones
  needs exactly one `Wall`. Then list it in `FEATURE_SCENARIOS`.

## Caveats

- Wall area, stiffness and force are identifiable only as A F / k and c / k.
- `c/k` is largely a material property for a homogeneous TPU wall: thickness
  changes k and c together. The mech_tau designs assume geometry (domes, ribs,
  bellows, lattice infill) can change the apparent sink-in time by 2x or more,
  which still needs measuring on printed test pieces.
- Gas compliance is linearised. Fast snap-back can drive a pressed chamber to
  -25 kPa (series_thin) or -73 kPa (star_orifice), outside that range.
- No wall mass, channel inertance or heat exchange. Channel delays (< 15 ms)
  are invisible at 40 SPS.
