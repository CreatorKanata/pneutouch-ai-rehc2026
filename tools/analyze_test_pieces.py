"""Compare the pneumatic test pieces (ti1..ti5 / tp1..tp5) from recorded CSVs.

Each CSV holds the presses of ONE piece, recorded with the learning tool or
src/learning-tool/capture.py. The piece is read from the file name prefix:

    captures/test-pieces/ti1-plain-20261020-1.csv
    captures/test-pieces/ti4-gap-1.5-20261020-1.csv

Run:
    .venv/bin/python tools/analyze_test_pieces.py captures/test-pieces --out docs/analysis/2026-10-20-test-pieces

Writes summary.md (per-piece medians, leave-one-out nearest-centroid accuracy,
go/no-go checks), events.csv, waveforms.png and features.png. Reuses the event
detection and the 12-feature extraction of tools/analyze_pressure.py, so the
numbers are comparable with the September dinosaur study.
"""
import argparse
import csv
import json
import re
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import analyze_pressure as ap  # noqa: E402

KEY = ['rise_ms', 'width_ms', 'decay_ms', 'release_width_ms', 'recovery_ms',
       'trough_peak_ratio', 'area_ratio', 'peak_counts', 'trough_counts']
SHAPE = ap.POS_SHAPE + ap.NEG_SHAPE + ap.RATIOS          # amplitude-free 12 features
TIME_KEYS = ['width_ms', 'decay_ms', 'recovery_ms', 'release_width_ms']
GO_PIECES = ('ti1', 'ti4', 'ti5')
MIN_GO_EVENTS = 7
GO_ACCURACY = 0.8


def piece_of(path):
    m = re.match(r'(t[ip]\d)', path.name)
    return m.group(1) if m else None


def load_events(folder):
    events, captures = [], {}
    for path in sorted(Path(folder).glob('*.csv')):
        piece = piece_of(path)
        if not piece:
            continue
        rows, seq, ms, t, y = ap.load(path)
        meta = dict(file=path.name, kind='seven_zone', zone_id=int(piece[2]), label_basis=f'file name {piece}')
        for e in ap.extract(meta, t, y):
            e['piece'] = piece
            events.append(e)
        captures[path.name] = (t, y)
    return events, captures


def loo_centroid(x, labels):
    """Leave-one-out nearest-centroid on log features standardised on the training part."""
    labels = np.asarray(labels)
    hits = 0
    for i in range(len(x)):
        train = np.ones(len(x), bool); train[i] = False
        mu, sd = x[train].mean(0), x[train].std(0) + 1e-9
        z = (x - mu) / sd
        cents = {c: z[train & (labels == c)].mean(0) for c in np.unique(labels[train])}
        pred = min(cents, key=lambda c: np.linalg.norm(z[i] - cents[c]))
        hits += pred == labels[i]
    return hits / len(x)


def subset_accuracy(x, labels, pieces, min_events=2):
    """Evaluate only the requested classes; never score a missing/undersampled class."""
    labels = np.asarray(labels)
    if len(pieces) < 2 or any(np.count_nonzero(labels == p) < min_events for p in pieces):
        return float('nan')
    keep = np.isin(labels, pieces)
    return loo_centroid(x[keep], labels[keep])


def medians(events, pieces):
    return {p: {k: float(np.median([e[k] for e in events if e['piece'] == p])) for k in KEY} for p in pieces}


def strength_check(events, pieces):
    """Does the time feature move with press strength inside one piece? (Spearman rho)."""
    out = {}
    for p in pieces:
        es = [e for e in events if e['piece'] == p]
        if len(es) < 4:
            out[p] = None; continue
        a = np.argsort(np.argsort([e['peak_counts'] for e in es]))
        rho = {}
        for k in TIME_KEYS:
            b = np.argsort(np.argsort([e[k] for e in es]))
            rho[k] = float(np.corrcoef(a, b)[0, 1])
        out[p] = rho
    return out


def pairwise_ratios(med, pieces):
    """For every pair of pieces, the largest median ratio among the time features."""
    rows = []
    for i, a in enumerate(pieces):
        for b in pieces[i + 1:]:
            best = max(TIME_KEYS, key=lambda k: abs(np.log(med[a][k] / med[b][k])))
            r = med[a][best] / med[b][best]
            rows.append((a, b, best, r if r >= 1 else 1 / r))
    return rows


def plot_waveforms(events, captures, pieces, path):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    grid = np.arange(-0.3, 2.0, 0.025)
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    colors = plt.cm.tab10(np.linspace(0, 1, 10))
    for ci, p in enumerate(pieces):
        raw_curves, norm_curves = [], []
        for e in events:
            if e['piece'] != p or not e['complete']:
                continue
            t, y = captures[e['file']]
            z = y - e['baseline_counts']
            w = np.interp(e['onset_s'] + grid, t, z)
            raw_curves.append(w / 1000.0); norm_curves.append(w / e['peak_counts'])
        for ax, curves, title in ((axes[0], raw_curves, 'delta counts / 1000'),
                                  (axes[1], norm_curves, 'normalised by press peak')):
            if not curves:
                continue
            m = np.mean(curves, 0); s = np.std(curves, 0)
            ax.plot(grid, m, color=colors[ci], lw=2, label=f'{p} (n={len(curves)})')
            ax.fill_between(grid, m - s, m + s, color=colors[ci], alpha=0.15)
            ax.set_title(title); ax.set_xlabel('time from press onset [s]'); ax.grid(alpha=0.3)
    axes[0].legend(); fig.suptitle('Test pieces: mean ± 1 sd waveform aligned at the 10 % onset')
    fig.tight_layout(); fig.savefig(path, dpi=110); plt.close(fig)


def plot_features(events, pieces, path):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(1, 3, figsize=(16, 4.8))
    pairs = [('width_ms', 'recovery_ms'), ('decay_ms', 'release_width_ms'), ('peak_counts', 'width_ms')]
    colors = plt.cm.tab10(np.linspace(0, 1, 10))
    for ax, (kx, ky) in zip(axes, pairs):
        for ci, p in enumerate(pieces):
            es = [e for e in events if e['piece'] == p and e['complete']]
            ax.scatter([e[kx] for e in es], [e[ky] for e in es], color=colors[ci], label=p, s=40)
        ax.set_xlabel(kx); ax.set_ylabel(ky); ax.set_xscale('log'); ax.set_yscale('log'); ax.grid(alpha=0.3, which='both')
    axes[0].legend(); fig.suptitle('Feature scatter per piece (log axes)')
    fig.tight_layout(); fig.savefig(path, dpi=110); plt.close(fig)


def write_summary(out, events, pieces, med, acc_all, acc_pairs, acc_go, strength, ratios, counts):
    lines = ['# 試験片の比較', '', f'入力イベント: 検出 {sum(counts[p]["detected"] for p in pieces)}、'
             f'完全な波形 {sum(counts[p]["complete"] for p in pieces)}。', '',
             '## 片ごとの中央値（完全な波形のみ）', '',
             '| 片 | n | 山 (k) | 立ち上がり ms | 山の幅 ms | 減衰 ms | 谷の幅 ms | 回復 ms | 谷/山 | 面積比 |',
             '|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|']
    for p in pieces:
        m = med[p]
        lines.append(f"| {p} | {counts[p]['complete']} | {m['peak_counts']/1000:.0f} | {m['rise_ms']:.0f} | {m['width_ms']:.0f} | "
                     f"{m['decay_ms']:.0f} | {m['release_width_ms']:.0f} | {m['recovery_ms']:.0f} | {m['trough_peak_ratio']:.2f} | {m['area_ratio']:.2f} |")
    lines += ['', '## 分離（一回抜き最近重心法、振幅を含まない 12 特徴量）', '']
    target = ' / '.join(GO_PIECES)
    if np.isfinite(acc_go):
        decision = 'go' if acc_go >= GO_ACCURACY else 'no-go'
        lines.append(f'- {target} の 3 分類（go 判定）: **{acc_go*100:.1f}%** → **{decision}**'
                     f'（基準: {GO_ACCURACY*100:.0f}% 以上、丸め前の精度で判定）')
    else:
        lines.append(f'- {target} の 3 分類（go 判定）: **未判定**'
                     f'（各片の完全な波形が {MIN_GO_EVENTS} 件以上必要）')
    lines.append('- go 判定用の完全な波形数: ' + '、'.join(
        f'{p}={counts.get(p, {}).get("complete", 0)}' for p in GO_PIECES))
    lines += ['', '全片・ペア別の精度は参考値（go 判定には使わない）。', '',
              f'- 全片: **{acc_all*100:.0f}%**（偶然 {100/len(pieces):.0f}%）']
    for (a, b), v in acc_pairs.items():
        lines.append(f'- {a} vs {b}: {v*100:.0f}%')
    lines += ['', '## go / no-go の判定材料', '',
              '### 時間特徴の中央値の比（片のペアごとに最大のもの。2 倍以上なら「設計で差が作れた」）', '',
              '| 片 A | 片 B | 特徴 | 比 |', '|---|---|---|---:|']
    for a, b, k, r in ratios:
        flag = ' ✔' if r >= 2 else ''
        lines.append(f'| {a} | {b} | {k} | {r:.2f}{flag} |')
    lines += ['', '### 押す強さとの相関（片の中で、山の高さの順位と時間特徴の順位の相関。|ρ| が小さいほど強さに頑健）', '',
              '| 片 | ' + ' | '.join(TIME_KEYS) + ' |', '|---|' + '---:|' * len(TIME_KEYS)]
    for p in pieces:
        r = strength[p]
        lines.append(f'| {p} | ' + (' | '.join(f'{r[k]:+.2f}' for k in TIME_KEYS) if r else 'n<4') + ' |')
    lines += ['', '## 図', '', '- `waveforms.png`: 片ごとの平均波形（左: 生の counts、右: 山で正規化）',
              '- `features.png`: 特徴量の散布図', '- `events.csv`: 全イベントの特徴量']
    (out / 'summary.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('folder')
    parser.add_argument('--out', required=True)
    args = parser.parse_args()
    out = Path(args.out); out.mkdir(parents=True, exist_ok=True)
    events, captures = load_events(args.folder)
    pieces = sorted({e['piece'] for e in events})
    if not pieces:
        sys.exit('no CSV named like ti1-*.csv / tp1-*.csv found')
    complete = [e for e in events if e['complete']]
    counts = {p: dict(detected=sum(e['piece'] == p for e in events),
                      complete=sum(e['piece'] == p for e in complete)) for p in pieces}
    med = medians(complete, pieces)
    x = np.log(np.array([[e[k] for k in SHAPE] for e in complete]))
    labels = [e['piece'] for e in complete]
    acc_all = subset_accuracy(x, labels, pieces)
    acc_go = subset_accuracy(x, labels, GO_PIECES, min_events=MIN_GO_EVENTS)
    acc_pairs = {}
    for i, a in enumerate(pieces):
        for b in pieces[i + 1:]:
            acc_pairs[(a, b)] = subset_accuracy(x, labels, (a, b))
    write_summary(out, complete, pieces, med, acc_all, acc_pairs, acc_go, strength_check(complete, pieces),
                  pairwise_ratios(med, pieces), counts)
    with (out / 'events.csv').open('w', newline='', encoding='utf-8') as f:
        keys = ['piece', 'file', 'event', 'complete', 'exclusion'] + KEY + SHAPE
        w = csv.DictWriter(f, fieldnames=keys, extrasaction='ignore'); w.writeheader()
        for e in events:
            w.writerow({k: e.get(k) for k in keys})
    plot_waveforms(complete, captures, pieces, out / 'waveforms.png')
    plot_features(complete, pieces, out / 'features.png')
    (out / 'counts.json').write_text(json.dumps(counts, indent=2), encoding='utf-8')
    print((out / 'summary.md').read_text(encoding='utf-8'))


if __name__ == '__main__':
    main()
