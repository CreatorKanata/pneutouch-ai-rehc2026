"""prepare_retrain --replay: features from the host replay of pressure_features.c."""
import contextlib
import io
import json
import os
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
sys.path.insert(0, str(ROOT / 'tests'))
import generate_pressure_model as gpm  # noqa: E402
import prepare_retrain as pr  # noqa: E402
import retrain_replay as rp  # noqa: E402
from test_prepare_retrain import BASE, EVENT_SPACING_MS, GOOD, write_recording  # noqa: E402

FAKE = """#!{python}
import sys
from pathlib import Path
csv = Path(sys.argv[1])
if csv.stem.startswith('back'):
    sys.exit(3)
sys.stdout.write(csv.with_suffix('.replay.txt').read_text())
"""
T0 = 100000


def replay_text(zone, rng):
    """GOOD completes, one INVALID, one TIMEOUT, a trailing START and a garbled line."""
    lines = []
    for i in range(GOOD + 2):
        trig = T0 + 1000 + i * EVENT_SPACING_MS
        lines.append(f'1,{i + 1},{trig},{trig}')
        if i < GOOD:
            feats = [v * rng.lognormal(0, 0.08) for v in BASE[zone]]
            vals = feats + [4e6, rng.uniform(60_000, 90_000), 3e5]
            lines.append(f'2,{i + 1},{trig},{trig + 1250},' + ','.join(f'{v:.8g}' for v in vals))
        else:
            lines.append(f'{3 if i == GOOD else 4},{i + 1},{trig},{trig + 900}')
    lines += [f'1,{GOOD + 3},{T0 + 90000},{T0 + 90000}', 'garbage,1,2']
    return '\n'.join(lines) + '\n'


@unittest.skipIf(os.name == 'nt', 'fake replay uses a shebang script')
class ReplayModeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        base = Path(self.tmp.name)
        self.folder, self.out = base / 'rec', base / 'out'
        self.folder.mkdir()
        rng = np.random.default_rng(11)
        for zone in ('head', 'legs', 'tail'):
            write_recording(self.folder, f'{zone}-1', zone, rng)
            (self.folder / f'{zone}-1.replay.txt').write_text(replay_text(zone, rng))
        (self.folder / 'tail-1.uart.log').unlink()
        self.exe = base / 'fake_replay'
        self.exe.write_text(FAKE.format(python=sys.executable))
        self.exe.chmod(0o755)

    def tearDown(self):
        self.tmp.cleanup()

    def run_tool(self):
        with contextlib.redirect_stdout(io.StringIO()):
            return pr.main([str(self.folder), '--out', str(self.out), '--replay', str(self.exe)])

    def test_counts_predictions_and_source(self):
        ctx = self.run_tool()
        data = json.loads((self.out / pr.TRAINING_NAME).read_text(encoding='utf-8'))
        rec = {r['zone']: r for r in data['records']}
        for zone in ('HEAD', 'LEGS', 'TAIL'):
            self.assertEqual({k: rec[zone][k] for k in ('started', 'completed', 'invalid', 'timeout', 'kept')},
                             dict(started=GOOD + 3, completed=GOOD, invalid=1, timeout=1, kept=GOOD), zone)
        head = [e for e in data['events'] if e['zone'] == 'HEAD']
        self.assertTrue(all(e['firmware_predicted'] == 'BACK' for e in head))
        self.assertTrue(all(60_000 <= e['peak'] <= 90_000 and e['baseline'] == 4e6 for e in head))
        tail = [e for e in data['events'] if e['zone'] == 'TAIL']
        self.assertEqual(len(tail), GOOD)  # no .uart.log is fine in replay mode
        self.assertTrue(all(e['firmware_predicted'] is None for e in tail))
        self.assertEqual(data['source']['kind'], 'replay')
        self.assertEqual(data['source']['exe'], str(self.exe.resolve()))
        self.assertIn(data['source']['sha256'], data['note'])
        summary = (self.out / 'summary.md').read_text(encoding='utf-8')
        self.assertIn('feature source: **replay**', summary)
        self.assertIn(data['source']['sha256'], summary)
        reasons = sorted(e['reason'] for e in ctx['events'] if e['reason'])
        self.assertEqual(reasons, sorted(['invalid', 'timeout'] * 3))
        self.assertIn('PNEU_MODEL_EXAMPLES 36U', gpm.generate(self.out / pr.TRAINING_NAME))

    def test_failing_replay_raises_with_file_name(self):
        write_recording(self.folder, 'back-1', 'back', np.random.default_rng(2))
        with self.assertRaisesRegex(RuntimeError, 'back-1.csv'):
            self.run_tool()


class ReplayParsingTests(unittest.TestCase):
    def test_parse_replay_line(self):
        self.assertEqual(rp.parse_replay_line('3,4,1000,1500'),
                         dict(kind='invalid', event=4, trigger_ms=1000, ms=1500))
        full = rp.parse_replay_line('2,1,10,20,' + ','.join(['1.5'] * 12) + ',4000000,80000,30000')
        self.assertEqual((full['features12'], full['peak']), ([1.5] * 12, 80000.0))
        for bad in ('', 'x,1,2,3', '2,1,10,20', '3,1,10,20,5', '9,1,10,20', '1,2,3'):
            with self.subTest(line=bad):
                self.assertIsNone(rp.parse_replay_line(bad))


if __name__ == '__main__':
    unittest.main()
