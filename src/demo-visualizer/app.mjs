// PneutouchAI — HTML / Web Serial demo. Inference is performed by the physical SolistAI.
import {SerialConnection} from '../shared/serial.mjs';
import {parseDemoLine, PARTS} from './protocol.mjs';
import {DemoState} from './state.mjs';
import {VideoPlayback} from './media.mjs';
import {drawTrace} from './chart.mjs';
const $ = id => document.getElementById(id);
const names = {HEAD:'あたま',BACK:'せなか',LEGS:'あし',TAIL:'しっぽ'};
const model = new DemoState();
let assets, playback, connection, connected = false, opening = false, lastReceived = 0, stalled = false;
let lastImage = '', raf = 0, imageFailed = false;
const notice = (text,error=false) => { $('notice').textContent = text; $('notice').dataset.error = error; };
function controls() {
  $('connect').disabled = opening || connected || !connection || !('serial' in navigator);
  $('connect').hidden = connected; $('disconnect').hidden = !connected;
  $('disconnect').disabled = !connected;
  $('connection').dataset.state = stalled ? 'error' : connected ? 'on' : 'off';
  $('connection').textContent = opening ? '接続しています' : stalled ? '受信待ち' : connected ? '恐竜とつながっています' : '未接続';
}
function render() {
  const phase = model.phase, part = model.part;
  const key = phase === 'pressed' ? 'pressed' : phase === 'result' ? part : 'idle';
  const src = assets?.images[key];
  if (src && lastImage !== src) {
    lastImage = src; imageFailed = false; $('image-error').hidden = true;
    $('status-image').hidden = false; $('status-image').src = src;
  }
  $('status-image').alt = phase === 'pressed' ? '押下を検出し、全体が黄色になった恐竜' :
    phase === 'result' ? `${names[part]}が赤くなった恐竜` : '待機中の恐竜のサイドビュー';
  $('status-stage').dataset.phase = phase;
  $('phase-badge').textContent = phase === 'pressed' ? '押している' : phase === 'result' ? 'ここに触れた！' : phase === 'unknown' ? '判定できませんでした' : '待機中';
  $('part-en').textContent = phase === 'pressed' ? 'FEELING YOUR TOUCH' : phase === 'result' ? part : phase === 'unknown' ? 'TRY AGAIN' : 'READY TO PLAY';
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
function receive(line) {
  const p = parseDemoLine(line);
  if (!p) return;
  if (p.type === 'sample') {
    lastReceived = performance.now();
    if (stalled) { stalled=false; controls(); notice('つながりました。好きなところを押してみよう。'); }
  }
  const previousTrace = model.trace;
  const effects = model.consume(p,performance.now());
  if (effects.includes('reset')) { playback.showIdle(); notice('圧力データの続き方が変わりました。次のタッチを待っています。'); }
  if (effects.includes('press')) notice('タッチを感じました。手を離すと場所がわかります。');
  if (effects.includes('result')) {
    if (PARTS.includes(p.part)) { playback.play(p.part); notice(`${names[p.part]}にタッチ！ 次の場所もさわってみよう。`); }
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
    model.reset(); playback.showIdle(); lastReceived=performance.now(); stalled=false;
    await connection.start(port);
    connected=Boolean(connection.port); notice('COM7の圧力データを待っています。恐竜を押してみよう。');
  } catch (error) {
    connected=false;
    notice(error.name === 'NotFoundError' ? '接続をキャンセルしました。' : `接続できませんでした。COM7を使っているアプリを確認してください。(${error.message})`,error.name !== 'NotFoundError');
  } finally { opening=false; controls(); scheduleRender(); }
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
try {
  const [configResponse,assetResponse]=await Promise.all([fetch('/config.json'),fetch('assets.json')]);
  if (!configResponse.ok || !assetResponse.ok) throw new Error('設定を読み込めません');
  const settings=await configResponse.json(); assets=await assetResponse.json();
  for (const key of ['idle','pressed',...PARTS]) if (typeof assets.images?.[key] !== 'string') throw new Error(`状態画像 ${key} の設定がありません`);
  for (const key of ['static',...PARTS.map(p=>p.toLowerCase())]) if (typeof assets.videos?.[key] !== 'string') throw new Error(`動画 ${key} の設定がありません`);
  for (const src of Object.values(assets.images)) { const preload = new Image(); preload.src = src; }
  playback=new VideoPlayback($('idle-video'),$('reaction-video'),assets.videos,{
    onChange:(part,loading=false) => { $('video-label').textContent=part==='static'?'いつもの恐竜':`${names[part]}のリアクション${loading?' · 準備中':''}`; },
    onError:message => { notice(message,true); $('play-video').hidden=false; },
  });
  playback.setMuted(true); playback.showIdle();
  connection=new SerialConnection({...settings,maxLine:256},{
    onLine:receive,
    onError:message => notice(message,true),
    onClose:() => { connected=false; stalled=false; model.reset(); playback.showIdle(); controls(); scheduleRender(); notice('接続が終了しました。再接続すると続けられます。'); },
  });
  notice('serial' in navigator ? '「恐竜につなぐ」からUSB SerialのCOM7を選んでください。' : 'USB接続にはChromeまたはEdgeで開いてください。');
  setInterval(() => {
    if (model.tick(performance.now())) scheduleRender();
    if (model.phase==='result' || model.phase==='unknown') $('reset-count').textContent=`${Math.max(0,(model.deadline-performance.now())/1000).toFixed(1)} s`;
    if (connected && !stalled && performance.now()-lastReceived>settings.noDataMs) {
      stalled=true; model.reset(); playback.showIdle(); controls(); scheduleRender();
      notice('圧力データが届いていません。COM7と実機のRun状態を確認してください。',true);
    }
  },100);
} catch (error) { notice(`デモを開けませんでした。起動用サーバーを確認してください。(${error.message})`,true); }
controls(); render();
