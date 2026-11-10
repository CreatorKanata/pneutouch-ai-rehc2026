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
test('write sends one UTF-8 line with CRLF, releases the writer, and fails when closed', async () => {
  const f = fixture(), chunks = [];
  f.port.writable = new WritableStream({write(chunk) { chunks.push(new TextDecoder().decode(chunk)); }});
  await assert.rejects(f.connection.write('PAI1,MODE,1*1f24'), /接続/);
  await f.connection.start(f.port);
  await f.connection.write('PAI1,MODE,1*1f24'); await f.connection.write('PAI1,MODE,0*0f05');
  assert.deepEqual(chunks, ['PAI1,MODE,1*1f24\r\n', 'PAI1,MODE,0*0f05\r\n']);
  assert.equal(f.port.writable.locked, false);
  await f.connection.stop();
  await assert.rejects(f.connection.write('x'), /接続/);
});
test('paced write sends one byte per call with a pause between bytes, using one writer', async () => {
  const f = fixture(), chunks = [], sleeps = [];
  f.connection.sleep = async ms => { sleeps.push(ms); };
  f.port.writable = new WritableStream({write(chunk) { chunks.push([...chunk]); }});
  await f.connection.start(f.port);
  await f.connection.write('AB', {paceMs: 15});
  assert.deepEqual(chunks, [[65], [66], [13], [10]]);
  assert.deepEqual(sleeps, [15, 15, 15]);
  assert.equal(f.port.writable.locked, false);
  await f.connection.write('C', {paceMs: 0}); assert.deepEqual(chunks.at(-1), [67, 13, 10]);
  await f.connection.stop();
});
