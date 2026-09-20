// Browser protocol tests run with Node, without a serial port or UI automation.
import test from 'node:test';
import assert from 'node:assert/strict';
import {parseLine, LineBuffer, csv} from '../src/shared/protocol.mjs';

test('signed data and counter boundaries', () => {
  assert.deepEqual(parseLine('PNEU1,4294967295,25,-8388608\r\n'),
    {seq: 4294967295, ms: 25, raw: -8388608});
  for (const line of ['PNEU1,1,2,8388608\n', 'PNEU1,1,2,3', 'PNEU2,1,2,3\n',
    'PNEU1,4294967296,2,3\n', '# ERROR,HX710B_TIMEOUT\n']) assert.equal(parseLine(line), null);
});
test('fragmented and overlong frames recover at newline', () => {
  const buffer = new LineBuffer(160);
  assert.deepEqual([...buffer.feed('PNEU1,1,')], []);
  assert.deepEqual([...buffer.feed('25,-1\r\n')], ['PNEU1,1,25,-1\r\n']);
  assert.deepEqual([...buffer.feed('x'.repeat(161))], []);
  assert.deepEqual([...buffer.feed('junk\nPNEU1,2,50,0\n')], ['PNEU1,2,50,0\n']);
});
test('CSV keeps label, negative value and timestamp', () => {
  const data = csv([{utc: '2026-09-20T00:00:00Z', seq: 2, ms: 50, raw: -1, label: '7'}]);
  assert.equal(data, 'host_utc,seq,mcu_ms,raw,label\r\n"2026-09-20T00:00:00Z","2","50","-1","7"\r\n');
});
