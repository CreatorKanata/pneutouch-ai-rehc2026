# Retraining data for the new dinosaur

- recordings used: 8; events kept: 141; excluded: 0
- feature source: **replay** `/Users/hide/src/github.com/CreatorKanata/pneutouch-ai-rehc2026/build/replay_pressure_features` (sha256 `f957dddaffc47fd8384752266886f3050f620ecc1c03ac8db3c398c58216fd92`)

## Per zone

| zone | files | started | completed | invalid | timeout | kept | decay ms | width ms | recovery ms | release width ms | release rise ms | trough/peak | peak (k) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| TAIL | 2 | 37 | 37 | 0 | 0 | 37 | 220 | 197 | 58 | 84 | 64 | 3.13 | 147 |
| BACK | 2 | 34 | 33 | 0 | 0 | 33 | 137 | 126 | 45 | 108 | 165 | 1.38 | 578 |
| LEGS | 3 | 51 | 51 | 0 | 0 | 51 | 143 | 205 | 53 | 79 | 68 | 3.18 | 47 |
| HEAD | 1 | 20 | 20 | 0 | 0 | 20 | 64 | 77 | 44 | 62 | 61 | 1.41 | 108 |

## Old firmware model (95A dinosaur) on the new dinosaur

- accuracy: **27.5%** over 40 kept events with a PNEC1 prediction (101 without)

| true \ predicted | TAIL | BACK | LEGS | HEAD | none |
|---|---:|---:|---:|---:|---:|
| TAIL | 7 | 0 | 8 | 0 | 22 |
| BACK | 13 | 4 | 0 | 0 | 16 |
| LEGS | 0 | 0 | 0 | 1 | 50 |
| HEAD | 7 | 0 | 0 | 0 | 13 |

## Leave-one-out nearest centroid (12 log features)

- 4 zones: **70.2%** (chance 25%)
- HEAD vs LEGS: 95.8%
- TAIL vs BACK: 88.6%

| true \ predicted | TAIL | BACK | LEGS | HEAD |
|---|---:|---:|---:|---:|
| TAIL | 31 | 4 | 2 | 0 |
| BACK | 3 | 28 | 2 | 0 |
| LEGS | 19 | 10 | 21 | 1 |
| HEAD | 1 | 0 | 0 | 19 |

*Model inputs as the firmware uses them: inputs 5/10 are log(peak/1000) and log(trough/1000) instead of asymmetry and trough/peak.*

- 4 zones, model inputs (peak/trough replace inputs 5/10): **88.7%** (chance 25%)
- HEAD vs LEGS: 95.8%
- TAIL vs BACK: 100.0%

| true \ predicted | TAIL | BACK | LEGS | HEAD |
|---|---:|---:|---:|---:|
| TAIL | 36 | 1 | 0 | 0 |
| BACK | 1 | 32 | 0 | 0 |
| LEGS | 10 | 0 | 38 | 3 |
| HEAD | 0 | 0 | 1 | 19 |

## v2 check (HEAD ÷ LEGS medians, ✔ if the contrast in either direction is ≥ 2.0)

- decay_ms: HEAD 63.63 ÷ LEGS 143.01 = **0.44** (contrast 2.2x) ✔
- width_ms: HEAD 77.25 ÷ LEGS 204.60 = **0.38** (contrast 2.6x) ✔
- release_rise_ms: HEAD 60.69 ÷ LEGS 67.62 = **0.90** (contrast 1.1x)
- trough_peak_ratio: HEAD 1.41 ÷ LEGS 3.18 = **0.44** (contrast 2.2x) ✔

## Readiness

- **ready**: 141 events. Next:

```
.venv/bin/python tools/generate_pressure_model.py --input docs/analysis/2026-10-23-dino-85a/pressure-demo-training.json
```

## Files

- `pressure-demo-training.json`, `events.csv` (all events with exclusion reason), `waveforms.png` (mean ± 1σ, baseline-subtracted, aligned at trigger)
