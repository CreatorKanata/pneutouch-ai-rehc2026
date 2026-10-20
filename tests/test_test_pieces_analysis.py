"""Regression checks for the internal-piece go/no-go criterion."""
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
import analyze_test_pieces as a


class TestPiecesAnalysisTests(unittest.TestCase):
    def setUp(self):
        # Three overlapping clusters: every binary task passes, but the
        # simultaneous three-class task fails. Values are already log features.
        centers = np.array([[0., 0.], [10., 0.], [5., 8.660254]])
        self.x = np.vstack([
            np.vstack([np.tile(c, (7, 1)),
                       np.tile(c + .65 * (centers[(i + 1) % 3] - c), (3, 1))])
            for i, c in enumerate(centers)
        ])
        self.x = np.tile(self.x, (1, 6))  # twelve shape features
        self.labels = np.repeat(('ti1', 'ti4', 'ti5'), 10)

    def test_binary_passes_do_not_imply_three_class_go(self):
        for pair in [('ti1', 'ti4'), ('ti1', 'ti5'), ('ti4', 'ti5')]:
            self.assertAlmostEqual(a.subset_accuracy(self.x, self.labels, pair), .85)
        self.assertAlmostEqual(a.subset_accuracy(
            self.x, self.labels, a.GO_PIECES, min_events=a.MIN_GO_EVENTS), .70)

    def test_other_pieces_do_not_change_target_score_or_scaling(self):
        # Non-target data overlap target classes and include extreme outliers.
        x = np.vstack([self.x, self.x, np.full((10, 12), 1e9)])
        labels = np.r_[self.labels, np.repeat('ti2', 30), np.repeat('ti3', 10)]
        self.assertAlmostEqual(a.subset_accuracy(
            x, labels, a.GO_PIECES, min_events=a.MIN_GO_EVENTS), .70)

    def test_missing_or_insufficient_target_is_not_a_binary_score(self):
        for n in (0, 1, 6):
            with self.subTest(ti5_count=n):
                keep = np.r_[np.arange(20), np.arange(20, 20 + n)]
                self.assertTrue(np.isnan(a.subset_accuracy(
                    self.x[keep], self.labels[keep], a.GO_PIECES,
                    min_events=a.MIN_GO_EVENTS)))
        keep = np.r_[np.arange(20), np.arange(20, 27)]
        self.assertTrue(np.isfinite(a.subset_accuracy(
            self.x[keep], self.labels[keep], a.GO_PIECES,
            min_events=a.MIN_GO_EVENTS)))

    def test_report_uses_unrounded_target_score(self):
        pieces = list(a.GO_PIECES)
        med = {p: {k: 1. for k in a.KEY} for p in pieces}
        counts = {p: {'detected': 10, 'complete': 10} for p in pieces}
        strength = {p: {k: 0. for k in a.TIME_KEYS} for p in pieces}
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder)
            for score, expected in [(.7996, '**no-go**'), (.8, '**go**'),
                                    (float('nan'), '**未判定**')]:
                with self.subTest(score=score):
                    a.write_summary(out, [], pieces, med, 1., {('ti1', 'ti4'): 1.},
                                    score, strength, [], counts)
                    summary = (out / 'summary.md').read_text(encoding='utf-8')
                    target_line = next(line for line in summary.splitlines()
                                       if 'の 3 分類（go 判定）:' in line)
                    self.assertIn(expected, target_line)


if __name__ == '__main__':
    unittest.main()
