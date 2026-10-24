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
import make_log_table  # noqa: E402

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


class FastLogTests(unittest.TestCase):
    def test_header_is_generated(self):
        header = (ROOT / 'src/pneutouch-solist/S_PneuTouch/fast_log_table.h').read_text(encoding='utf-8')
        self.assertEqual(header, make_log_table.header_text())
        table = make_log_table.table()
        self.assertEqual((table.dtype, table.shape, table[0], table[256]), (np.float32, (257,), 0.0, 1.0))

    def test_accuracy_and_exact_points(self):
        x = np.exp(np.linspace(np.log(1e-4), np.log(1e8), 50000)).astype(np.float32)
        got = fs.fast_log(x)
        self.assertEqual(got.dtype, np.float32)
        self.assertLessEqual(float(np.max(np.abs(got.astype(np.float64) - np.log(x).astype(np.float32)))), 4e-6)
        self.assertTrue(np.all(np.diff(got) >= 0))
        powers = np.arange(-40, 61)
        np.testing.assert_array_equal(fs.fast_log(np.ldexp(np.float32(1), powers).astype(np.float32)),
                                      powers.astype(np.float32) * np.float32(0.69314718))
        self.assertEqual(float(fs.fast_log(1.0)), 0.0)
        self.assertAlmostEqual(float(fs.fast_log(np.float32(1.5) * np.float32(2.0 ** -140))),
                               np.log(1.5 * 2.0 ** -140), delta=1e-4)  # denormal

    def test_perfect_separator_ranks_first(self):
        exponents = np.array(FIXTURE['exponents'], dtype=float)
        exponents[:, 3] = [1, 1, 1, 1, 2, 2, 2, 2]
        s = fs.stats(FIXTURE['labels'], (2.0 ** exponents).astype(np.float32))
        chosen = fs.select(s, 0xF, 0)
        self.assertEqual(float(chosen['fisher'][3]), float(np.float32(1e9)))
        self.assertEqual(fs.rank(chosen['fisher'])[0], 3)


class ReferenceTests(unittest.TestCase):
    def test_pool_rows_trough_rule(self):
        events = [dict(label=1, features12=[2.0] * 10 + [0.5, 3.0], peak=1000.0),
                  dict(label=2, features12=[2.0] * 12, peak=1000.0, trough=123.0)]
        rows = fs.pool_rows(events)
        self.assertEqual(rows.dtype, np.float32)
        self.assertEqual(rows[0, 13], 500.0)
        self.assertEqual(rows[1, 13], 123.0)
        np.testing.assert_array_equal(fs.log_pool(events), fs.fast_log(rows))
        np.testing.assert_allclose(fs.log_pool(events), np.log(rows), rtol=0, atol=4e-6)

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
        self.assertIn('selected (4): rise_ms, slope_norm_s, trough_peak_ratio, trough', text)
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

    def test_report_two_classes(self):
        """A KRAG slot (HAT 1 / FACE 2 only) reports like any other: absent classes count 0."""
        events = [dict(e, label=(e['label'] - 1) % 2 + 1) for e in fixture_events()]
        lines = fs.report(events)
        self.assertEqual(lines[0], 'records: 8  per class: [4, 4, 0, 0]')
        self.assertTrue(any(line.startswith('selected (') for line in lines))


if __name__ == '__main__':
    unittest.main()
