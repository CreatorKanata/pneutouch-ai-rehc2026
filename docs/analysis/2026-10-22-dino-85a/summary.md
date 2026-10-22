# Retraining data for the new dinosaur

- recordings used: 6; events kept: 117; excluded: 0
- feature source: **replay** `/Users/hide/src/github.com/CreatorKanata/pneutouch-ai-rehc2026/build/replay_pressure_features` (sha256 `f957dddaffc47fd8384752266886f3050f620ecc1c03ac8db3c398c58216fd92`)

## Per zone

| zone | files | started | completed | invalid | timeout | kept | decay ms | width ms | recovery ms | release width ms | release rise ms | trough/peak | peak (k) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| TAIL | 1 | 25 | 25 | 0 | 0 | 25 | 185 | 195 | 59 | 84 | 66 | 2.62 | 173 |
| BACK | 1 | 22 | 21 | 0 | 0 | 21 | 113 | 122 | 44 | 104 | 180 | 1.19 | 736 |
| LEGS | 3 | 51 | 51 | 0 | 0 | 51 | 143 | 205 | 53 | 79 | 68 | 3.18 | 47 |
| HEAD | 1 | 20 | 20 | 0 | 0 | 20 | 64 | 77 | 44 | 62 | 61 | 1.41 | 108 |

## Old firmware model (95A dinosaur) on the new dinosaur

- accuracy: **18.8%** over 16 kept events with a PNEC1 prediction (101 without)

| true \ predicted | TAIL | BACK | LEGS | HEAD | none |
|---|---:|---:|---:|---:|---:|
| TAIL | 3 | 0 | 0 | 0 | 22 |
| BACK | 5 | 0 | 0 | 0 | 16 |
| LEGS | 0 | 0 | 0 | 1 | 50 |
| HEAD | 7 | 0 | 0 | 0 | 13 |

## Leave-one-out nearest centroid (12 log features)

- 4 zones: **73.5%** (chance 25%)
- HEAD vs LEGS: 95.8%
- TAIL vs BACK: 95.7%

| true \ predicted | TAIL | BACK | LEGS | HEAD |
|---|---:|---:|---:|---:|
| TAIL | 21 | 1 | 3 | 0 |
| BACK | 0 | 20 | 1 | 0 |
| LEGS | 14 | 9 | 27 | 1 |
| HEAD | 2 | 0 | 0 | 18 |

*Model inputs as the firmware uses them: inputs 5/10 are log(peak/1000) and log(trough/1000) instead of asymmetry and trough/peak.*

- 4 zones, model inputs (peak/trough replace inputs 5/10): **94.9%** (chance 25%)
- HEAD vs LEGS: 95.8%
- TAIL vs BACK: 100.0%

| true \ predicted | TAIL | BACK | LEGS | HEAD |
|---|---:|---:|---:|---:|
| TAIL | 25 | 0 | 0 | 0 |
| BACK | 0 | 21 | 0 | 0 |
| LEGS | 2 | 1 | 46 | 2 |
| HEAD | 1 | 0 | 0 | 19 |

## v2 check (HEAD ÷ LEGS medians, ✔ if the contrast in either direction is ≥ 2.0)

- decay_ms: HEAD 63.63 ÷ LEGS 143.01 = **0.44** (contrast 2.2x) ✔
- width_ms: HEAD 77.25 ÷ LEGS 204.60 = **0.38** (contrast 2.6x) ✔
- release_rise_ms: HEAD 60.69 ÷ LEGS 67.62 = **0.90** (contrast 1.1x)
- trough_peak_ratio: HEAD 1.41 ÷ LEGS 3.18 = **0.44** (contrast 2.2x) ✔

## Readiness

- **ready**: 117 events. Next:

```
.venv/bin/python tools/generate_pressure_model.py --input docs/analysis/2026-10-22-dino-85a/pressure-demo-training.json
```

## Files

- `pressure-demo-training.json`, `events.csv` (all events with exclusion reason), `waveforms.png` (mean ± 1σ, baseline-subtracted, aligned at trigger)
