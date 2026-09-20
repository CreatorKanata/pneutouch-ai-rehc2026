// Phase 0 serial framing and CSV: shared by the browser and Node host tests.
export function parseLine(line) {
  const match = /^PNEU1,([0-9]{1,10}),([0-9]{1,10}),(-?[0-9]{1,7})\r?\n$/.exec(line);
  if (!match) return null;
  const [seq, ms, raw] = match.slice(1).map(Number);
  if (seq > 0xffffffff || ms > 0xffffffff || raw < -8388608 || raw > 8388607) return null;
  return {seq, ms, raw};
}

export class LineBuffer {
  constructor(limit) { this.limit = limit; this.pending = ''; this.discarding = false; this.dropped = 0; }
  *feed(chunk) {
    for (const char of chunk) {
      if (!this.discarding) {
        this.pending += char;
        if (this.pending.length > this.limit) { this.pending = ''; this.discarding = true; }
      }
      if (char === '\n') {
        if (!this.discarding) yield this.pending;
        else this.dropped++;
        this.pending = ''; this.discarding = false;
      }
    }
  }
}

export function csv(rows) {
  return ['host_utc,seq,mcu_ms,raw,label', ...rows.map(row =>
    [row.utc, row.seq, row.ms, row.raw, row.label].map(value =>
      '"' + String(value).replaceAll('"', '""') + '"').join(','))].join('\r\n') + '\r\n';
}
