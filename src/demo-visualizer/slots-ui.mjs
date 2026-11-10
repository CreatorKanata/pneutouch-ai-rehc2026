// Browser-side slot switching: a <select> of committed slots, a "モデル一覧" button and a status text.
// Switching goes through the firmware; the resulting `# PNEM3` switches the mode like SW3 does.
export const MIN_RECORDS = 4;
export const COMMITTED = 2;
export const LIST_FAILED = 'スロット一覧を取得できませんでした（古いファームウェアか、ボードが処理中です）。「モデル一覧」で再試行できます。';
export const LIST_FAILED_MS = 8000;
export const usableSlots = list => list.slots.filter(s => s.state === COMMITTED && s.count >= MIN_RECORDS);
export const reasonOf = error => error?.why ?? error?.message ?? String(error);
export const optionLabels = (slots, active, labelOf) => slots
  .filter(s => s.state === COMMITTED && s.count >= MIN_RECORDS)
  .map(s => ({value: String(s.slot), active: s.slot === active,
    text: `スロット ${s.slot} · ${labelOf(s.object)} · ${s.count}件${s.slot === active ? ' · 使用中' : ''}`}));

export class SlotPicker {
  // labelOf(object) → '恐竜'; onPause() keeps the stall warning off; onSwitching(slot) announces a switch;
  // onNotice(text, ms) shows a non-error notice; onError(text) a red one.
  constructor({select, refresh, status, store, labelOf, onPause, onSwitching, onError, onNotice,
    createOption = () => document.createElement('option')}) {
    Object.assign(this, {select, refresh, status, store, labelOf, onPause, onSwitching, onError, onNotice, createOption});
    this.connected = false;
    select.addEventListener('change', () => this.use(Number(select.value)));
    refresh.addEventListener('click', () => this.list());
    this.reset();
  }
  reset() {
    this.slots = null; this.active = null; this.statusText = '';
    this.render();
  }
  setConnected(connected) {
    this.connected = connected;
    if (!connected) this.reset(); else this.sync();
  }
  sync() {
    const busy = this.store.busy, text = busy ? this.statusText : '';
    this.select.hidden = !this.connected || !this.select.options.length; this.refresh.hidden = !this.connected;
    this.select.disabled = busy; this.refresh.disabled = busy;
    this.status.textContent = text; this.status.hidden = !text;
  }
  render() {
    const labels = this.slots ? optionLabels(this.slots, this.active, this.labelOf) : [];
    this.select.replaceChildren(...labels.map(({value, text}) => Object.assign(this.createOption(), {value, textContent: text})));
    this.select.selectedIndex = labels.findIndex(o => o.active);
    this.sync();
  }
  // The active slot from `# PNEM3` (with its record count) or from the list; relabels without a new listing.
  markActive(slot, count) {
    this.active = slot;
    if (this.slots && count !== undefined && count !== null)
      this.slots = this.slots.map(s => (s.slot === slot ? {...s, count} : s));
    this.render();
  }
  async list() {
    this.statusText = 'スロット一覧を取得中…'; this.onPause();
    try {
      const list = await this.pending(this.store.listSlots());
      this.slots = list.slots; this.active = list.active;
    } catch (error) {
      console.debug('PST1 slot list unavailable', error);
      this.slots = null; this.onNotice(LIST_FAILED, LIST_FAILED_MS);
    } finally { this.render(); }
  }
  async use(slot) {
    const before = this.active;
    this.statusText = `スロット ${slot} に切り替え中…`; this.onSwitching(slot);
    try {
      await this.pending(this.store.useSlot(slot));
    } catch (error) {
      this.active = before;
      this.onError(`スロット ${slot} に切り替えられませんでした（${reasonOf(error)}）`);
    } finally { this.render(); }
  }
  // Disable the controls and show the status as soon as a sequence is queued.
  pending(promise) { this.sync(); return promise; }
}
