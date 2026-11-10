// SlotPicker visibility and status with minimal fake DOM elements (no browser).
import test from 'node:test';
import assert from 'node:assert/strict';
import {SlotPicker, LIST_FAILED, optionLabels} from '../src/demo-visualizer/slots-ui.mjs';

const element = () => ({hidden: false, disabled: false, textContent: '', handlers: {},
  addEventListener(name, fn) { this.handlers[name] = fn; }});
function setup(store) {
  const select = {...element(), options: [], selectedIndex: -1, value: '',
    replaceChildren(...options) { this.options = options; }};
  const refresh = element(), status = element(), events = [];
  const picker = new SlotPicker({select, refresh, status, store, labelOf: o => ({DINO: '恐竜', KRAG: 'からあげ'})[o],
    onPause: () => events.push('pause'), onSwitching: n => events.push(`switch ${n}`),
    onError: text => events.push(`error ${text}`), onNotice: (text, ms) => events.push(`notice ${ms} ${text}`),
    createOption: () => ({})});
  return {picker, select, refresh, status, events};
}
const LIST = {active: 2, firmware: 'fw', slots: [
  {slot: 2, count: 221, state: 2, name: '1023-176', object: 'DINO'},
  {slot: 3, count: 141, state: 2, name: 'KRAGBTN', object: 'KRAG'},
  {slot: 1, count: 2, state: 1, name: 'DINOBTN', object: 'DINO'}]};
function fakeStore(listSlots) {
  const store = {busy: false, listSlots: async () => { store.busy = true; try { return await listSlots(); } finally { store.busy = false; } },
    useSlot: async () => {}};
  return store;
}

test('the button shows whenever connected; the select only with a list', async () => {
  const f = setup(fakeStore(async () => LIST));
  assert.equal(f.refresh.hidden, true); assert.equal(f.select.hidden, true);
  f.picker.setConnected(true);
  assert.equal(f.refresh.hidden, false); assert.equal(f.select.hidden, true);
  const pending = f.picker.list();
  assert.equal(f.refresh.disabled, true); assert.equal(f.status.textContent, 'スロット一覧を取得中…');
  await pending;
  assert.equal(f.select.hidden, false); assert.equal(f.refresh.disabled, false); assert.equal(f.status.hidden, true);
  assert.deepEqual(f.select.options.map(o => o.textContent), ['スロット 2 · 恐竜 · 221件 · 使用中', 'スロット 3 · からあげ · 141件']);
  assert.equal(f.select.selectedIndex, 0);
  f.picker.setConnected(false);
  assert.equal(f.refresh.hidden, true); assert.equal(f.select.hidden, true);
});
test('a failed listing shows the retry notice for 8 s and keeps the select hidden', async () => {
  const f = setup(fakeStore(async () => { throw Object.assign(new Error('x'), {why: 'TIMEOUT'}); }));
  f.picker.setConnected(true); await f.picker.list();
  assert.deepEqual(f.events, ['pause', `notice 8000 ${LIST_FAILED}`]);
  assert.equal(f.select.hidden, true); assert.equal(f.refresh.hidden, false); assert.equal(f.refresh.disabled, false);
  f.refresh.handlers.click(); // the button retries
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(f.events.filter(e => e === 'pause').length, 2);
});
test('switching shows its status; the next # PNEM3 relabels the active slot without a new listing', async () => {
  let release;
  const store = fakeStore(async () => LIST);
  store.useSlot = () => { store.busy = true; return new Promise(resolve => { release = () => { store.busy = false; resolve(); }; }); };
  const f = setup(store); f.picker.setConnected(true); await f.picker.list();
  f.select.value = '3'; const pending = f.select.handlers.change();
  assert.equal(f.status.textContent, 'スロット 3 に切り替え中…'); assert.equal(f.select.disabled, true);
  release(); await pending;
  f.picker.markActive(3, 145);
  assert.deepEqual(f.select.options.map(o => o.textContent), ['スロット 2 · 恐竜 · 221件', 'スロット 3 · からあげ · 145件 · 使用中']);
  assert.equal(f.select.selectedIndex, 1); assert.deepEqual(f.events, ['pause', 'switch 3']);
});
test('option labels: committed slots with four records, active marked', () => {
  assert.deepEqual(optionLabels(LIST.slots, 3, o => o), [
    {value: '2', text: 'スロット 2 · DINO · 221件', active: false},
    {value: '3', text: 'スロット 3 · KRAG · 141件 · 使用中', active: true}]);
});
