"""Input substitution rule of tools/generate_pressure_model.py (inputs 5/10)."""
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
import generate_pressure_model as gpm  # noqa: E402

SHAPE = [120, 200, 220, 240, 9, 1.6, 110, 130, 120, 140, 1.5, 1.0]


def make_events(explicit_trough=None):
    events = []
    for i in range(8):
        label = i % 4 + 1
        feats = [v * (1 + 0.05 * i + 0.01 * j) for j, v in enumerate(SHAPE)]
        events.append(dict(label=label, features12=feats, peak=40_000.0 + 7_000 * i))
    if explicit_trough is not None:
        events[0] = dict(events[0], trough=explicit_trough)
    return events


def floats_of(header, name):
    match = re.search(rf'{name}\[12\] = \{{([^}}]*)\}};', header)
    return [float(v.rstrip('f')) for v in match.group(1).split(',')]


class GeneratePressureModelTests(unittest.TestCase):
    def generate(self, events):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'training.json'
            source.write_text(json.dumps(dict(model=dict(hidden=32), events=events)), encoding='utf8')
            return gpm.generate(source)

    def test_header_has_specialist_reference_trough(self):
        header = self.generate(make_events())
        self.assertIn('static const float specialist_reference_trough = ', header)
        self.assertIn('input 10 = trough magnitude / 1000 counts', header)

    def test_reference_row_uses_peak_and_ratio_trough(self):
        events = make_events()
        raw = floats_of(self.generate(events), 'model_reference_raw')
        e0 = events[0]
        self.assertAlmostEqual(raw[5], e0['peak'] / 1000, places=4)
        self.assertAlmostEqual(raw[10], e0['peak'] * e0['features12'][10] / 1000, places=4)
        self.assertAlmostEqual(raw[0], e0['features12'][0], places=3)

    def test_explicit_trough_overrides_ratio(self):
        events = make_events(explicit_trough=123_000.0)
        header = self.generate(events)
        raw = floats_of(header, 'model_reference_raw')
        self.assertAlmostEqual(raw[10], 123.0, places=4)
        # Event 0 is TAIL; the specialist reference is the first LEGS event.
        shape = floats_of(header, 'specialist_reference_shape')
        legs = events[2]
        self.assertAlmostEqual(shape[5], legs['peak'] / 1000, places=4)
        self.assertAlmostEqual(shape[10], legs['peak'] * legs['features12'][10] / 1000, places=4)

    def test_model_inputs_does_not_touch_other_columns(self):
        events = make_events()
        before = json.dumps(events)
        x = gpm.model_inputs(events)
        others = [i for i in range(12) if i not in (5, 10)]
        np.testing.assert_allclose(x[:, others], np.array([e['features12'] for e in events])[:, others],
                                   rtol=1e-6)
        self.assertEqual(json.dumps(events), before)  # source not mutated


if __name__ == '__main__':
    unittest.main()
