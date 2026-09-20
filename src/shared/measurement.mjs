// Bounded raw samples and explicit labels; missing samples are never invented.
export class Measurement {
  constructor(settings) {
    this.settings = settings; this.points = []; this.rows = [];
    this.recording = false; this.dirty = false;
    this.gaps = 0; this.resets = 0; this.invalid = 0;
  }
  resetTrace() { this.points = []; }
  start() { this.recording = this.rows.length < this.settings.recordingSamples; }
  stop() { this.recording = false; }
  add(sample, label, utc) {
    const previous = this.points.at(-1);
    const delta = previous ? (sample.ms - previous.ms) >>> 0 : 0;
    const step = previous ? (sample.seq - previous.seq) >>> 0 : 1;
    // Counter wrap gives a small forward delta; reset usually goes backwards.
    const reset = previous && (delta > 0x7fffffff || step > 0x7fffffff);
    const gap = previous && (step !== 1 || reset);
    if (reset) { this.resets++; this.points = []; }
    if (gap) this.gaps++;
    const elapsed = this.points.length ? previous.elapsed + delta : 0;
    this.points.push({...sample, elapsed, gap: Boolean(gap)});
    if (this.points.length > this.settings.displaySamples) this.points.shift();
    if (this.recording) {
      this.rows.push({...sample, label, utc}); this.dirty = true;
      if (this.rows.length >= this.settings.recordingSamples) this.stop();
    }
    return {gap: Boolean(gap), reset: Boolean(reset)};
  }
  get rate() {
    const duration = this.points.at(-1)?.elapsed - this.points[0]?.elapsed;
    return duration > 0 ? (this.points.length - 1) * 1000 / duration : null;
  }
}
