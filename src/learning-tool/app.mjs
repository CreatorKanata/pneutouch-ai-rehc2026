// Phase 0 UI: real raw samples, explicit labels, bounded recording and CSV export.
import {parseLine, csv} from '../shared/protocol.mjs';
import {SerialConnection} from '../shared/serial.mjs';
import {Measurement} from '../shared/measurement.mjs';
import {drawChart} from './chart.mjs';

const $ = id => document.getElementById(id);
let settings, connection, model, connected = false, opening = false, lastReceived = 0;
const status = message => { $('status').textContent = message; };
const draw = () => { $('range').textContent = drawChart($('chart'), model?.points ?? []); };

function controls() {
  $('connect').disabled = connected || opening || !settings || !('serial' in navigator);
  $('disconnect').disabled = !connected;
  $('record').disabled = !connected || model?.recording || model?.rows.length >= settings?.recordingSamples;
  $('record').textContent = model?.rows.length ? '記録を再開' : '記録開始';
  $('stop').disabled = !model?.recording;
  $('download').disabled = !model?.rows.length || model?.recording;
  $('clear').disabled = !model?.rows.length || model?.recording || model?.dirty;
  $('connection').textContent = opening ? '接続準備中' : connected ? '接続中' : '未接続';
  $('count').textContent = `${model?.recording ? '● 記録中 · ' : ''}${(model?.rows.length ?? 0).toLocaleString()} サンプル`;
  $('issues').textContent = `欠番等 ${model?.gaps ?? 0} · 再起動推定 ${model?.resets ?? 0} · 不正行 ${model?.invalid ?? 0}`;
}

function receive(line) {
  if (line.startsWith('#')) {
    if (line.startsWith('# PneuTouch')) model.resetTrace();
    status(line.trim()); return;
  }
  const sample = parseLine(line);
  if (!sample) { model.invalid++; status('不正な受信行を除外しました。通信速度を確認してください。'); controls(); return; }
  const wasRecording = model.recording;
  const event = model.add(sample, $('label').value, new Date().toISOString());
  lastReceived = performance.now();
  $('raw').textContent = sample.raw.toLocaleString();
  $('rate').textContent = model.rate === null ? '受信中' : `${model.rate.toFixed(1)} サンプル/秒`;
  status(event.gap ? '欠番または再起動を検出しました。波形の線を区切っています。' : '受信中。押したときと離したときの波形を確認できます。');
  if (sample.raw === -8388608 || sample.raw === 8388607) status('ADCが飽和しています。配線・電源・加圧量を確認してください。');
  if (wasRecording && !model.recording) status('記録上限に達しました。CSVを保存してから次の記録を始めてください。');
  controls(); draw();
}

$('connect').onclick = async () => {
  opening = true; controls();
  try {
    const port = await navigator.serial.requestPort();
    model.resetTrace(); connected = true; lastReceived = performance.now();
    $('raw').textContent = '—'; $('rate').textContent = '受信待ち'; draw();
    await connection.start(port);
    if (connection.port) status('接続しました。センサーのデータを待っています。');
  } catch (error) { connected = false; status(`接続できませんでした: ${error.message}`); }
  finally { opening = false; controls(); }
};
$('disconnect').onclick = async () => {
  $('disconnect').disabled = true;
  try { await connection.stop(); status('切断しました。記録済みデータはCSVに保存できます。'); }
  catch (error) { status(`切断エラー: ${error.message}`); }
};
$('record').onclick = () => { model.start(); controls(); };
$('stop').onclick = () => { model.stop(); controls(); };
$('clear').onclick = () => { model.rows = []; controls(); };
$('download').onclick = () => {
  const url = URL.createObjectURL(new Blob([csv(model.rows)], {type: 'text/csv;charset=utf-8'}));
  const link = document.createElement('a'); link.href = url;
  link.download = `pneutouch-${new Date().toISOString().replaceAll(':', '-')}.csv`;
  link.click(); setTimeout(() => URL.revokeObjectURL(url), 1000);
  model.dirty = false; controls();
};
window.addEventListener('beforeunload', event => {
  if (model?.dirty || model?.recording) { event.preventDefault(); event.returnValue = ''; }
});
window.addEventListener('resize', draw);
try {
  const response = await fetch('/config.json');
  if (!response.ok) throw new Error('config.json');
  settings = await response.json(); model = new Measurement(settings);
  connection = new SerialConnection(settings, {
    onLine: receive,
    onError: message => { model.invalid++; status(message); controls(); },
    onClose: () => { connected = false; model.stop(); $('rate').textContent = '受信停止'; controls(); },
  });
  $('port-details').textContent = `CN9 · FT2232H B · ${settings.baudRate} baud`;
  $('nominal').textContent = `公称${settings.nominalSps}サンプル/秒`;
  $('limit').textContent = `1回の記録上限は${settings.recordingSamples.toLocaleString()}点です。保存後に新しい記録を始められます。`;
  status('serial' in navigator ? 'センサーに接続し、FT2232HのUART Bチャネルを選んでください。' :
    'このブラウザはWeb Serial非対応です。Mac / WindowsのChromeまたはEdgeで開いてください。');
  setInterval(() => {
    if (connected && performance.now() - lastReceived > settings.noDataMs) {
      $('rate').textContent = '受信停止';
      status('データが届いていません。CN9、USB割り当て、Run状態、センサー配線を確認してください。');
    }
  }, settings.noDataMs);
} catch (error) { status(`起動できません。python3 tools/serve.py で起動してください。(${error.message})`); }
controls(); draw();
