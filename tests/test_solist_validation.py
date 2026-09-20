import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
try:
    import numpy as np
    from validate_solist_ai import encode, decode, partitions, transform
except ImportError:
    np = None


@unittest.skipIf(np is None, 'Install requirements-analysis.txt and pyserial')
class ValidationTests(unittest.TestCase):
    def test_wire_bfloat16_known_values(self):
        self.assertEqual(encode([0, 1, -1, .5]), '00003f80bf803f00')
        np.testing.assert_array_equal(decode(['0000','3f80','bf80','3f00']), [0, 1, -1, .5])
        with self.assertRaises(ValueError):
            encode([float('nan')])
        with self.assertRaises(ValueError):
            decode(['7f80'])

    def test_original_zone_splits_keep_four_feet_separate(self):
        zones = np.repeat(np.arange(1, 8), [5, 5, 6, 6, 6, 8, 7])
        for _, train, test in partitions(zones):
            self.assertEqual(len(set(train)&set(test)), 0)
            for z in range(1, 8):
                self.assertEqual(int(np.sum(zones[train] == z)), 3)

    def test_held_out_values_cannot_change_training_scale(self):
        x = np.array([[1, 2], [3, 4], [10, 20]], dtype=float)
        a, pa = transform(x, [0, 1], 'features12')
        x[2] *= 100000
        b, pb = transform(x, [0, 1], 'features12')
        self.assertEqual(pa, pb)
        np.testing.assert_array_equal(a[:2], b[:2])


if __name__ == '__main__':
    unittest.main()
