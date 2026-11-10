// PneutouchAI / Kanata the Kid Creator — live firmware frames and PAI1/PST1 replies.
import {parseLine} from '../shared/protocol.mjs';
const uint = s => /^\d{1,10}$/.test(s) && Number(s) <= 0xffffffff;
export const KNOWN_PARTS = Object.freeze({
  DINO: Object.freeze(['TAIL', 'BACK', 'LEGS', 'HEAD']),
  KRAG: Object.freeze(['HAT', 'FACE']),
});
export const ALL_PARTS = Object.freeze([...new Set(Object.values(KNOWN_PARTS).flat())]);
export const DEFAULT_OBJECT = 'DINO';
// Firmware tag (or a slot name's first four characters) to a known object; anything else is DINO.
export const objectOf = tag => Object.hasOwn(KNOWN_PARTS, tag?.slice(0, 4) ?? '') ? tag.slice(0, 4) : DEFAULT_OBJECT;
const keyValues = fields => Object.fromEntries(fields.filter(v => v.includes('='))
  .map(v => [v.slice(0, v.indexOf('=')), v.slice(v.indexOf('=')+1)]));

function parseLive(f) {
  if (f[0] === '# PNEE1' && f.length === 4 && uint(f[1]) && uint(f[3]) &&
      ['START','INVALID','TIMEOUT'].includes(f[2]))
    return {type: 'event', id: +f[1], action: f[2], ms: +f[3]};
  if (f[0] === '# PNEC1' && f.length === 9 && uint(f[1]) && uint(f[2]) && uint(f[4]) &&
      [...ALL_PARTS,'UNKNOWN'].includes(f[3]) && f.slice(5).every(v => /^[\da-f]{4}$/i.test(v)))
    return {type: 'result', id: +f[1], ms: +f[2], part: f[3], inferenceUs: +f[4]};
  if (f[0] === '# PNEF1' && f.length === 11 && f.slice(1,4).every(uint) &&
      ['0','6'].includes(f[4]) && f.slice(5).every(uint))
    return {type: 'features', id: +f[1], trigger: +f[2], end: +f[3], offset: +f[4],
      values: f.slice(5).map(v => Number(v)/1000)};
  return null;
}

// `# PNEM3` follows every (re)training; `# PNES1` is the device SW3 slot switch.
function parseModel(f) {
  if (f[0] === '# PNEM3' && ['READY','ERROR'].includes(f[1])) {
    const kv = keyValues(f.slice(2));
    if (!uint(kv.slot) || !uint(kv.count)) return null;
    return {type: 'object', object: objectOf(kv.object), slot: +kv.slot, count: +kv.count};
  }
  if (f[0] !== '# PNES1') return null;
  if (f[1] === 'ERROR' && f.length === 3) return {type: 'slot-error', why: f[2]};
  const kv = keyValues(f.slice(1));
  return f.length === 3 && uint(kv.slot) && uint(kv.count) ? {type: 'slot-switch', slot: +kv.slot, count: +kv.count} : null;
}

// Replies carry no CRC. MODE lines are their own type (mode 1 also clears the live state);
// every other `# PST1,…` / `# PAI1,…` reply is a store line with the fields after the op.
function parseStore(f, text) {
  if (!['# PST1', '# PAI1'].includes(f[0]) || f.length < 3) return null;
  if (f[0] === '# PAI1' && f[1] === 'MODE')
    return ['0','1'].includes(f[2]) ? {type: 'mode', mode: +f[2], text} : null;
  return {type: 'store', family: f[0].slice(2), op: f[1], fields: f.slice(2), text};
}

export function parseDemoLine(line) {
  const sample = parseLine(line);
  if (sample) return {type: 'sample', ...sample};
  const text = line.trim(), f = text.split(',');
  const parsed = parseLive(f) ?? parseModel(f) ?? parseStore(f, text);
  if (parsed) return parsed;
  if (/^# PneutouchAi (Live;|live acquisition resumed)/.test(line) || /^# ERROR,HX710B_TIMEOUT/.test(line))
    return {type: 'reset'};
  return null;
}
export const msDelta = (a, b) => (a-b) << 0;
