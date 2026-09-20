// PneutouchAI / Kanata the Kid Creator — receive-only live firmware frames.
import {parseLine} from '../shared/protocol.mjs';
const uint = s => /^\d{1,10}$/.test(s) && Number(s) <= 0xffffffff;
export const PARTS = ['TAIL', 'BACK', 'LEGS', 'HEAD'];
export function parseDemoLine(line) {
  const sample = parseLine(line);
  if (sample) return {type: 'sample', ...sample};
  const f = line.trim().split(',');
  if (f[0] === '# PNEE1' && f.length === 4 && uint(f[1]) && uint(f[3]) &&
      ['START','INVALID','TIMEOUT'].includes(f[2]))
    return {type: 'event', id: +f[1], action: f[2], ms: +f[3]};
  if (f[0] === '# PNEC1' && f.length === 9 && uint(f[1]) && uint(f[2]) && uint(f[4]) &&
      [...PARTS,'UNKNOWN'].includes(f[3]) && f.slice(5).every(v => /^[\da-f]{4}$/i.test(v)))
    return {type: 'result', id: +f[1], ms: +f[2], part: f[3], inferenceUs: +f[4]};
  if (f[0] === '# PNEF1' && f.length === 11 && f.slice(1,4).every(uint) &&
      ['0','6'].includes(f[4]) && f.slice(5).every(uint))
    return {type: 'features', id: +f[1], trigger: +f[2], end: +f[3], offset: +f[4],
      values: f.slice(5).map(v => Number(v)/1000)};
  if (/^# PneutouchAi (Live;|live acquisition resumed)/.test(line) ||
      /^# ERROR,HX710B_TIMEOUT/.test(line) || /^# PAI1,MODE,1/.test(line))
    return {type: 'reset'};
  return null;
}
export const msDelta = (a, b) => (a-b) << 0;
