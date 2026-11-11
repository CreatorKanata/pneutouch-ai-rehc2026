// One visible video. New results replace an in-progress reaction; timers cannot reset a newer clip.
// Idle play() rejections other than an autoplay block (e.g. AbortError) are retried quietly.
const IDLE_RETRY_MS = 500, IDLE_RETRY_LIMIT = 3;
export class VideoPlayback {
  constructor(idle, reaction, urls, {onChange = () => {}, onError = () => {}, timers = globalThis} = {}) {
    Object.assign(this, {idle, reaction, urls, onChange, onError, timers});
    this.token = 0; this.timer = null; this.part = 'static'; this.ready = false; this.idleRetries = 0;
    idle.loop = true; reaction.loop = false;
    idle.src = urls.static;
    reaction.addEventListener('timeupdate', () => {
      if (this.part !== 'static' && reaction.currentTime >= 5) this.showIdle();
    });
    reaction.addEventListener('ended', () => this.showIdle());
    reaction.addEventListener('error', () => {
      if (this.part !== 'static') { this.showIdle(); this.onError('リアクション動画を読み込めませんでした'); }
    });
    idle.addEventListener('error', () => this.onError('待機動画を読み込めませんでした'));
  }
  async showIdle(retry = false) {
    const token = ++this.token;
    this.timers.clearTimeout(this.timer); this.timer = null;
    if (!retry) this.idleRetries = 0;
    this.part = 'static'; this.ready = false; this.reaction.pause();
    this.idle.hidden = false; this.reaction.hidden = true; this.onChange('static');
    try { await this.idle.play(); this.idleRetries = 0; }
    catch (error) {
      if (token !== this.token) return;
      if (error?.name === 'NotAllowedError') this.onError('画面の「動画を再生」を押してください', {needsGesture: true});
      else if (this.idleRetries >= IDLE_RETRY_LIMIT) this.onError('待機動画を再生できませんでした');
      else {
        this.idleRetries++;
        this.timer = this.timers.setTimeout(() => { if (token === this.token) this.showIdle(true); }, IDLE_RETRY_MS);
      }
    }
  }
  // Another object's clips (keys: static + upper-case part names); any pending reaction is dropped.
  setObject(urls) {
    this.urls = urls; this.idle.src = urls.static;
    return this.showIdle();
  }
  async play(part) {
    const url = Object.hasOwn(this.urls, part) && part !== 'static' ? this.urls[part] : null;
    if (!url) return;
    const token = ++this.token;
    this.timers.clearTimeout(this.timer); this.timer = null;
    this.part = part; this.ready = false; this.idleRetries = 0;
    this.reaction.pause(); this.reaction.src = url; this.reaction.currentTime = 0;
    // Keep static visible while the new clip is loading.
    this.reaction.hidden = true; this.idle.hidden = false;
    this.onChange(part, true);
    try {
      await this.reaction.play();
      if (token !== this.token) return;
      this.ready = true; this.idle.pause(); this.idle.hidden = true; this.reaction.hidden = false;
      this.onChange(part, false);
      this.timer = this.timers.setTimeout(() => { if (token === this.token) this.showIdle(); }, 5000);
    } catch {
      if (token === this.token) { this.showIdle(); this.onError('動画を再生できませんでした'); }
    }
  }
  setMuted(muted) { this.idle.muted = muted; this.reaction.muted = muted; }
}
