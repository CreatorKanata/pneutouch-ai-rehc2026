// Android app's Web Serial shim (android/app/src/main/assets/web-serial-shim.js) with a fake native bridge.
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {SerialConnection} from '../src/shared/serial.mjs';
const SHIM = readFileSync(new URL('../android/app/src/main/assets/web-serial-shim.js', import.meta.url), 'utf8');
const tick = () => new Promise(resolve => setImmediate(resolve));
const encode = text => Buffer.from(text).toString('base64');
const decode = base64 => [...Buffer.from(base64, 'base64')];

// The bridge records calls; the test answers them through window.__pneuTouchSerial like the native side.
function fixture({navigator = {}, bridge = true} = {}) {
  const calls = [];
  const window = {navigator, ReadableStream, WritableStream, atob, btoa, setTimeout};
  if (bridge) window.PneuTouchSerial = {
    requestPort: id => calls.push(['requestPort', id]),
    open: (id, options) => calls.push(['open', id, JSON.parse(options)]),
    write: (id, base64) => calls.push(['write', id, decode(base64)]),
    close: id => calls.push(['close', id]),
  };
  window.window = window;
  vm.runInNewContext(SHIM, window);
  const native = () => window.__pneuTouchSerial;
  const last = name => calls.findLast(call => call[0] === name);
  const answered = new Set();
  // Answer the oldest unanswered call of this kind, waiting for the shim to make it.
  const settle = async (name, error = null) => {
    for (let i = 0; i < 50; i += 1) {
      const call = calls.find(c => c[0] === name && !answered.has(c[1]));
      if (call) { answered.add(call[1]); native().settle(call[1], error); return; }
      await tick();
    }
    throw new Error(`no ${name} call`);
  };
  return {window, navigator, calls, native, last, settle};
}
async function openedPort(f) {
  const requested = f.navigator.serial.requestPort();
  await f.settle('requestPort'); const port = await requested;
  const opened = port.open({baudRate: 115200, dataBits: 8, stopBits: 1, parity: 'none', flowControl: 'none'});
  await f.settle('open'); await opened;
  return port;
}

test('defines navigator.serial only when absent and a bridge exists', () => {
  const existing = {requestPort() {}};
  assert.equal(fixture({navigator: {serial: existing}}).navigator.serial, existing);
  assert.equal('serial' in fixture({bridge: false}).navigator, false);
  const f = fixture();
  assert.equal('serial' in f.navigator, true);
  assert.equal(f.navigator.serial.androidUsb, true);
  assert.equal(typeof f.navigator.serial.requestPort, 'function');
});

test('requestPort rejects with the native Japanese message (no device / permission denied)', async () => {
  const f = fixture();
  const first = f.navigator.serial.requestPort();
  await f.settle('requestPort', 'USBケーブルでボードがつながっていません');
  await assert.rejects(first, error => error.message === 'USBケーブルでボードがつながっていません' && error.name === 'Error');
  const second = f.navigator.serial.requestPort();
  await f.settle('requestPort', 'USBの使用が許可されませんでした');
  await assert.rejects(second, /許可されませんでした/);
});

test('open passes the serial options, and rejects when native open fails', async () => {
  const f = fixture();
  const requested = f.navigator.serial.requestPort(); await f.settle('requestPort');
  const port = await requested;
  assert.equal(port.readable, null); assert.equal(port.writable, null);
  const failed = port.open({baudRate: 115200});
  assert.deepEqual(f.last('open')[2], {baudRate: 115200, dataBits: 8, stopBits: 1, parity: 'none', flowControl: 'none'});
  await f.settle('open', 'ポートを開けませんでした');
  await assert.rejects(failed, /ポートを開けませんでした/);
  assert.equal(port.readable, null);
  await assert.rejects(port.open({}), /baudRate/);
});

test('SerialConnection reads lines, writes paced bytes, and closes through the shim', async () => {
  const f = fixture(), lines = [], errors = [], events = [];
  const connection = new SerialConnection({baudRate: 115200, maxLine: 160},
    {onLine: line => lines.push(line), onError: e => errors.push(e), onClose: () => events.push('closed')});
  const requested = f.navigator.serial.requestPort(); await f.settle('requestPort');
  const next = await requested;
  const started = connection.start(next); await f.settle('open'); await started;
  f.native().data(encode('PNEU1,1,')); f.native().data(encode('25,-1\r\n'));
  await tick();
  assert.deepEqual(lines, ['PNEU1,1,25,-1\r\n']);
  connection.sleep = async () => {};
  const writing = connection.write('AB', {paceMs: 15});
  for (let i = 0; i < 4; i += 1) await f.settle('write');
  await writing;
  assert.deepEqual(f.calls.filter(c => c[0] === 'write').map(c => c[2]), [[65], [66], [13], [10]]);
  const stopping = connection.stop();
  await f.settle('close'); await stopping;
  assert.deepEqual(errors, []); assert.deepEqual(events, ['closed']);
  assert.equal(next.readable, null); assert.equal(next.writable, null);
});

test('a write error from native rejects the write', async () => {
  const f = fixture(); const port = await openedPort(f);
  const writer = port.writable.getWriter();
  const writing = writer.write(Uint8Array.of(1, 2, 3));
  await tick(); assert.deepEqual(f.last('write')[2], [1, 2, 3]);
  await f.settle('write', '送信できませんでした');
  await assert.rejects(writing, /送信できませんでした/);
});

test('native loss (unplug) errors the stream so the app closes the port', async () => {
  const f = fixture(), errors = [], events = [];
  const requested = f.navigator.serial.requestPort(); await f.settle('requestPort');
  const port = await requested;
  const connection = new SerialConnection({baudRate: 115200, maxLine: 160},
    {onLine() {}, onError: e => errors.push(e), onClose: () => events.push('closed')});
  const started = connection.start(port); await f.settle('open'); await started;
  f.native().lost('USBケーブルが外れました');
  await f.settle('close'); await connection.running;
  assert.deepEqual(errors, ['受信停止: USBケーブルが外れました']);
  assert.deepEqual(events, ['closed']);
  assert.equal(port.readable, null);
});

test('data and settle for unknown ids or closed ports are ignored', async () => {
  const f = fixture();
  f.native().data(encode('ignored')); f.native().settle(999, null); f.native().lost('x');
  const port = await openedPort(f);
  const reader = port.readable.getReader();
  f.native().data(encode('ok'));
  const {value} = await reader.read();
  assert.deepEqual([...value], [...Buffer.from('ok')]);
  reader.releaseLock();
  const closing = port.close(); await f.settle('close'); await closing;
  f.native().data(encode('late'));
  assert.equal(port.readable, null);
});
