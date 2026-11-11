// PneutouchAI — HTML / Web Serial demo. Inference is performed by the physical SolistAI.
import {SerialConnection} from '../shared/serial.mjs';
import {parseDemoLine} from './protocol.mjs';
import {DemoState} from './state.mjs';
import {VideoPlayback} from './media.mjs';
import {validateAssets} from './assets.mjs';
import {StoreClient} from './store.mjs';
import {SlotPicker} from './slots-ui.mjs';
import {drawTrace} from './chart.mjs';
const $ = id => document.getElementById(id);
const FALLBACK = {label: '恐竜', names: {}, images: null};
const SWITCH_MS = 15000, ANNOUNCE_MS = 5000;
// The Android app (android/) provides navigator.serial over the USB cable: no COM port to choose.
const ANDROID_USB = navigator.serial?.androidUsb === true, PORT_NAME = ANDROID_USB ? 'USBケーブル' : 'COM7';
const model = new DemoState();
let assets, settings, playback, connection, store, picker, connected = false, opening = false, lastReceived = 0, stalled = false;
let lastImage = '', raf = 0, imageFailed = false, switchUntil = 0, announceUntil = 0, listPending = false, switchAnnounced = false;
const current = () => assets?.objects[model.object] ?? FALLBACK;
const labelOf = object => assets?.objects[object]?.label ?? object;
// Plain notices stay hidden (as before) and never hide a fresh announcement; errors always show.
const notice = (text,error=false) => {
  if (!error && performance.now() < announceUntil) return;
  announceUntil = 0; switchAnnounced = false; $('notice').textContent = error ? text : ''; $('notice').dataset.error = error; $('notice').hidden = !error;
};
const announce = (text,ms=ANNOUNCE_MS) => {
  announceUntil = performance.now()+ms; switchAnnounced = false; $('notice').textContent = text; $('notice').dataset.error = false; $('notice').hidden = false;
};
// A slot switch or a PST1 sequence pauses the samples for a few seconds: not a stall.
const switching = () => store?.busy || performance.now() < switchUntil;
const pauseStall = () => { switchUntil = performance.now()+SWITCH_MS; };
// Only this announcement is cleared by the next `# PNEM3`; a listing failure notice stays its 8 s.
const announceSwitch = slot => { pauseStall(); announce(`スロット ${slot} に切り替え中…（学習しています）`,SWITCH_MS); switchAnnounced = true; };
function controls() {
  const label = current().label;
  $('connect').disabled = opening || connected || !connection || !('serial' in navigator);
  $('connect').hidden = connected; $('disconnect').hidden = !connected;
  $('connect').textContent = `${label}につなぐ ↗`;
  $('disconnect').disabled = !connected;
  $('connection').dataset.state = stalled ? 'error' : connected ? 'on' : 'off';
  $('connection').textContent = opening ? '接続しています' : stalled ? '受信待ち' : connected ? `${label}とつながっています` : '未接続';
  $('mode-badge').textContent = `${label}モード${model.slot === null ? '' : ` · スロット ${model.slot} · ${model.count} 件`}`;
  $('mode-badge').dataset.object = model.object;
  picker?.sync();
}
function renderImage(phase, part) {
  const {images, label, names} = current();
  $('status-stage').toggleAttribute('data-noimage', !images);
  if (!images) { lastImage = ''; $('status-image').hidden = true; $('image-error').hidden = true; return; }
  const src = images[phase === 'pressed' ? 'pressed' : phase === 'result' ? part : 'idle'];
  if (src && lastImage !== src) {
    lastImage = src; imageFailed = false; $('image-error').hidden = true;
    $('status-image').hidden = false; $('status-image').src = src;
  }
  $('status-image').alt = phase === 'pressed' ? `押下を検出し、全体が黄色になった${label}` :
    phase === 'result' ? `${names[part]}が赤くなった${label}` : `待機中の${label}`;
}
function render() {
  const phase = model.phase, part = model.part, names = current().names;
  renderImage(phase, part);
  $('status-stage').dataset.phase = phase; $('status-stage').dataset.part = phase === 'result' ? part : '';
  $('phase-badge').textContent = phase === 'pressed' ? '押している' : phase === 'result' ? 'ここに触れた！' : phase === 'unknown' ? '判定できませんでした' : '待機中';
  $('part-ja').textContent = phase === 'pressed' ? 'ぎゅっ。離してみよう' : phase === 'result' ? `${names[part]}にタッチ！` : phase === 'unknown' ? 'もう一度さわってみよう' : 'どこをさわる？';
  $('reset-count').textContent = ['result','unknown'].includes(phase) ? `${Math.max(0,(model.deadline-performance.now())/1000).toFixed(1)} s` : '';
  const trace = model.trace;
  $('waveform-card').hidden = !trace || !model.traceDeadline;
  $('chart-empty').hidden = Boolean(trace?.points.length);
  $('trace-label').textContent = trace ? `${names[trace.part] ?? '判定なし'} · ${trace.part}` : 'タッチを待っています';
  $('trace-detail').textContent = trace ? `${trace.points.length} samples · ${trace.base === null ? '生値（基準区間なし）' : '直前の基準値からの変化'}${trace.partial ? ' · 接続直後のため一部のみ' : ''}` : '実際に受信したデータを表示します';
  if (!$('waveform-card').hidden) drawTrace($('chart'),trace);
}
function scheduleRender() { if (!raf) raf = requestAnimationFrame(() => { raf=0; render(); }); }
// `# PNEM3` after every (re)training: a new object swaps videos, names and pictures.
function applyObject(effects) {
  const wasSwitching = performance.now() < switchUntil;
  if (wasSwitching) { switchUntil = 0; lastReceived = performance.now(); }
  picker?.markActive(model.slot, model.count);
  if (effects.includes('object')) {
    playback.setObject(current().videos); lastImage = '';
    announce(`${current().label}モードに切り替わりました`);
  } else if (switchAnnounced) { announceUntil = 0; notice(''); }
  controls();
}
function receiveSample() {
  lastReceived = performance.now();
  if (stalled) { stalled=false; controls(); notice('つながりました。好きなところを押してみよう。'); }
  if (listPending) { listPending = false; picker.list(); }
}
function receive(line) {
  const p = parseDemoLine(line);
  if (!p) return;
  store.feed(p);
  if (p.type === 'sample') receiveSample();
  if (p.type === 'slot-switch') announceSwitch(p.slot);
  if (p.type === 'slot-error') notice(`スロットを切り替えられませんでした（${p.why}）`,true);
  const previousTrace = model.trace;
  const effects = model.consume(p,performance.now());
  if (p.type === 'object') applyObject(effects);
  if (effects.includes('reset')) { playback.showIdle(); if (!switching()) notice('圧力データの続き方が変わりました。次のタッチを待っています。'); }
  if (effects.includes('press')) notice('タッチを感じました。手を離すと場所がわかります。');
  if (effects.includes('result')) {
    if (model.phase === 'result') { playback.play(model.part); notice(''); }
    else notice('場所を判定できませんでした。もう一度、約1秒押して離してみよう。');
  }
  if (effects.includes('rejected')) notice('波形を読み取れませんでした。もう一度押して離してみよう。');
  if (p.type !== 'sample' || previousTrace !== model.trace) scheduleRender();
}
async function connect() {
  if (opening || connected) return;
  opening=true; controls();
  try {
    // The browser chooser requires a user click and displays the actual COM port names.
    const port = await navigator.serial.requestPort();
    model.reset(); playback.showIdle(); lastReceived=performance.now(); stalled=false; picker.reset();
    await connection.start(port);
    connected=Boolean(connection.port); listPending=connected; picker.setConnected(connected);
    notice(`${PORT_NAME}の圧力データを待っています。${current().label}を押してみよう。`);
  } catch (error) {
    connected=false;
    notice(error.name === 'NotFoundError' ? '接続をキャンセルしました。' : `接続できませんでした。${ANDROID_USB ? 'USBケーブルを確認してください。' : 'COM7を使っているアプリを確認してください。'}(${error.message})`,error.name !== 'NotFoundError');
  } finally { opening=false; controls(); scheduleRender(); }
}
function closed() {
  connected=false; stalled=false; listPending=false; switchUntil=0;
  store.cancel('CLOSED'); picker.setConnected(false); model.reset(); playback.showIdle(); controls(); scheduleRender();
  notice('接続が終了しました。再接続すると続けられます。',false);
}
$('connect').onclick=connect;
$('disconnect').onclick=async () => { $('disconnect').disabled=true; await connection.stop(); notice('切断しました。いつでも再接続できます。'); };
$('sound').onclick=() => {
  const on=$('sound').getAttribute('aria-pressed') !== 'true';
  $('sound').setAttribute('aria-pressed',String(on)); $('sound').textContent=on?'音 ON':'音 OFF';
  $('sound').setAttribute('aria-label',on?'動画の音をオフにする':'動画の音をオンにする'); playback.setMuted(!on);
};
$('play-video').onclick=() => { $('play-video').hidden=true; playback.showIdle(); };
// Fullscreen the page so status and chart stay above both video elements.
function fullscreenControls() {
  const active = Boolean(document.fullscreenElement);
  $('fullscreen').textContent = active ? '⛶ 全画面を終了' : '⛶ 全画面';
  $('fullscreen').setAttribute('aria-pressed',String(active));
  $('fullscreen').setAttribute('aria-label',active ? '全画面を終了する' : 'ステータスと波形を含めて全画面にする');
  scheduleRender();
}
$('fullscreen').disabled = !document.fullscreenEnabled;
$('fullscreen').onclick = async () => {
  try {
    if (document.fullscreenElement) await document.exitFullscreen();
    else await document.documentElement.requestFullscreen();
  } catch { notice('全画面にできませんでした。ブラウザーのF11キーでも表示できます。',true); }
};
document.addEventListener('fullscreenchange',fullscreenControls);
$('status-image').onerror=() => {
  if (!imageFailed) { imageFailed=true; $('status-image').hidden=true; $('image-error').hidden=false; }
};
window.addEventListener('resize',scheduleRender);
window.addEventListener('pagehide',() => { connection?.stop(); });
function tick() {
  const now = performance.now();
  if (model.tick(now)) scheduleRender();
  if (model.phase==='result' || model.phase==='unknown') $('reset-count').textContent=`${Math.max(0,(model.deadline-now)/1000).toFixed(1)} s`;
  picker.sync();
  if (connected && !stalled && !switching() && now-lastReceived>settings.noDataMs) {
    stalled=true; model.reset(); playback.showIdle(); controls(); scheduleRender();
    notice(`圧力データが届いていません。${PORT_NAME}と実機のRun状態を確認してください。`,true);
  }
}
function setup() {
  for (const object of Object.values(assets.objects))
    for (const src of Object.values(object.images ?? {})) { const preload = new Image(); preload.src = src; }
  playback=new VideoPlayback($('idle-video'),$('reaction-video'),current().videos,{
    onChange:(part,loading=false) => { $('video-label').textContent=part==='static'?`いつもの${current().label}`:`${current().names[part]}のリアクション${loading?' · 準備中':''}`; },
    // The play button is only for a browser autoplay block; other failures just show the notice.
    onError:(message,{needsGesture=false}={}) => { notice(message,true); if (needsGesture) $('play-video').hidden=false; },
  });
  $('idle-video').addEventListener('playing',() => { $('play-video').hidden=true; });
  // Android WebView pauses media behind system dialogs (USB permission) and other apps; restart the idle loop.
  document.addEventListener('visibilitychange',() => {
    if (document.visibilityState==='visible' && playback.part==='static' && $('idle-video').paused) playback.showIdle();
  });
  playback.setMuted(true); playback.showIdle();
  connection=new SerialConnection({...settings,maxLine:256},{onLine:receive, onError:message => notice(message,true), onClose:closed});
  store=new StoreClient({write:(text,options) => connection.write(text,options)});
  picker=new SlotPicker({select:$('slot-select'), refresh:$('slots-refresh'), status:$('slot-status'), store, labelOf,
    onPause:pauseStall, onSwitching:announceSwitch, onError:message => notice(message,true), onNotice:announce});
}
try {
  const [configResponse,assetResponse]=await Promise.all([fetch('/config.json'),fetch('assets.json')]);
  if (!configResponse.ok || !assetResponse.ok) throw new Error('設定を読み込めません');
  settings=await configResponse.json(); assets=validateAssets(await assetResponse.json());
  setup();
  notice(ANDROID_USB ? `USBケーブルで${current().label}をつなぎ、「${current().label}につなぐ」を押してください。` :
    'serial' in navigator ? `「${current().label}につなぐ」からUSB SerialのCOM7を選んでください。` : 'USB接続にはChromeまたはEdgeで開いてください。');
  setInterval(tick,100);
} catch (error) { notice(`デモを開けませんでした。起動用サーバーを確認してください。(${error.message})`,true); }
controls(); render();
