// StoreClient against a scripted device: replies are fed back like parsed UART lines.
import test from 'node:test';
import assert from 'node:assert/strict';
import {StoreClient} from '../src/demo-visualizer/store.mjs';
import {parseDemoLine} from '../src/demo-visualizer/protocol.mjs';
import {crc16} from '../src/shared/crc16.mjs';

// reply(body) returns the device lines for one command body (without CRC), or [] for silence.
// Every write call is logged with its options; the empty line before a MODE packet is not a command.
function device(reply, options = {}) {
  const sent = [], writes = [], sleeps = [];
  let client;
  const write = async (line, writeOptions) => {
    writes.push([line, writeOptions]);
    if (line === '') return;
    const [body, crc] = line.split('*');
    assert.equal(parseInt(crc, 16), crc16(body), line);
    sent.push(body);
    queueMicrotask(() => { for (const text of reply(body)) client.feed(parseDemoLine(text + '\r\n')); });
  };
  const sleep = async ms => { sleeps.push(ms); };
  client = new StoreClient({write, sleep, ...options});
  return {client, sent, writes, sleeps};
}
const slotLine = (id, n, state, count, name) => `# PST1,SLOT,${id},${n},${state},${count},${count},4,${name},4,90:80:70:60,0f0f`;
function firmware(body) {
  const [family, op, a, b] = body.split(',');
  if (family === 'PAI1') return [`# PAI1,MODE,${a}`];
  if (op === 'INFO') return [`# PST1,INFO,${a},2,0,0,221,141,ss6-thresh`];
  if (op === 'SLOT') return [slotLine(a, b, 2, b === '2' ? 221 : 141, b === '2' ? '1023-176' : 'KRAGBTN')];
  if (op === 'USE') return [`# PST1,USE,${a},${b}`];
  return [];
}

test('listSlots: MODE 1, INFO, SLOT for each non-empty slot, MODE 0; object from the name', async () => {
  const {client, sent} = device(firmware);
  const list = client.listSlots();
  assert.equal(client.busy, true);
  assert.deepEqual(await list, {active: 2, firmware: 'ss6-thresh', slots: [
    {slot: 2, count: 221, state: 2, name: '1023-176', object: 'DINO', k: 4, tunedK: 4},
    {slot: 3, count: 141, state: 2, name: 'KRAGBTN', object: 'KRAG', k: 4, tunedK: 4}]});
  assert.deepEqual(sent, ['PAI1,MODE,1', 'PST1,INFO,1', 'PST1,SLOT,2,2', 'PST1,SLOT,3,3', 'PAI1,MODE,0']);
  assert.equal(client.busy, false);
});
test('useSlot: MODE 1, USE n, MODE 0; token ids keep increasing', async () => {
  const {client, sent} = device(firmware, {idStart: 40});
  await client.useSlot(3);
  await client.useSlot(2);
  assert.deepEqual(sent, ['PAI1,MODE,1', 'PST1,USE,40,3', 'PAI1,MODE,0', 'PAI1,MODE,1', 'PST1,USE,41,2', 'PAI1,MODE,0']);
});
test('ERROR reply rejects with the reason and still restores MODE 0', async () => {
  const {client, sent} = device(body => body.startsWith('PST1') ? ['# PST1,ERROR,STORE'] : firmware(body));
  await assert.rejects(client.useSlot(1), error => error.why === 'STORE' && /STORE/.test(error.message));
  assert.deepEqual(sent, ['PAI1,MODE,1', 'PST1,USE,1,1', 'PAI1,MODE,0']);
  assert.equal(client.busy, false);
});
test('a silent device (older firmware) times out, and the sequence still ends with MODE 0', async () => {
  const {client, sent} = device(body => body.startsWith('PST1') ? [] : firmware(body), {timeoutMs: 20});
  await assert.rejects(client.listSlots(), error => error.why === 'TIMEOUT');
  assert.deepEqual(sent, ['PAI1,MODE,1', 'PST1,INFO,1', 'PAI1,MODE,0']);
});
test('sequences are serialized; unrelated lines and stale ids are ignored', async () => {
  const {client, sent} = device(body => {
    const [, op, a] = body.split(',');
    if (op === 'INFO') return ['# PST1,INFO,99,0,0,0,0,0,x', 'PNEU1,1,2,3', `# PST1,INFO,${a},0,5,0,0,0,fw`];
    return firmware(body);
  });
  const [one, two] = await Promise.all([client.listSlots(), client.useSlot(0)]);
  assert.deepEqual(one.slots.map(s => s.slot), [0]); assert.equal(two, undefined);
  assert.deepEqual(sent, ['PAI1,MODE,1', 'PST1,INFO,1', 'PST1,SLOT,2,0', 'PAI1,MODE,0',
    'PAI1,MODE,1', 'PST1,USE,3,0', 'PAI1,MODE,0']);
});
test('a failed write rejects; cancel rejects the pending command', async () => {
  const failing = new StoreClient({write: async () => { throw new Error('not connected'); }});
  await assert.rejects(failing.useSlot(1), /not connected/);
  assert.equal(failing.busy, false);
  const {client} = device(() => [], {timeoutMs: 10000});
  const pending = client.listSlots();
  await new Promise(resolve => setImmediate(resolve));
  client.cancel('CLOSED');
  await assert.rejects(pending, error => error.why === 'CLOSED');
  assert.equal(client.feed(null), false);
});
test('the browser offers only committed slots with at least four records', async () => {
  const {usableSlots, reasonOf} = await import('../src/demo-visualizer/slots-ui.mjs');
  const slots = [{slot: 0, state: 2, count: 4}, {slot: 1, state: 1, count: 40}, {slot: 2, state: 2, count: 3}, {slot: 3, state: 2, count: 141}];
  assert.deepEqual(usableSlots({active: 0, slots}).map(s => s.slot), [0, 3]);
  assert.equal(reasonOf(Object.assign(new Error('x'), {why: 'STORE'})), 'STORE');
  assert.equal(reasonOf(new Error('closed')), 'closed');
});
test('MODE packets are paced: a bare line ending, 50 ms, then one byte every 15 ms; others are bursts', async () => {
  const {client, writes, sleeps} = device(firmware);
  await client.useSlot(3);
  assert.deepEqual(writes, [
    ['', undefined], ['PAI1,MODE,1*1f24', {paceMs: 15}], ['PST1,USE,1,3*' + crc16('PST1,USE,1,3').toString(16).padStart(4, '0'), undefined],
    ['', undefined], ['PAI1,MODE,0*' + crc16('PAI1,MODE,0').toString(16).padStart(4, '0'), {paceMs: 15}]]);
  assert.deepEqual(sleeps, [50, 50]);
});
test('a line error on MODE 1 (live stream dropped bytes) is retried and the sequence succeeds', async () => {
  let failures = 1;
  const {client, sent} = device(body => (body === 'PAI1,MODE,1' && failures-- > 0 ? ['# PAI1,ERROR,CRC'] : firmware(body)));
  await client.useSlot(3);
  assert.deepEqual(sent, ['PAI1,MODE,1', 'PAI1,MODE,1', 'PST1,USE,1,3', 'PAI1,MODE,0']);
});
test('three line errors on MODE reject; CRC_FORMAT and OVERLONG count as line errors too', async () => {
  const errors = ['# PAI1,ERROR,CRC_FORMAT', '# PAI1,ERROR,OVERLONG', '# PAI1,ERROR,CRC'];
  const {client, sent} = device(body => (body === 'PAI1,MODE,1' ? [errors.shift() ?? '# PAI1,ERROR,CRC'] : firmware(body)));
  await assert.rejects(client.useSlot(3), error => error.why === 'CRC');
  assert.deepEqual(sent, ['PAI1,MODE,1', 'PAI1,MODE,1', 'PAI1,MODE,1', 'PAI1,MODE,0']);
});
test('other errors on MODE and errors on PST1 commands are not retried', async () => {
  const {client, sent} = device(body => (body === 'PAI1,MODE,1' ? ['# PAI1,ERROR,BUSY'] : firmware(body)));
  await assert.rejects(client.useSlot(3), error => error.why === 'BUSY');
  assert.deepEqual(sent, ['PAI1,MODE,1', 'PAI1,MODE,0']);
});
