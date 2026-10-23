"""tools/feature_select.py (float32 reference) against the shared C fixture."""
import contextlib
import io
import json
import re
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

FIXTURE = json.loads((ROOT / 'tests/fixtures/feature_select_fixture.json').read_text(encoding='utf-8'))
HEADER = (ROOT / 'tests/feature_select_fixture.h').read_text(encoding='utf-8')


def close(testcase, got, expected):
    np.testing.assert_allclose(np.asarray(got, dtype=float), expected, rtol=1e-4, atol=1e-5)


def c_array(name):
    body = re.search(rf'{name}\[[^\]]*\](?:\[[^\]]*\])? = \{{(.*?)\}};', HEADER, re.S).group(1)
    return [float(int(v, 16)) if v.startswith('0x') else float(v) for v in re.findall(r'-?0x[0-9a-f]+|-?[0-9.e+-]+', body)]


def fixture_events():
    pool = np.array(FIXTURE['pool'])
    return [dict(label=l, features12=row[:12].tolist(), peak=float(row[12]), trough=float(row[13]))
            for l, row in zip(FIXTURE['labels'], pool)]


class FixtureTests(unittest.TestCase):
    def setUp(self):
        self.pool = np.array(FIXTURE['pool'], dtype=np.float32)
        self.stats = fs.stats(FIXTURE['labels'], self.pool)

    def check(self, key, mask):
        expected = FIXTURE[key]
        chosen = fs.select(self.stats, mask, 0)
        for field in ('fisher', 'mean', 'scale'):
            close(self, chosen[field], expected[field])
        self.assertEqual(fs.rank(chosen['fisher']), expected['ranking'])
        self.assertEqual(chosen['index'], expected['auto'])
        self.assertEqual(fs.select(self.stats, mask, 3)['index'], expected['k3'])
        words = fs.words(chosen, self.pool[expected['words_record']])
        self.assertEqual(words.tolist(), expected['words'])

    def test_all_classes(self):
        self.check('primary', 0xF)
        chosen = fs.select(self.stats, 0xF, 0)
        self.assertEqual(chosen['fisher'][2], chosen['fisher'][5])  # exact tie, lower index first
        self.assertEqual(chosen['fisher'][1], 0)                    # constant feature
        self.assertNotIn(1, fs.select(self.stats, 0xF, 12)['index'])

    def test_class_mask(self):
        self.check('specialist', 0xC)

    def test_train_order(self):
        self.assertEqual(fs.train_order(8), FIXTURE['train']['order'])
        for n in list(range(1, 130)) + [210, 999, 1000]:
            for e in range(fs.TRAIN_EPOCHS):
                self.assertEqual(sorted(fs.train_index(n, e, i) for i in range(n)), list(range(n)))
        self.assertEqual([fs.train_index(105, e, 1) for e in range(4)], [11, 50, 91, 25])

    def test_header_matches_json(self):
        self.assertEqual(c_array('fix_labels'), FIXTURE['labels'])
        self.assertEqual(c_array('fix_exponents'), sum(FIXTURE['exponents'], []))
        np.testing.assert_array_equal(2.0 ** np.array(FIXTURE['exponents']), FIXTURE['pool'])
        for key in ('primary', 'specialist'):
            for field in ('fisher', 'mean', 'scale'):
                np.testing.assert_allclose(c_array(f'fix_{key}_{field}'), FIXTURE[key][field], rtol=1e-15, atol=1e-300)
            for field in ('ranking', 'auto', 'k3', 'words'):
                self.assertEqual(c_array(f'fix_{key}_{field}'), FIXTURE[key][field])
            self.assertIn(f'#define FIX_{key.upper()}_WORDS_RECORD {FIXTURE[key]["words_record"]}U', HEADER)
        self.assertEqual(c_array('fix_train_order'), FIXTURE['train']['order'])


class ReferenceTests(unittest.TestCase):
    def test_pool_rows_trough_rule(self):
        events = [dict(label=1, features12=[2.0] * 10 + [0.5, 3.0], peak=1000.0),
                  dict(label=2, features12=[2.0] * 12, peak=1000.0, trough=123.0)]
        rows = fs.pool_rows(events)
        self.assertEqual(rows.dtype, np.float32)
        self.assertEqual(rows[0, 13], 500.0)
        self.assertEqual(rows[1, 13], 123.0)
        np.testing.assert_array_equal(fs.log_pool(events), np.log(rows))

    def test_rejects_like_the_chip(self):
        pool = np.ones((4, 14), dtype=np.float32)
        for bad in (np.nan, np.inf, 0.0, -1.0):
            broken = pool.copy(); broken[2, 3] = bad
            with self.assertRaises(ValueError):
                fs.stats([1, 1, 2, 2], broken)
        with self.assertRaises(ValueError):
            fs.stats([1, 1, 2, 5], pool)
        s = fs.stats([1, 1, 2, 2], np.array(FIXTURE['pool'][:4], dtype=np.float32))
        self.assertIsNone(fs.select(s, 0x1, 0))   # one class
        self.assertIsNone(fs.select(s, 0xF, 13))  # k out of range
        same = fs.stats([1, 1, 2, 2], np.array([FIXTURE['pool'][0], FIXTURE['pool'][1]] * 2, dtype=np.float32))
        self.assertIsNone(fs.select(same, 0xF, 0))  # every Fisher score 0
        chosen = fs.select(fs.stats(FIXTURE['labels'], np.array(FIXTURE['pool'], dtype=np.float32)), 0xF, 0)
        row = np.array(FIXTURE['pool'][0], dtype=np.float32)
        row[1] = np.nan  # not selected: ignored
        fs.words(chosen, row)
        row[4] = 0
        with self.assertRaises(ValueError):
            fs.words(chosen, row)

    def test_report_and_cli(self):
        lines = fs.report(fixture_events())
        text = '\n'.join(lines)
        self.assertIn('selected (5): rise_ms, slope_norm_s, trough_peak_ratio, trough, peak', text)
        self.assertIn('LOO nearest centroid, all 14 log features:', text)
        self.assertNotIn('HEAD/LEGS', text)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'training.json'
            path.write_text(json.dumps(dict(events=fixture_events())), encoding='utf-8')
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                fs.main([str(path), '--k', '3'])
            self.assertIn('selected (3): rise_ms, slope_norm_s, trough_peak_ratio', out.getvalue())
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                fs.main([str(path), '--k', '13'])
        self.assertIn('selection failed', fs.report(fixture_events()[:2] * 2)[0])


if __name__ == '__main__':
    unittest.main()
