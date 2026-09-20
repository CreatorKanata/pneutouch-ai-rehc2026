// Exercise production serial lifecycle using WHATWG streams, with no hardware.
import test from 'node:test';
import assert from 'node:assert/strict';
import {SerialConnection} from '../src/shared/serial.mjs';
const settings = {baudRate: 115200, maxLine: 160};
function fixture() {
  const lines = [], errors = [], events = [];
  let controller;
  const port = {
    readable: new ReadableStream({start(c) { controller = c; }}),
    async open(options) { assert.equal(options.parity, 'none'); events.push('open'); },
    async close() { assert.equal(this.readable.locked, false); events.push('close'); },
  };
  const connection = new SerialConnection(settings, {onLine: l => lines.push(l),
    onError: e => errors.push(e), onClose: () => events.push('closed')});
  return {port, connection, controller, lines, errors, events};
}
test('fragmented input, long frame and graceful cancellation release port', async () => {
  const f = fixture(); await f.connection.start(f.port);
  for (const chunk of ['PNEU1,1,', '25,-1\r\n', 'x'.repeat(161) + '\n'])
    f.controller.enqueue(new TextEncoder().encode(chunk));
  await new Promise(resolve => setImmediate(resolve));
  await f.connection.stop();
  assert.deepEqual(f.lines, ['PNEU1,1,25,-1\r\n']);
  assert.equal(f.errors.length, 1);
  assert.deepEqual(f.events, ['open', 'close', 'closed']);
});
test('unplug closes reader; a subsequent connection can start', async () => {
  const f = fixture(); await f.connection.start(f.port);
  f.controller.error(new Error('unplugged')); await f.connection.running;
  assert.equal(f.errors.length, 1); assert.equal(f.connection.port, undefined);
  const next = fixture(); await f.connection.start(next.port); await f.connection.stop();
  assert.equal(next.events.filter(e => e === 'close').length, 1);
});
