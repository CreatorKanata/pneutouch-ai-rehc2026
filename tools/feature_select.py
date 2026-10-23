"""float32 reference of the on-chip feature selection (feature_select.c).

The chip ranks 14 pool features (features12 + peak + trough, all log) by the
Fisher score over the four classes, keeps the best ones, normalizes
with mean and population std, and trains Solist-AI in a fixed order.
This module mirrors that arithmetic in numpy float32 so a training JSON can
be previewed on the PC before it is loaded with tools/model_store.py.

Run:
    .venv/bin/python tools/feature_select.py docs/analysis/.../pressure-demo-training.json [--k 0]
"""
import argparse
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import prepare_retrain  # noqa: E402
import retrain_report  # noqa: E402

POOL_COUNT = 14
SELECT_MAX = 12
CLASSES = 4
ALL_CLASSES = 0xF  # the firmware always uses every class (one Solist-AI instance)
FEATURE_NAMES = prepare_retrain.FEATURE_ORDER + ['peak', 'trough']
TRAIN_PRIMES = (3, 5, 7, 11, 13, 17, 19, 23)
TRAIN_EPOCHS = 4
MIN_SCALE = np.float32(1e-6)
AUTO_DIVISOR = np.float32(10)
F32 = np.float32


def trough_of(event):
    """Positive trough magnitude in counts; the PAI1 rule when the JSON has none."""
    return event.get('trough', event['peak'] * event['features12'][10])


def pool_rows(events):
    """Raw (pre-log) pool rows, float32: features12 + peak + trough."""
    return np.array([list(e['features12']) + [e['peak'], trough_of(e)] for e in events], dtype=F32)


def log_pool(events):
    """float32 logarithm of every pool value (what the chip feeds the statistics)."""
    return np.log(pool_rows(events))


def _valid_row(row):
    row = np.asarray(row, dtype=F32)
    return row.shape == (POOL_COUNT,) and bool(np.isfinite(row).all() and (row > 0).all())


def stats(labels, pool):
    """Streaming per-class sums of log values in record order (pneu_stats_add)."""
    n = np.zeros(CLASSES, dtype=np.int64)
    total = np.zeros((CLASSES, POOL_COUNT), dtype=F32)
    squares = np.zeros((CLASSES, POOL_COUNT), dtype=F32)
    for label, row in zip(labels, pool):
        if label not in (1, 2, 3, 4) or not _valid_row(row):
            raise ValueError(f'invalid record: label={label}')
        logged = np.log(np.asarray(row, dtype=F32))
        total[label - 1] = total[label - 1] + logged
        squares[label - 1] = squares[label - 1] + logged * logged
        n[label - 1] += 1
    return dict(n=n, sum=total, sumsq=squares)


def rank(fisher):
    """Pool indices by Fisher score descending; ties keep the lower index first."""
    return sorted(range(POOL_COUNT), key=lambda j: (-float(fisher[j]), j))


def _scores(s, classes):
    count = F32(sum(int(s['n'][c]) for c in classes))
    fisher = np.zeros(POOL_COUNT, dtype=F32)
    mean = np.zeros(POOL_COUNT, dtype=F32)
    scale = np.zeros(POOL_COUNT, dtype=F32)
    for j in range(POOL_COUNT):
        total = squares = within = between = F32(0)
        for c in classes:
            total = F32(total + s['sum'][c][j])
            squares = F32(squares + s['sumsq'][c][j])
        mean[j] = F32(total / count)
        for c in classes:
            nc = F32(s['n'][c])
            sc = s['sum'][c][j]
            within = F32(within + F32(s['sumsq'][c][j] - F32(F32(sc * sc) / nc)))
            d = F32(F32(sc / nc) - mean[j])
            between = F32(between + F32(F32(nc * d) * d))
        var = F32(F32(squares / count) - F32(mean[j] * mean[j]))
        scale[j] = F32(np.sqrt(max(var, F32(0)), dtype=F32))
        fisher[j] = F32(between / within) if scale[j] > MIN_SCALE and within > 0 else F32(0)
    return fisher, mean, scale


def _pick(order, fisher, scale, k):
    eligible = [j for j in order if scale[j] > MIN_SCALE]
    if k:
        return eligible[:k]
    floor = F32(fisher[order[0]] / AUTO_DIVISOR)
    chosen = [j for j in eligible if fisher[j] > 0 and fisher[j] >= floor]
    return eligible[:max(2, min(len(chosen), SELECT_MAX))]


def select(s, class_mask, k=0):
    """dict(index, fisher, mean, scale), or None when the chip would refuse."""
    classes = [c for c in range(CLASSES) if class_mask & (1 << c) and s['n'][c] > 0]
    if k > SELECT_MAX or len(classes) < 2 or sum(int(s['n'][c]) for c in classes) < 4:
        return None
    fisher, mean, scale = _scores(s, classes)
    order = rank(fisher)
    if not fisher[order[0]] > 0:
        return None
    return dict(index=_pick(order, fisher, scale, k), fisher=fisher, mean=mean, scale=scale)


def words(selection, pool_row):
    """12 bf16 words (float32 high halves) as the chip feeds Solist-AI; zero-filled."""
    out = np.zeros(SELECT_MAX, dtype=np.uint16)
    for s, j in enumerate(selection['index']):
        value = F32(pool_row[j])
        if not (np.isfinite(value) and value > 0):
            raise ValueError(f'invalid pool value {j}')
        scaled = np.array([(np.log(value) - selection['mean'][j]) / selection['scale'][j]], dtype=F32)
        if not np.isfinite(scaled).all():
            raise ValueError(f'nonfinite word {j}')
        out[s] = int(scaled.view(np.uint32)[0] >> 16)
    return out


def train_index(n, epoch, i):
    """Record visited at step i of an epoch: a fixed stride permutation per epoch."""
    if n == 0:
        return 0
    strides = [p for p in TRAIN_PRIMES if n % p]
    stride = strides[epoch] if epoch < len(strides) else 1
    return (i * stride + epoch * 37 % n) % n


def train_order(n):
    return [train_index(n, e, i) for e in range(TRAIN_EPOCHS) for i in range(n)]


def loo_accuracy(x, labels):
    preds = retrain_report.loo_predictions(x, labels)
    return float(np.mean(np.array(preds) == np.asarray(labels)))


def report(events, k=0):
    """Text lines: ranking, selections and LOO preview (used by the CLI)."""
    labels = [e['label'] for e in events]
    pool = pool_rows(events)
    s = stats(labels, pool)
    chosen = select(s, ALL_CLASSES, k)
    if chosen is None:
        return ['selection failed (need >= 4 records in >= 2 classes and a nonzero score)']
    lines = [f'records: {len(events)}  per class: {s["n"].tolist()}', '', 'ranking (Fisher):']
    lines += [f'  {FEATURE_NAMES[j]:>22}  {float(chosen["fisher"][j]):.4f}' for j in rank(chosen['fisher'])]
    lines.append(f'selected ({len(chosen["index"])}): ' + ', '.join(FEATURE_NAMES[j] for j in chosen['index']))
    logged = np.log(pool.astype(np.float64))
    lines.append(f'LOO nearest centroid, all 14 log features: {loo_accuracy(logged, labels) * 100:.1f}%')
    lines.append(f'LOO nearest centroid, selected features:  '
                 f'{loo_accuracy(logged[:, chosen["index"]], labels) * 100:.1f}%')
    return lines


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('training', type=Path)
    parser.add_argument('--k', type=int, default=0, help='0 = automatic, 1..12 = keep the best k')
    args = parser.parse_args(argv)
    if not 0 <= args.k <= SELECT_MAX:
        parser.error('--k must be 0..12')
    events = json.loads(args.training.read_text(encoding='utf-8'))['events']
    print('\n'.join(report(events, args.k)))


if __name__ == '__main__':
    main()
