"""tools/model_store.py against a scripted fake of the PST1 firmware (no hardware)."""
import binascii
import contextlib
import io
import json
import sys
import tempfile
from pathlib import Path
import unittest

try:
    import numpy as np
    import serial  # noqa: F401  (validate_solist_ai imports pyserial)
except ImportError as exc:
    raise unittest.SkipTest('Offline tool tests require requirements-analysis.txt') from exc

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import feature_select as fs  # noqa: E402
import model_store as ms  # noqa: E402
from validate_solist_ai import Device  # noqa: E402

FIXTURE = json.loads((ROOT / 'tests/fixtures/feature_select_fixture.json').read_text(encoding='utf-8'))


def fixture_training():
    events = [dict(label=l, zone=ms.ZONE_NAMES[l], features12=row[:12], peak=row[12], trough=row[13])
              for l, row in zip(FIXTURE['labels'], FIXTURE['pool'])]
    return dict(feature_order=ms.FEATURE_ORDER, events=events)


# ok, err, count, best_k, correct k=4..8, recall, counts, confusion (row-major), advice, us
TUNE_OK = ('1,0,176,4,169:162:146:141:137,90:100:100:78,37:33:69:37,'
           '33:2:2:0:0:33:0:0:0:0:69:0:0:0:8:29,4,61000000')
TUNE_FAIL = '0,2,0,0,0:0:0:0:0,255:255:255:255,0:0:0:0,0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0,0,0'


class FakeBoard:
    """Byte-level stand-in for the CN9 UART: parses lines, checks CRC, answers like store_protocol.c."""

    def __init__(self):
        self.rx, self.out = b'', []
        self.mode, self.active = 0, 0
        self.slots = [dict(state=0, k=0, name='', records=[], tuned_k=0, recall='0:0:0:0') for _ in range(4)]
        self.sent, self.override, self.corrupt_get = [], {}, False
        self.old_slot_reply = False  # firmware before TUNE: SLOT ends with the name
        self.tune_reply = TUNE_OK

    def write(self, data):
        self.rx += data
        while b'\n' in self.rx:
            line, self.rx = self.rx.split(b'\n', 1)
            if line:
                self.handle(line.decode('ascii'))

    def reset_input_buffer(self):
        self.out.clear()

    def readline(self):
        return (self.out.pop(0) + '\r\n').encode('ascii') if self.out else b''

    def close(self):
        pass

    def reply(self, *fields):
        self.out.append('# ' + ','.join(map(str, fields)))

    def handle(self, line):
        body, crc = line.rsplit('*', 1)
        assert int(crc, 16) == binascii.crc_hqx(body.encode('ascii'), 0xffff), line
        f = body.split(',')
        self.sent.append(f[:2])
        if f[1] in self.override:
            return self.out.append(self.override[f[1]])
        if f[0] == 'PAI1' and f[1] == 'MODE':
            self.mode = int(f[2])
            return self.reply('PAI1', 'MODE', f[2])
        error = self.store(f)
        if error:
            self.reply('PST1', 'ERROR', error)

    def store(self, f):
        op, token = f[1], f[2]
        if op not in ('INFO', 'SLOT', 'SELFTEST') and not self.mode:
            return 'MODE_REQUIRED'
        if op == 'INFO':
            counts = [s['state'] and len(s['records']) for s in self.slots]
            return self.reply('PST1', 'INFO', token, self.active, *counts, 'fake-fw')
        if op == 'SELFTEST':
            return self.reply('PST1', 'SELFTEST', token, 0)
        if op == 'TUNE':
            fields = self.tune_reply.split(',')
            if fields[0] == '1':
                s = self.slots[self.active]
                s['tuned_k'], s['recall'] = int(fields[3]), fields[5]
            return self.reply('PST1', 'TUNE', token, *fields)
        if op == 'TRAIN':
            s = self.slots[self.active]
            return self.reply('PST1', 'TRAIN', token, 1, 0, len(s['records']), 3, '0:4:10', 1500000)
        slot = int(f[3])
        if slot > 3:
            return 'SLOT'
        s = self.slots[slot]
        if op == 'SLOT':
            extra = () if self.old_slot_reply else (s['tuned_k'], s['recall'])
            return self.reply('PST1', 'SLOT', token, slot, s['state'], len(s['records']), 14, s['k'], s['name'], *extra)
        if op == 'OPEN':
            self.slots[slot] = dict(state=1, k=int(f[4]), name=f[5], records=[], tuned_k=0, recall='0:0:0:0')
            return self.reply('PST1', 'OPEN', token, slot)
        if op == 'PUT':
            if s['state'] != 1:
                return 'STATE'
            s['records'].append((int(f[4]), f[5]))
            return self.reply('PST1', 'PUT', token, slot, len(s['records']))
        if op == 'COMMIT':
            s['state'] = 2
            return self.reply('PST1', 'COMMIT', token, slot, len(s['records']))
        if op == 'GET':
            label, hex_ = s['records'][int(f[4])]
            if self.corrupt_get:
                hex_ = hex_[:-1] + ('0' if hex_[-1] != '0' else '1')
            return self.reply('PST1', 'GET', token, slot, f[4], label, hex_)
        if op in ('USE', 'ERASE'):
            if op == 'USE':
                self.active = slot
            else:
                self.slots[slot] = dict(state=0, k=0, name='', records=[], tuned_k=0, recall='0:0:0:0')
            return self.reply('PST1', op, token, slot)
        return 'COMMAND'


def make_store(board=None):
    board = board or FakeBoard()
    return ms.Store(Device('FAKE', io.StringIO(), connection=board, sleep=lambda s: None)), board


class EncodingTests(unittest.TestCase):
    def test_round_trip_is_bit_exact(self):
        pool = np.array(FIXTURE['pool'][0], dtype=np.float32) * np.float32(1.1)
        text = ms.encode_pool(pool)
        self.assertEqual(len(text), 112)
        self.assertEqual(text[:8], f'{int(pool[:1].view(np.uint32)[0]):08x}')
        np.testing.assert_array_equal(ms.decode_pool(text).view(np.uint32), pool.view(np.uint32))
        self.assertEqual(ms.encode_pool([1.0] * 14)[:8], '3f800000')  # high nibble first, like PAI1

    def test_rejects_invalid(self):
        for bad in ([1.0] * 13, [1.0] * 13 + [0.0], [1.0] * 13 + [float('nan')], [-1.0] * 14):
            with self.assertRaises(ValueError):
                ms.encode_pool(bad)
        with self.assertRaises(ValueError):
            ms.decode_pool('00')
        for name in ('ninechars', 'a,b', 'x*'):
            with self.assertRaises(ValueError):
                ms.check_name(name)
        with self.assertRaises(ValueError):
            ms.check_slot(4)


class SequencingTests(unittest.TestCase):
    def test_load_sequence_verify_and_train(self):
        store, board = make_store()
        result = ms.load(store, 2, fixture_training(), k=0, name='dino', use=True)
        ops = [op for _, op in board.sent]
        self.assertEqual(ops[:2], ['MODE', 'OPEN'])
        self.assertEqual(ops[2:10], ['PUT'] * 8)
        self.assertEqual(ops[10:], ['COMMIT', 'GET', 'GET', 'USE', 'TRAIN', 'MODE'])
        self.assertEqual(board.mode, 0)
        self.assertEqual(board.active, 2)
        self.assertEqual(board.slots[2]['name'], 'dino')
        self.assertEqual(result, dict(ready=True, error=0, count=8, k=3, selected=[0, 4, 10], training_us=1500000))
        self.assertIn('rise_ms, slope_norm_s, trough_peak_ratio', ms.describe_training(result))

    def test_dump_round_trip(self):
        store, board = make_store()
        training = fixture_training()
        ms.load(store, 1, training, k=5, name='x')
        data = ms.dump(store, 1)
        self.assertEqual(data['source'], dict(kind='fram', slot=1, name='x', k=5, state='committed',
                                              tuned_k=0, recall=[0, 0, 0, 0]))
        self.assertEqual(data['feature_order'], ms.FEATURE_ORDER)
        np.testing.assert_array_equal(fs.pool_rows(data['events']).view(np.uint32),
                                      fs.pool_rows(training['events']).view(np.uint32))
        self.assertEqual([e['label'] for e in data['events']], FIXTURE['labels'])
        self.assertEqual(data['events'][6]['zone'], 'HEAD')
        self.assertEqual(fs.select(fs.stats([e['label'] for e in data['events']], fs.pool_rows(data['events'])),
                                   0xF, 0)['index'], FIXTURE['primary']['auto'])  # feature_select accepts it
        self.assertEqual(board.mode, 0)
        self.assertEqual(ms.dump(store, 3)['events'], [])

    def test_slot_with_and_without_tuning_fields(self):
        store, board = make_store()
        ms.load(store, 0, fixture_training(), use=True)
        board.slots[0].update(tuned_k=5, recall='100:50:100:255')
        info = store.slot(0)
        self.assertEqual((info['tuned_k'], info['recall'], info['count']), (5, [100, 50, 100, 255], 8))
        board.old_slot_reply = True
        info = store.slot(0)
        self.assertEqual((info['tuned_k'], info['recall'], info['count']), (0, [0, 0, 0, 0], 8))
        board.old_slot_reply = False
        source = ms.dump(store, 0)['source']
        self.assertEqual((source['tuned_k'], source['recall']), (5, [100, 50, 100, 255]))

    def test_tune_parses_the_reply(self):
        store, board = make_store()
        with store.paused():
            result = store.tune()
        self.assertEqual(result['correct'], {4: 169, 5: 162, 6: 146, 7: 141, 8: 137})
        self.assertEqual((result['ok'], result['error'], result['count'], result['best_k']), (True, 0, 176, 4))
        self.assertEqual(result['recall'], [90, 100, 100, 78])
        self.assertEqual(result['counts'], [37, 33, 69, 37])
        self.assertEqual(result['confusion'], [[33, 2, 2, 0], [0, 33, 0, 0], [0, 0, 69, 0], [0, 0, 8, 29]])
        self.assertEqual((result['advice'], result['elapsed_us']), (4, 61000000))
        lines = ms.describe_tune(result)
        self.assertEqual(lines[0], 'tune: 176 records, 4-fold, best k = 4 (61.0 s)')
        self.assertEqual(lines[1], '  k=4 96%  k=5 92%  k=6 83%  k=7 80%  k=8 78%')
        self.assertEqual(lines[2], '  TAIL 90% (37)  BACK 100% (33)  LEGS 100% (69)  HEAD 78% (37)')
        self.assertEqual(lines[3], '  提案: 頭（HEAD）のデータを追加してください（足（LEGS）と混同 8/37）')
        none = dict(result, advice=0, recall=[100, 255, 95, 90])
        self.assertEqual(ms.describe_tune(none)[2:], ['  TAIL 100% (37)  BACK - (33)  LEGS 95% (69)  HEAD 90% (37)',
                                                      '  提案: なし（すべての部位が 90% 以上）'])
        with self.assertRaisesRegex(RuntimeError, 'NO_SLOT'):
            ms.describe_tune(dict(result, ok=False, error=2))

    def test_readback_mismatch_restores_live_mode(self):
        store, board = make_store()
        board.corrupt_get = True
        with self.assertRaisesRegex(RuntimeError, 'read-back mismatch at record 0'):
            ms.load(store, 0, fixture_training())
        self.assertEqual(board.mode, 0)
        self.assertNotIn(['PST1', 'TRAIN'], board.sent)

    def test_firmware_error_is_raised_and_mode_restored(self):
        store, board = make_store()
        board.override['COMMIT'] = '# PST1,ERROR,STORE'
        with self.assertRaisesRegex(RuntimeError, 'PST1,ERROR,STORE'):
            ms.load(store, 0, fixture_training())
        self.assertEqual(board.mode, 0)
        board.override = {'INFO': '# PAI1,ERROR,CRC'}
        with self.assertRaisesRegex(RuntimeError, 'PAI1,ERROR,CRC'):
            store.info()

    def test_silent_board_times_out(self):
        store, board = make_store()
        board.override['INFO'] = ''
        board.out.clear()
        with self.assertRaises(TimeoutError):
            store._cmd('INFO', timeout=0.05)

    def test_invalid_training_is_refused_before_any_write(self):
        store, board = make_store()
        training = fixture_training()
        for events in (training['events'][:3], training['events'] * 126,
                       [dict(training['events'][0], label=5)] + training['events'][1:]):
            with self.assertRaises(ValueError):
                ms.load(store, 0, dict(events=events))
        with self.assertRaises(ValueError):
            ms.load(store, 0, training, k=13)
        self.assertEqual(board.sent, [])


class CliTests(unittest.TestCase):
    def run_cli(self, board, *argv):
        out = io.StringIO()
        factory = lambda port, log: Device(port, log, connection=board, sleep=lambda s: None)  # noqa: E731
        with contextlib.redirect_stdout(out):
            ms.main(['--port', 'FAKE', *argv], device_factory=factory)
        return out.getvalue()

    def test_commands(self):
        board = FakeBoard()
        with tempfile.TemporaryDirectory() as directory:
            source, dumped = Path(directory) / 'training.json', Path(directory) / 'dump.json'
            source.write_text(json.dumps(fixture_training()), encoding='utf-8')
            self.assertIn('8 records loaded and verified', self.run_cli(board, 'load', '--slot', '0', '--input',
                                                                         str(source), '--use'))
            info = self.run_cli(board, 'info')
            self.assertIn('slot 0: 8 records', info)
            self.assertIn('firmware: fake-fw', info)
            self.assertIn('8 records ->', self.run_cli(board, 'dump', '--slot', '0', '--out', str(dumped)))
            self.assertEqual(len(json.loads(dumped.read_text())['events']), 8)
            self.assertIn('active slot: 3', self.run_cli(board, 'use', '--slot', '3'))
            self.assertIn('slot 0 erased', self.run_cli(board, 'erase', '--slot', '0'))
            self.assertIn('READY', self.run_cli(board, 'train'))
            board.sent.clear()
            self.assertIn('PASS', self.run_cli(board, 'selftest'))
            # Live mode drops whole packets: even read-only commands pause acquisition first.
            self.assertEqual([op for _, op in board.sent], ['MODE', 'SELFTEST', 'MODE'])
            board.sent.clear()
            self.run_cli(board, 'info')
            self.assertEqual([op for _, op in board.sent], ['MODE', 'INFO', 'MODE'])
            board.override['SELFTEST'] = '# PST1,ERROR,MODE_REQUIRED'
            with self.assertRaises(SystemExit):
                self.run_cli(board, 'selftest')
        self.assertEqual(board.mode, 0)

    def test_tune_then_train(self):
        board = FakeBoard()
        store, _ = make_store(board)
        ms.load(store, 0, fixture_training(), use=True)
        board.sent.clear()
        out = self.run_cli(board, 'tune')
        self.assertEqual([op for _, op in board.sent], ['MODE', 'TUNE', 'TRAIN', 'MODE'])
        self.assertIn('best k = 4 (61.0 s)', out)
        self.assertIn('提案: 頭（HEAD）のデータを追加してください（足（LEGS）と混同 8/37）', out)
        self.assertIn('READY: 8 records, 3 features selected', out)
        self.assertEqual(board.slots[0]['tuned_k'], 4)
        board.tune_reply = TUNE_FAIL
        board.sent.clear()
        with self.assertRaisesRegex(SystemExit, 'model_store: TUNE failed: NO_SLOT'):
            self.run_cli(board, 'tune')
        self.assertEqual([op for _, op in board.sent], ['MODE', 'TUNE', 'MODE'])
        self.assertEqual(board.mode, 0)


if __name__ == '__main__':
    unittest.main()
