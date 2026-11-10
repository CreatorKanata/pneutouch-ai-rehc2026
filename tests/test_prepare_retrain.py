"""Regression checks for tools/prepare_retrain.py on synthetic recordings."""
import contextlib
import csv
import io
import json
import sys
import tempfile
from pathlib import Path
import unittest

try:
    import numpy as np
except ImportError as exc:
    raise unittest.SkipTest('Offline analysis tests require requirements-analysis.txt') from exc

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import feature_select as fs  # noqa: E402
import prepare_retrain as pr  # noqa: E402
import retrain_report as rr  # noqa: E402

GOOD = 12
# rise, decay, width, area_w, slope, asym, rel_rise, recovery, rel_width, rel_area_w, trough/peak, area
BASE = {
    'tail': [120, 200, 220, 240, 9, 1.6, 110, 130, 120, 140, 1.5, 1.0],
    'back': [180, 300, 320, 340, 7, 1.4, 150, 180, 170, 190, 2.0, 1.1],
    'legs': [150, 250, 270, 290, 8, 1.5, 120, 140, 130, 150, 2.2, 1.0],
    'head': [170, 600, 640, 660, 5, 2.5, 130, 300, 280, 300, 2.3, 1.2],
}
PREDICT = {'tail': 'TAIL', 'back': 'LEGS', 'legs': 'LEGS', 'head': 'BACK'}
SAMPLE_MS = 25
EVENT_SPACING_MS = 5000


def pnef(eid, trig, end, start, values):
    return f'# PNEF1,{eid},{trig},{end},{start},' + ','.join(str(int(round(v * 1000))) for v in values)


def event_lines(eid, trig, end, feats, predicted):
    return [f'# PNEE1,{eid},START,{trig}', f'# PNEC1,{eid},{end + 50},{predicted},103,3e22,3edf,3e86,3e10',
            pnef(eid, trig, end, 0, feats[:6]), 'PNEU1,1,2,3', pnef(eid, trig, end, 6, feats[6:])]


def write_recording(folder, stem, zone, rng, extras=True):
    """GOOD normal events, plus (optionally) a zero feature, a part-0-only event and an INVALID."""
    n = GOOD + 3
    t0 = 100000
    ms = np.arange(t0, t0 + (n + 1) * EVENT_SPACING_MS, SAMPLE_MS)
    raw = 4_000_000 + rng.normal(0, 300, len(ms))
    lines = ['# PNEF1; version=1; units=x1000', '# PNEL1,UNKNOWN,1']
    for i in range(n):
        trig = t0 + 1000 + i * EVENT_SPACING_MS
        end = trig + 1200
        amp = rng.uniform(150_000, 300_000)
        raw += amp * np.exp(-0.5 * ((ms - trig - 300) / 150.0) ** 2)
        feats = [v * rng.lognormal(0, 0.08) for v in BASE[zone]]
        eid = i + 1
        if not extras or i < GOOD:
            lines += event_lines(eid, trig, end, feats, PREDICT[zone])
        elif i == GOOD:
            lines += event_lines(eid, trig, end, [0.0] + feats[1:], PREDICT[zone])
        elif i == GOOD + 1:
            lines += event_lines(eid, trig, end, feats, PREDICT[zone])[:3]
        else:
            lines += [f'# PNEE1,{eid},START,{trig}', f'# PNEE1,{eid},INVALID,{trig + 400}']
    with (folder / f'{stem}.csv').open('w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(['host_utc', 'seq', 'mcu_ms', 'raw', 'label'])
        for k, (m, r) in enumerate(zip(ms, raw)):
            w.writerow(['2026-10-22T00:00:00+00:00', k, int(m), int(r), 0])
    (folder / f'{stem}.uart.log').write_text('\r\n'.join(lines) + '\r\n', encoding='utf-8')


class PrepareRetrainTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        base = Path(self.tmp.name)
        self.folder, self.out = base / 'rec', base / 'out'
        self.folder.mkdir()
        rng = np.random.default_rng(7)
        for zone, stem in [('head', 'HEAD-20261022-1'), ('back', 'back_1'), ('legs', 'legs-1'), ('tail', 'tail-1')]:
            write_recording(self.folder, stem, zone, rng)

    def tearDown(self):
        self.tmp.cleanup()

    def run_tool(self, *extra):
        with contextlib.redirect_stdout(io.StringIO()):
            return pr.main([str(self.folder), '--out', str(self.out), *extra])

    def test_counts_reasons_labels_and_predictions(self):
        ctx = self.run_tool()
        data = json.loads((self.out / pr.TRAINING_NAME).read_text(encoding='utf-8'))
        self.assertEqual(data['feature_order'], pr.FEATURE_ORDER)
        self.assertEqual(data['model'], dict(hidden=32, seed=1, epochs=4))
        expected = {'TAIL': 1, 'BACK': 2, 'LEGS': 3, 'HEAD': 4}
        for zone, label in expected.items():
            es = [e for e in data['events'] if e['zone'] == zone]
            self.assertEqual(len(es), GOOD, zone)
            self.assertTrue(all(e['label'] == label and e['source_set'] == 'dino_85a_live' for e in es))
            self.assertTrue(all(len(e['features12']) == 12 and min(e['features12']) > 0 for e in es))
            self.assertTrue(all(e['peak'] > 100_000 for e in es))
        rec = {r['zone']: r for r in data['records']}
        self.assertEqual({k: rec['HEAD'][k] for k in ('started', 'completed', 'invalid', 'timeout', 'kept')},
                         dict(started=GOOD + 3, completed=GOOD + 2, invalid=1, timeout=0, kept=GOOD))
        self.assertEqual(len(rec['HEAD']['sha256']), 64)
        head = [e for e in data['events'] if e['zone'] == 'HEAD']
        self.assertTrue(all(e['firmware_predicted'] == 'BACK' for e in head))
        with (self.out / 'events.csv').open(encoding='utf-8') as f:
            rows = list(csv.DictReader(f))
        reasons = sorted(r['reason'] for r in rows if r['reason'])
        self.assertEqual(reasons, sorted(['nonpositive_feature', 'incomplete_feature_parts', 'invalid'] * 4))
        self.assertEqual(len(ctx['kept']), 4 * GOOD)
        self.assertTrue((self.out / 'waveforms.png').stat().st_size > 0)

    def test_summary_and_feature_select_accept_output(self):
        self.run_tool('--hidden', '64')
        summary = (self.out / 'summary.md').read_text(encoding='utf-8')
        self.assertIn('**ready**', summary)
        self.assertIn('tools/model_store.py --port PORT load --slot 0 --input', summary)
        self.assertIn('--use', summary)
        # TAIL and LEGS predicted right, BACK and HEAD wrong -> 50 %.
        self.assertIn('accuracy: **50.0%**', summary)
        self.assertIn('decay_ms: HEAD', summary)
        self.assertIn('✔', summary)
        data = json.loads((self.out / pr.TRAINING_NAME).read_text(encoding='utf-8'))
        self.assertEqual(data['model']['hidden'], 64)
        events = data['events']
        self.assertEqual(len(events), 4 * GOOD)
        chosen = fs.select(fs.stats([e['label'] for e in events], fs.pool_rows(events)), 0xF, 0)
        self.assertTrue(2 <= len(chosen['index']) <= 12)  # the chip's selection accepts the output

    def test_skips_unknown_names_and_missing_uart_log(self):
        rng = np.random.default_rng(1)
        write_recording(self.folder, 'ti1-foo', 'tail', rng)
        write_recording(self.folder, 'legs-nolog', 'legs', rng)
        (self.folder / 'legs-nolog.uart.log').unlink()
        ctx = self.run_tool()
        skipped = {s['file']: s['reason'] for s in ctx['skipped']}
        self.assertIn('ti1-foo.csv', skipped)
        self.assertIn('uart.log', skipped['legs-nolog.csv'])
        self.assertEqual(len(ctx['records']), 4)
        summary = (self.out / 'summary.md').read_text(encoding='utf-8')
        self.assertIn('ti1-foo.csv', summary)
        self.assertIn('legs-nolog.csv', summary)

    def test_missing_zone_and_short_zone_are_reported(self):
        (self.folder / 'tail-1.csv').unlink()
        ctx = self.run_tool('--min-events', '13')
        summary = (self.out / 'summary.md').read_text(encoding='utf-8')
        self.assertIn('| TAIL | 0 | missing |', summary)
        self.assertIn('4 zones: **n/a**', summary)
        self.assertIn('TAIL vs BACK: n/a', summary)
        self.assertIn('**not ready**', summary)
        self.assertIn('TAIL (0/13)', summary)
        self.assertEqual(len(ctx['kept']), 3 * GOOD)

    def test_model_inputs_substitute_peak_and_trough(self):
        order = pr.FEATURE_ORDER
        feats = list(BASE['legs'])
        kept = [dict(features12=feats, peak=48_000.0, label=3)]
        x = rr.model_inputs(kept, order)
        ratio = feats[order.index('trough_peak_ratio')]
        self.assertAlmostEqual(x[0, 5], 48.0)
        self.assertAlmostEqual(x[0, 10], 48_000.0 * ratio / 1000)
        others = [i for i in range(12) if i not in (5, 10)]
        self.assertEqual(list(x[0, others]), [feats[i] for i in others])
        self.assertEqual(feats, BASE['legs'])  # input not mutated

    def test_summary_has_model_input_loo_line(self):
        self.run_tool()
        summary = (self.out / 'summary.md').read_text(encoding='utf-8')
        self.assertIn('- 4 zones: **', summary)
        self.assertIn('- 4 zones, model inputs (peak/trough replace inputs 5/10): **', summary)
        self.assertEqual(summary.count('HEAD vs LEGS:'), 2)

    def test_loo_predictions_match_shared_accuracy(self):
        rng = np.random.default_rng(3)
        x = np.vstack([rng.normal(c, 1.0, (8, 12)) for c in (0, 1.5, 3)])
        labels = np.repeat((1, 2, 3), 8)
        preds = rr.loo_predictions(x, labels)
        self.assertAlmostEqual(np.mean(np.array(preds) == labels), rr.atp.loo_centroid(x, labels))


class TelemetryParsingTests(unittest.TestCase):
    def test_garbled_prefixed_and_header_lines(self):
        tele = pr.parse_telemetry('\n'.join([
            '# PNEF1; version=1; units=x1000', 'RX # PNEE1,3,START,1000', '# PNEF1,3,1000,zz,0,1,2,3,4,5,6',
            '# PNEE1,4,TIMEOUT,9000', 'PNEU1,5,6,7']))
        self.assertEqual(tele['started'], [(3, 1000)])
        self.assertEqual(tele['timeout'], [(4, 9000)])
        self.assertEqual(tele['features'], {})

    def test_zone_of(self):
        self.assertEqual(pr.zone_of('Head_x.csv'), 4)
        self.assertEqual(pr.zone_of('tail.csv'), 1)
        self.assertIsNone(pr.zone_of('pressure-head-1.csv'))

    def test_baseline_fallback_and_empty_window(self):
        ms = np.arange(0, 2000, 100)
        raw = np.full(len(ms), 10.0); raw[15] = 50.0
        self.assertEqual(pr.baseline_and_peak(ms, raw, 1400, 1600), (10.0, 40.0))  # [600,1100] has 6
        self.assertEqual(pr.baseline_and_peak(ms, raw, 300, 1600)[0], 10.0)       # fallback
        self.assertIsNone(pr.baseline_and_peak(ms, raw, 5000, 6000))
        self.assertIsNone(pr.baseline_and_peak(ms, raw, 6000, 5000))               # wrapped window

    def test_flat_window_is_nonpositive_peak(self):
        ms = np.arange(0, 3000, 25)
        entry = dict(trigger_ms=1500, end_ms=2000, parts={0: [1.0] * 6, 6: [1.0] * 6}, predicted=None)
        event = pr.build_event('head-1.csv', 4, 1, entry, (ms, np.full(len(ms), 5.0)))
        self.assertEqual(event['reason'], 'nonpositive_peak')


if __name__ == '__main__':
    unittest.main()
