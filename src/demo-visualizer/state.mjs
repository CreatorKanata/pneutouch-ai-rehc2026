// Event association and independent display deadlines, without a body classifier.
import {msDelta} from './protocol.mjs';
export class DemoState {
  constructor() { this.reset(); }
  reset() {
    this.samples = []; this.events = new Map(); this.lastSample = null;
    this.phase = 'idle'; this.part = null; this.activeId = null; this.deadline = 0;
    this.trace = null; this.lastResult = null; this.traceDeadline = 0;
  }
  consume(p, now) {
    if (!p) return [];
    if (p.type === 'reset') { this.reset(); return ['reset']; }
    if (p.type === 'sample') {
      const prev = this.lastSample;
      const gap = prev && (((p.seq-prev.seq) >>> 0) !== 1 || msDelta(p.ms,prev.ms) <= 0 || msDelta(p.ms,prev.ms) > 100);
      if (gap) this.reset();
      this.samples.push(p); if (this.samples.length > 800) this.samples.shift();
      this.lastSample = p;
      if (this.lastResult && msDelta(p.ms,this.lastResult.end) <= 350) this.updateTrace();
      return gap ? ['reset'] : [];
    }
    if (p.type === 'event' && p.action === 'START') {
      if (this.events.has(p.id)) return [];
      this.events.set(p.id, {id: p.id, trigger: p.ms, result: false});
      while (this.events.size > 16) this.events.delete(this.events.keys().next().value);
      this.activeId = p.id; this.phase = 'pressed'; this.part = null; this.deadline = now+5000;
      return ['press'];
    }
    if (p.type === 'event') {
      if (this.activeId !== p.id) return [];
      this.phase = 'idle'; this.part = null; this.activeId = null; this.deadline = 0;
      return ['rejected'];
    }
    if (p.type === 'result') {
      if (this.events.get(p.id)?.result) return [];
      if (this.activeId !== null && this.activeId !== p.id) return [];
      const e = this.events.get(p.id) ?? {id: p.id, trigger: null};
      Object.assign(e, {result: true, part: p.part, end: this.lastSample?.ms ?? p.ms,
        predictionMs: p.ms, inferenceUs: p.inferenceUs, exactBounds: false});
      this.events.set(p.id, e); this.lastResult = e;
      while (this.events.size > 16) this.events.delete(this.events.keys().next().value);
      this.activeId = null; this.part = p.part; this.phase = p.part === 'UNKNOWN' ? 'unknown' : 'result';
      this.deadline = now+3000; this.traceDeadline = now+5000; this.updateTrace();
      return ['result'];
    }
    if (p.type === 'features') {
      const e = this.events.get(p.id);
      if (!e?.result) return [];
      e.trigger = p.trigger; e.end = p.end; e.exactBounds = true;
      e.features ??= new Array(12); e.features.splice(p.offset, 6, ...p.values);
      if (this.lastResult?.id === p.id) this.updateTrace();
    }
    return [];
  }
  updateTrace() {
    const e = this.lastResult;
    // A mid-event connection cannot reconstruct samples that were never received.
    const trigger = e.trigger ?? ((e.end-3000) >>> 0);
    const points = this.samples.filter(p => msDelta(p.ms,trigger) >= -850 && msDelta(p.ms,e.end) <= 350);
    const baseline = points.filter(p => msDelta(p.ms,trigger) >= -825 && msDelta(p.ms,trigger) <= -325)
      .map(p => p.raw).sort((a,b) => a-b);
    const n = baseline.length;
    const base = n >= 5 ? (baseline[Math.floor((n-1)/2)]+baseline[Math.floor(n/2)])/2 : null;
    this.trace = {id: e.id, part: e.part, trigger, end: e.end, predictionMs: e.predictionMs,
      exactBounds: e.exactBounds, base, points: points.map(p => ({t: msDelta(p.ms,trigger)/1000, raw: p.raw})),
      partial: !points.length || msDelta(points[0].ms,trigger) > -700};
  }
  tick(now) {
    let changed = false;
    if (this.deadline && now >= this.deadline) {
      this.phase = 'idle'; this.part = null; this.activeId = null; this.deadline = 0;
      changed = true;
    }
    if (this.traceDeadline && now >= this.traceDeadline) {
      this.traceDeadline = 0;
      changed = true;
    }
    return changed;
  }
}
