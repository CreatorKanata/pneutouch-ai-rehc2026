// PST1 model-store client for the viewer: list the slots and make one active.
// Every sequence pauses acquisition first (PAI1 MODE 1: a long packet can be dropped in live mode)
// and always ends with MODE 0, after which the firmware retrains and prints `# PNEM3`.
import {framePacket} from '../shared/crc16.mjs';
import {objectOf} from './protocol.mjs';

// MODE packets go out while the live stream may still be running: a bare line ending first (ends any
// partial command), a pause, then one byte at a time. Line-level errors mean bytes were lost: try again.
const MODE_LEAD_MS = 50, MODE_PACE_MS = 15, MODE_ATTEMPTS = 3;
const LINE_ERRORS = ['CRC', 'CRC_FORMAT', 'OVERLONG'];
const storeError = (why, detail = why) => Object.assign(new Error(`PST1: ${detail}`), {why});
const count = v => (/^\d+$/.test(v ?? '') ? Number(v) : NaN);
const matches = (text, prefix) => text === prefix || text.startsWith(prefix + ',');

// `# PST1,SLOT,<id>,<n>,<state>,<count>,<pool>,<k>,<name>,<tuned_k>,<recall>,<mask>`
function parseSlot(fields) {
  const [, slot, state, records, , k, name = '', tunedK] = fields;
  const numbers = [slot, state, records, k, tunedK].map(count);
  if (numbers.some(Number.isNaN)) throw storeError('REPLY', `SLOT ${fields.join(',')}`);
  return {slot: numbers[0], count: numbers[2], state: numbers[1], name, object: objectOf(name), k: numbers[3], tunedK: numbers[4]};
}
// `# PST1,INFO,<id>,<active>,<c0>,…,<c3>,<fw>`
function parseInfo(fields) {
  const active = count(fields[1]), counts = fields.slice(2, -1).map(count);
  if (Number.isNaN(active) || !counts.length || counts.some(Number.isNaN)) throw storeError('REPLY', `INFO ${fields.join(',')}`);
  return {active, counts, firmware: fields.at(-1)};
}

export class StoreClient {
  // write(text, options?) sends one line; sleep(ms) is injectable so tests do not wait.
  constructor({write, timeoutMs = 8000, idStart = 1, timers = globalThis,
    sleep = ms => new Promise(resolve => timers.setTimeout(resolve, ms))}) {
    Object.assign(this, {write, timeoutMs, timers, sleep});
    this.nextId = idStart; this.pending = null; this.active = 0; this.epoch = 0; this.queue = Promise.resolve();
  }
  get busy() { return this.active > 0; }
  // Sends `body*crc`; resolves with the parsed reply whose text starts with expectPrefix.
  command(body, expectPrefix, writeOptions) {
    if (this.pending) return Promise.reject(storeError('BUSY'));
    return new Promise((resolve, reject) => {
      const pending = {expectPrefix, resolve, reject};
      pending.timer = this.timers.setTimeout(() => this.settle(pending, null, storeError('TIMEOUT', `${body} timed out`)), this.timeoutMs);
      this.pending = pending;
      const packet = framePacket(body);
      Promise.resolve().then(() => (writeOptions ? this.write(packet, writeOptions) : this.write(packet)))
        .catch(error => this.settle(pending, null, error));
    });
  }
  settle(pending, reply, error) {
    if (this.pending !== pending) return;
    this.timers.clearTimeout(pending.timer); this.pending = null;
    if (error) pending.reject(error); else pending.resolve(reply);
  }
  // Every parsed line goes here; returns true when it answered the pending command.
  feed(p) {
    const pending = this.pending;
    if (!pending || !p || !['store', 'mode'].includes(p.type)) return false;
    if (matches(p.text, pending.expectPrefix)) { this.settle(pending, p); return true; }
    if (p.type !== 'store' || p.op !== 'ERROR') return false;
    this.settle(pending, null, storeError(p.fields[0] ?? 'ERROR', `${p.family} ERROR ${p.fields.join(',')}`));
    return true;
  }
  // The port closed: reject what is waiting and skip the closing MODE 0 of that sequence.
  cancel(why = 'CLOSED') {
    this.epoch++;
    if (this.pending) this.settle(this.pending, null, storeError(why));
  }
  // PAI1 MODE with pacing; retried (fresh attempt) when the firmware saw a damaged line.
  async mode(value) {
    for (let attempt = 1; ; attempt++) {
      try {
        await this.write('');
        await this.sleep(MODE_LEAD_MS);
        return await this.command(`PAI1,MODE,${value}`, `# PAI1,MODE,${value}`, {paceMs: MODE_PACE_MS});
      } catch (error) {
        if (attempt >= MODE_ATTEMPTS || !LINE_ERRORS.includes(error.why)) throw error;
      }
    }
  }
  pst(op, ...args) {
    const id = this.nextId++;
    return this.command(['PST1', op, id, ...args].join(','), `# PST1,${op},${id}`);
  }
  serialize(steps) {
    this.active++;
    const run = this.queue.then(() => this.paused(steps));
    this.queue = run.catch(() => {});
    return run.finally(() => { this.active--; });
  }
  async paused(steps) {
    const epoch = this.epoch;
    let result, failure = null;
    try { await this.mode(1); result = await steps(); }
    catch (error) { failure = error; }
    if (epoch === this.epoch) {
      try { await this.mode(0); } catch (error) { failure ??= error; }
    }
    if (failure) throw failure;
    return result;
  }
  listSlots() {
    return this.serialize(async () => {
      const info = parseInfo((await this.pst('INFO')).fields);
      const used = info.counts.flatMap((records, slot) => (records > 0 ? [slot] : []));
      const slots = await used.reduce(async (done, slot) => {
        const before = await done;
        return [...before, parseSlot((await this.pst('SLOT', slot)).fields)];
      }, Promise.resolve([]));
      return {active: info.active, firmware: info.firmware, slots};
    });
  }
  useSlot(slot) {
    return this.serialize(async () => { await this.pst('USE', slot); });
  }
}
