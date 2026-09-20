// Ensure missing samples, counter wrap, reset and unlabeled records survive export.
import test from 'node:test';
import assert from 'node:assert/strict';
import {Measurement} from '../src/shared/measurement.mjs';
const settings = {displaySamples: 3, recordingSamples: 2};
test('recording bound keeps raw values and distinguishes blank from zero', () => {
  const m = new Measurement(settings); m.start();
  m.add({seq: 0, ms: 500, raw: -5}, '', 't1');
  m.add({seq: 1, ms: 525, raw: 8}, '0', 't2');
  m.add({seq: 2, ms: 550, raw: 10}, '7', 't3');
  assert.equal(m.recording, false); assert.equal(m.rows.length, 2);
  assert.deepEqual(m.rows.map(r => r.label), ['', '0']);
  assert.equal(m.rate, 40); assert.equal(m.dirty, true);
});
test('gaps break trace; clock and sequence wrap remain continuous', () => {
  const m = new Measurement(settings);
  m.add({seq: 0xffffffff, ms: 0xfffffff0, raw: 1}, '', '');
  assert.equal(m.add({seq: 0, ms: 9, raw: 2}, '', '').gap, false);
  assert.equal(m.rate, 40);
  assert.equal(m.add({seq: 2, ms: 59, raw: 4}, '', '').gap, true);
  assert.equal(m.points.length, 3); assert.equal(m.gaps, 1);
  assert.equal(m.add({seq: 0, ms: 0, raw: 3}, '', '').reset, true);
  assert.equal(m.points.length, 1); assert.equal(m.resets, 1);
});
