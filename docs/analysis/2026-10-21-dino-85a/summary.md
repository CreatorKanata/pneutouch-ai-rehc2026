# Retraining data for the new dinosaur

- recordings used: 4; events kept: 80; excluded: 0
- feature source: **replay** `/Users/hide/src/github.com/CreatorKanata/pneutouch-ai-rehc2026/build/replay_pressure_features` (sha256 `9b75886c3dabc4162847dbb545011ca718bb7191a042116daf59abff7758582b`)

## Per zone

| zone | files | started | completed | invalid | timeout | kept | decay ms | width ms | recovery ms | release width ms | release rise ms | trough/peak | peak (k) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| TAIL | 1 | 25 | 25 | 0 | 0 | 25 | 185 | 195 | 59 | 84 | 65 | 2.62 | 173 |
| BACK | 1 | 22 | 21 | 0 | 0 | 21 | 113 | 122 | 44 | 104 | 180 | 1.19 | 736 |
| LEGS | 1 | 14 | 14 | 0 | 0 | 14 | 100 | 130 | 55 | 84 | 462 | 2.66 | 65 |
| HEAD | 1 | 20 | 20 | 0 | 0 | 20 | 64 | 77 | 44 | 62 | 61 | 1.41 | 108 |

## Old firmware model (95A dinosaur) on the new dinosaur

- accuracy: **41.1%** over 56 kept events with a PNEC1 prediction (24 without)

| true \ predicted | TAIL | BACK | LEGS | HEAD | none |
|---|---:|---:|---:|---:|---:|
| TAIL | 23 | 0 | 0 | 0 | 2 |
| BACK | 21 | 0 | 0 | 0 | 0 |
| LEGS | 0 | 0 | 0 | 0 | 14 |
| HEAD | 12 | 0 | 0 | 0 | 8 |

## Leave-one-out nearest centroid (12 log features)

- 4 zones: **87.5%** (chance 25%)
- HEAD vs LEGS: 94.1%
- TAIL vs BACK: 95.7%

| true \ predicted | TAIL | BACK | LEGS | HEAD |
|---|---:|---:|---:|---:|
| TAIL | 24 | 1 | 0 | 0 |
| BACK | 0 | 20 | 0 | 1 |
| LEGS | 5 | 0 | 8 | 1 |
| HEAD | 2 | 0 | 0 | 18 |

## v2 check (HEAD ÷ LEGS medians, ✔ if the contrast in either direction is ≥ 2.0)

- decay_ms: HEAD 63.65 ÷ LEGS 100.14 = **0.64** (contrast 1.6x)
- width_ms: HEAD 77.25 ÷ LEGS 130.05 = **0.59** (contrast 1.7x)
- release_rise_ms: HEAD 60.68 ÷ LEGS 461.75 = **0.13** (contrast 7.6x) ✔
- trough_peak_ratio: HEAD 1.41 ÷ LEGS 2.66 = **0.53** (contrast 1.9x)

## Readiness

- **ready**: 80 events. Next:

```
.venv/bin/python tools/generate_pressure_model.py --input docs/analysis/2026-10-21-dino-85a/pressure-demo-training.json
```

## Files

- `pressure-demo-training.json`, `events.csv` (all events with exclusion reason), `waveforms.png` (mean ± 1σ, baseline-subtracted, aligned at trigger)
