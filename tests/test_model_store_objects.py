"""tools/model_store.py with object profiles (KRAG: HAT/FACE) against the fake PST1 board."""
import json
import tempfile
from pathlib import Path
import unittest

from test_model_store_tool import FINALISTS, FakeBoard, fixture_training, make_store, ms, run_cli


def krag_training():
    """The fixture with TAIL/LEGS as HAT (1) and BACK/HEAD as FACE (2)."""
    events = [dict(e, label=(e['label'] - 1) % 2 + 1) for e in fixture_training()['events']]
    return dict(feature_order=ms.FEATURE_ORDER, events=[dict(e, zone=ms.OBJECTS['KRAG'][e['label'] - 1])
                                                       for e in events])


# KRAG slot: HAT/FACE only; LEGS/HEAD recall 255 and empty confusion rows/columns.
TUNE_KRAG = (f'1,0,24,3300,4,20,3300,20,{FINALISTS},99,4,75:92:255:255,12:12:0:0,'
             '9:3:0:0:1:11:0:0:0:0:0:0:0:0:0:0,1,40000000')


class ObjectTests(unittest.TestCase):
    """KRAG slots (HAT/FACE): the tag in the slot name selects the object."""

    def test_object_of_name(self):
        self.assertEqual([ms.object_of_name(n) for n in ('KRAGBTN', 'KRAG', 'DINO1023', '1023-176', '', 'krag')],
                         ['KRAG', 'KRAG', 'DINO', 'DINO', 'DINO', 'DINO'])
        self.assertEqual(ms.zone_names('KRAG'), {1: 'HAT', 2: 'FACE'})
        self.assertEqual(ms.slot_name('KRAG', '1024'), 'KRAG1024')
        for obj, suffix in (('KRAG', '12345'), ('ROBO', 'x'), ('KRAG', 'a,b')):
            with self.assertRaises(ValueError):
                ms.slot_name(obj, suffix)

    def test_krag_load_dump(self):
        store, board = make_store()
        result = ms.load(store, 1, krag_training(), name='1024', use=True, obj='KRAG')
        self.assertEqual((board.slots[1]['name'], board.active, result['ready']), ('KRAG1024', 1, True))
        self.assertEqual({label for label, _ in board.slots[1]['records']}, {1, 2})
        data = ms.dump(store, 1)
        self.assertEqual((data['source']['object'], data['source']['name']), ('KRAG', 'KRAG1024'))
        self.assertEqual([e['zone'] for e in data['events']], ['HAT', 'HAT', 'FACE', 'FACE'] * 2)
        self.assertIn('object KRAG (HAT/FACE)', ms.describe_training(result, 'KRAG'))

    def test_krag_refuses_dino_labels_before_any_write(self):
        store, board = make_store()
        with self.assertRaisesRegex(ValueError, r'labels must be 1\.\.2 \(KRAG\)'):
            ms.load(store, 0, fixture_training(), obj='KRAG')
        with self.assertRaisesRegex(ValueError, 'suffix'):
            ms.load(store, 0, krag_training(), name='12345', obj='KRAG')
        self.assertEqual(board.sent, [])

    def test_krag_info_and_tune(self):
        board = FakeBoard()
        board.slots[0] = FakeBoard.empty_slot(state=2, name='KRAG1024', records=[(1, 'x'), (2, 'x')] * 12,
                                              tuned_k=4, recall='75:92:255:255', mask='3300')
        board.slots[1] = FakeBoard.empty_slot(state=2, name='1023-176', records=[(1, 'x')] * 8)
        board.tune_reply = TUNE_KRAG
        info = run_cli(board, 'info')
        self.assertIn('slot 0: 24 records  KRAG (HAT/FACE)  k=4 [release_width_ms, release_area_width_ms, peak, '
                      'trough]  recall HAT 75% FACE 92%  提案: 帽子（HAT）', info)
        self.assertIn('slot 1: 8 records  DINO (TAIL/BACK/LEGS/HEAD)  (未調整)', info)
        out = run_cli(board, 'tune').splitlines()
        self.assertIn('  HAT 75% (12)  FACE 92% (12)', out)
        self.assertEqual(out[out.index('  混同行列（行 = 正解、列 = 判定）') + 1:][:3],
                         ['         HAT FACE', '  HAT      9    3', '  FACE     1   11'])
        self.assertIn('  提案: 帽子（HAT）のデータを追加してください。HAT → FACE 3 件、FACE → HAT 1 件（件数 HAT 12 対 FACE 12）',
                      out)
        self.assertTrue(out[-1].endswith('object KRAG (HAT/FACE)'))
        board.sent.clear()
        self.assertIn('object KRAG (HAT/FACE)', run_cli(board, 'train'))
        self.assertEqual([op for _, op in board.sent], ['MODE', 'INFO', 'SLOT', 'TRAIN', 'MODE'])

    def test_cli_load_object(self):
        board = FakeBoard()
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'krag.json'
            source.write_text(json.dumps(krag_training()), encoding='utf-8')
            out = run_cli(board, 'load', '--slot', '2', '--input', str(source), '--object', 'KRAG',
                                   '--name', 'A1')
            self.assertIn('object KRAG (HAT/FACE)', out)
            self.assertEqual(board.slots[2]['name'], 'KRAGA1')
            run_cli(board, 'load', '--slot', '3', '--input', str(source))  # default DINO, MMDD
            self.assertRegex(board.slots[3]['name'], r'^DINO\d{4}$')
            source.write_text(json.dumps(fixture_training()), encoding='utf-8')
            with self.assertRaisesRegex(SystemExit, 'KRAG'):
                run_cli(board, 'load', '--slot', '1', '--input', str(source), '--object', 'KRAG')


if __name__ == '__main__':
    unittest.main()
