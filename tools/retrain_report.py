"""summary.md and waveforms.png for tools/prepare_retrain.py.

Accuracies reuse analyze_test_pieces (leave-one-out nearest centroid on the
12 log features) so they are comparable with the test-piece study.
"""
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import analyze_test_pieces as atp  # noqa: E402

ORDER = (1, 2, 3, 4)  # TAIL, BACK, LEGS, HEAD
MEDIAN_KEYS = ('decay_ms', 'width_ms', 'recovery_ms', 'release_width_ms', 'release_rise_ms', 'trough_peak_ratio')
V2_KEYS = ('decay_ms', 'width_ms', 'release_rise_ms', 'trough_peak_ratio')
V2_RATIO = 2.0
PAIRS = ((4, 3), (1, 2))  # HEAD vs LEGS, TAIL vs BACK
GRID_S = np.arange(-0.5, 3.0 + 1e-9, 0.025)
NO_PREDICTION = 'none'


def fmt_pct(v):
    return 'n/a' if v is None or not np.isfinite(v) else f'{v * 100:.1f}%'


def fmt_num(v, digits=0):
    return 'n/a' if v is None or not np.isfinite(v) else f'{v:.{digits}f}'


def feature(e, key, order):
    return e['features12'][order.index(key)]


def zone_medians(kept, order):
    """{label: {key: median}} for every zone present; peak included."""
    out = {}
    for z in ORDER:
        es = [e for e in kept if e['label'] == z]
        if es:
            med = {k: float(np.median([feature(e, k, order) for e in es])) for k in MEDIAN_KEYS}
            out[z] = dict(med, peak=float(np.median([e['peak'] for e in es])))
    return out


MEDIAN_HEADS = {'decay_ms': 'decay ms', 'width_ms': 'width ms', 'recovery_ms': 'recovery ms',
                'release_width_ms': 'release width ms', 'release_rise_ms': 'release rise ms',
                'trough_peak_ratio': 'trough/peak'}


def zone_table(ctx, med):
    names = ctx['zone_names']
    heads = [MEDIAN_HEADS.get(k, k) for k in MEDIAN_KEYS] + ['peak (k)']
    ncol = 6 + len(heads)
    lines = ['| zone | files | started | completed | invalid | timeout | kept | ' + ' | '.join(heads) + ' |',
             '|---|' + '---:|' * ncol]
    for z in ORDER:
        recs = [r for r in ctx['records'] if r['label'] == z]
        if not recs:
            lines.append(f'| {names[z]} | 0 | missing |' + ' |' * (ncol - 2))
            continue
        counts = [sum(r[k] for r in recs) for k in ('started', 'completed', 'invalid', 'timeout', 'kept')]
        m = med.get(z)
        values = ([fmt_num(m[k], 2 if k == 'trough_peak_ratio' else 0) for k in MEDIAN_KEYS]
                  + [fmt_num(m['peak'] / 1000)]) if m else ['n/a'] * len(heads)
        lines.append(f'| {names[z]} | {len(recs)} | ' + ' | '.join(map(str, counts + values)) + ' |')
    return lines


def confusion_lines(true, pred, names, columns):
    header = '| true \\ predicted | ' + ' | '.join(columns) + ' |'
    lines = [header, '|---|' + '---:|' * len(columns)]
    for z in ORDER:
        if z not in true:
            continue
        row = [sum(t == z and p == c for t, p in zip(true, pred)) for c in columns]
        lines.append(f'| {names[z]} | ' + ' | '.join(map(str, row)) + ' |')
    return lines


def firmware_section(kept, names):
    true = [e['label'] for e in kept]
    pred = [e['firmware_predicted'] or NO_PREDICTION for e in kept]
    scored = [(t, p) for t, p in zip(true, pred) if p != NO_PREDICTION]
    acc = np.mean([names[t] == p for t, p in scored]) if scored else float('nan')
    columns = [names[z] for z in ORDER] + sorted({p for p in pred} - {names[z] for z in ORDER})
    lines = ['## Old firmware model (95A dinosaur) on the new dinosaur', '',
             f'- accuracy: **{fmt_pct(acc)}** over {len(scored)} kept events with a PNEC1 prediction'
             f' ({len(pred) - len(scored)} without)', '']
    return lines + confusion_lines(true, pred, names, columns)


def loo_predictions(x, labels):
    """Same rule as analyze_test_pieces.loo_centroid, but returns each prediction."""
    labels = np.asarray(labels)
    preds = []
    for i in range(len(x)):
        train = np.ones(len(x), bool); train[i] = False
        mu, sd = x[train].mean(0), x[train].std(0) + 1e-9
        z = (x - mu) / sd
        cents = {c: z[train & (labels == c)].mean(0) for c in np.unique(labels[train])}
        preds.append(min(cents, key=lambda c: np.linalg.norm(z[i] - cents[c])))
    return preds


MODEL_PEAK_INPUT, MODEL_TROUGH_INPUT = 5, 10


def model_inputs(kept, order):
    """Raw (pre-log) model inputs: features12 with input 5 = peak/1000 and
    input 10 = trough/1000, trough = peak * trough_peak_ratio (as the firmware)."""
    x = np.array([e['features12'] for e in kept], dtype=float)
    peak = np.array([e['peak'] for e in kept], dtype=float)
    ratio = x[:, order.index('trough_peak_ratio')]
    x[:, MODEL_PEAK_INPUT] = peak / 1000
    x[:, MODEL_TROUGH_INPUT] = peak * ratio / 1000
    return x


def loo_lines(x, labels, names, title):
    """4-zone line, pair lines and (when defined) the confusion table."""
    acc4 = atp.subset_accuracy(x, labels, ORDER)
    lines = [f'- {title}: **{fmt_pct(acc4)}** (chance 25%)']
    for a, b in PAIRS:
        lines.append(f'- {names[a]} vs {names[b]}: {fmt_pct(atp.subset_accuracy(x, labels, (a, b)))}')
    if np.isfinite(acc4):
        columns = [names[z] for z in ORDER]
        pred = [names[p] for p in loo_predictions(x, labels)]
        lines += [''] + confusion_lines(list(labels), pred, names, columns)
    return lines


def loo_section(kept, names, order):
    lines = ['## Leave-one-out nearest centroid (12 log features)', '']
    if not kept:
        return lines + ['- no kept events: n/a']
    labels = np.array([e['label'] for e in kept])
    shape = np.log(np.array([e['features12'] for e in kept], dtype=float))
    lines += loo_lines(shape, labels, names, '4 zones')
    lines += ['', '*Model inputs as the firmware uses them: inputs 5/10 are log(peak/1000) and '
              'log(trough/1000) instead of asymmetry and trough/peak.*', '']
    lines += loo_lines(np.log(model_inputs(kept, order)), labels, names,
                       '4 zones, model inputs (peak/trough replace inputs 5/10)')
    return lines


def v2_section(med, names):
    """HEAD vs LEGS contrast in either direction: max(ratio, 1/ratio) >= V2_RATIO earns a check."""
    lines = [f'## v2 check (HEAD ÷ LEGS medians, ✔ if the contrast in either direction is ≥ {V2_RATIO})', '']
    for k in V2_KEYS:
        if 4 in med and 3 in med and med[3][k] > 0 and med[4][k] > 0:
            r = med[4][k] / med[3][k]
            contrast = max(r, 1 / r)
            lines.append(f'- {k}: {names[4]} {med[4][k]:.2f} ÷ {names[3]} {med[3][k]:.2f} = '
                         f'**{r:.2f}** (contrast {contrast:.1f}x){" ✔" if contrast >= V2_RATIO else ""}')
        else:
            lines.append(f'- {k}: n/a (HEAD or LEGS missing)')
    return lines


def readiness_section(ctx):
    names, kept = ctx['zone_names'], ctx['kept']
    counts = {z: sum(e['label'] == z for e in kept) for z in ORDER}
    short = [f'{names[z]} ({counts[z]}/{ctx["min_events"]})' for z in ORDER if counts[z] < ctx['min_events']]
    lines = ['## Readiness', '']
    if not short and len(kept) <= ctx['max_events']:
        return lines + [f'- **ready**: {len(kept)} events. Next:', '', '```',
                        f'.venv/bin/python tools/generate_pressure_model.py --input {ctx["training"]}', '```']
    if short:
        lines.append('- **not ready**: zones short of --min-events: ' + ', '.join(short))
    if len(kept) > ctx['max_events']:
        lines.append(f'- **not ready**: {len(kept)} events > {ctx["max_events"]} (model limit); drop some files')
    return lines


def input_section(ctx):
    excluded = [e for e in ctx['events'] if e['reason']]
    reasons = sorted({e['reason'] for e in excluded})
    lines = ['# Retraining data for the new dinosaur', '',
             f'- recordings used: {len(ctx["records"])}; events kept: {len(ctx["kept"])}; '
             f'excluded: {len(excluded)}'
             + (' (' + ', '.join(f'{r}={sum(e["reason"] == r for e in excluded)}' for r in reasons) + ')'
                if reasons else '')]
    src = ctx.get('source', dict(kind='telemetry'))
    lines.append(f'- feature source: **{src["kind"]}**' +
                 (f' `{src["exe"]}` (sha256 `{src["sha256"]}`)' if src['kind'] == 'replay' else ' (PNEF1)'))
    lines += [f'- skipped `{s["file"]}`: {s["reason"]}' for s in ctx['skipped']]
    return lines


def write_summary(ctx):
    names = ctx['zone_names']
    med = zone_medians(ctx['kept'], ctx['feature_order'])
    sections = [input_section(ctx), ['## Per zone', ''] + zone_table(ctx, med),
                firmware_section(ctx['kept'], names),
                loo_section(ctx['kept'], names, ctx['feature_order']),
                v2_section(med, names), readiness_section(ctx),
                ['## Files', '', '- `pressure-demo-training.json`, `events.csv` (all events with exclusion '
                 'reason), `waveforms.png` (mean ± 1σ, baseline-subtracted, aligned at trigger)']]
    text = '\n\n'.join('\n'.join(s) for s in sections) + '\n'
    (ctx['out'] / 'summary.md').write_text(text, encoding='utf-8')
    return text


def event_curve(e, ms, raw):
    lo, hi = e['trigger_ms'] + 1000 * GRID_S[0] - 100, e['trigger_ms'] + 1000 * GRID_S[-1] + 100
    keep = (ms >= lo) & (ms <= hi)
    if keep.sum() < 2:
        return None
    t = (ms[keep] - e['trigger_ms']) / 1000
    order = np.argsort(t, kind='stable')
    return np.interp(GRID_S, t[order], raw[keep][order] - e['baseline'])


def plot_waveforms(kept, folder, names, path, load_samples):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    cache = {f: load_samples(folder / f) for f in sorted({e['file'] for e in kept})}
    fig, ax = plt.subplots(figsize=(10, 5))
    colors = plt.cm.tab10(np.linspace(0, 1, 10))
    for ci, z in enumerate(ORDER):
        curves = [c for c in (event_curve(e, *cache[e['file']]) for e in kept if e['label'] == z)
                  if c is not None]
        if not curves:
            continue
        m, s = np.mean(curves, 0) / 1000, np.std(curves, 0) / 1000
        ax.plot(GRID_S, m, color=colors[ci], lw=2, label=f'{names[z]} (n={len(curves)})')
        ax.fill_between(GRID_S, m - s, m + s, color=colors[ci], alpha=0.15)
    ax.set_xlabel('time from firmware trigger [s]'); ax.set_ylabel('raw - baseline [k counts]')
    ax.grid(alpha=0.3)
    if ax.get_legend_handles_labels()[0]:
        ax.legend()
    ax.set_title('New dinosaur: mean ± 1σ per zone')
    fig.tight_layout(); fig.savefig(path, dpi=110); plt.close(fig)
