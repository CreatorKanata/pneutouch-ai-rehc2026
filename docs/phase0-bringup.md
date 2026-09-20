<!-- Phase 0 operation: build on Windows, display on PC, and distinguish tests from hardware evidence. -->
# Phase 0：HX710Bの圧力値をPCで表示する

作成日: 2026-09-20。MPS20N0040D＋HX710Bの4ピンモジュールを使用。
メインは `src/pneutouch-solist` の **PneutouchAi**。
Windows/LEXIDEのDebugビルド（エラー・警告0）、PneutouchAi Writeによる書き込み、
COM7から315サンプル／実効39.72 SPS・欠番0の受信を確認済み。
現在のポート対応は[実機の接続記録](local-hardware.md)に保存している。
単位は符号付き24 bitのADC counts。kPaへの換算、AI学習・部位判定は後続。

## 1. 配線とUSB

配線は電源を切って行い、[ハードウェア仕様](hardware.md)の向き・電圧を確認する。
標準MEMSボードがCN3に付いている場合は外す。4ピンの並びは製品の印字に従う。

| HX710Bモジュール | DT-EBML63Q2557 |
|---|---|
| VCC | CN3-5（8も同じ電源）、3.3V |
| GND | CN3-11 |
| SCK / CLK | CN3-12、P40 |
| DOUT / OUT / DAT | CN3-10、P42 |

- JP1は1–2を短絡して3.3Vを選ぶ。モジュールへの給電前に電圧を測る。
- 開発中はJP8を短絡。CN2のMCU-LINKはParallelsのWindowsへ接続する。
- CN9のFT2232Hは計測するOSへ接続する。Macで表示するならMacへ割り当てる。
- 通信はFT2232HのUART Bチャネル。CN8は給電専用で、データ通信には使えない。
- モジュールの3.3V動作・DOUTの電圧を確認する。CN3へ5V信号を入れない。

MCU-LINKとCN9は別のUSB機器としてOSを選べる。同じシリアルポートを
ブラウザー、CLI、別のシリアルモニターで同時に開かない。

## 2. プロジェクトと設定

現在のメインは `src/pneutouch-solist`。プロジェクト名は **PneutouchAi**。
旧 `src/pneutouch-solist-ai` は参考用で、現在のビルド・書き込みには使わない。
必要なROHMコード、AIライブラリー、LEXIDE設定は新プロジェクト内にある。
LEXIDE環境にはARM CMSIS 5.9.0とROHM ML63Q25x7デバイスパックが必要。
このWindows環境のROHMパックは1.1.0、LAPISビルドツールはVer.20260317。

計測設定の原本は `config.py`。変更したときのみ、以下でヘッダーを更新してビルドする。
コミット済みヘッダーがあるので、初回ビルドのためのPython実行は不要。

```sh
python tools/generate_config.py
python tools/generate_config.py --check
```

`tools/prepare_vendor.py` は旧プロジェクト専用。現在のリンカーやDebug設定を上書きしない。
別PCへ移すときは `python tools/package_firmware.py` で `build/PneutouchAi.zip` を作る。
launchにはこのWindows環境の絶対パスが残るので、別PCではパック/TCLの場所を合わせる。

## 3. Windowsでビルド・書き込み

1. LEXIDEの **PneutouchAi (in pneutouch-solist)** を選ぶ。
   新規環境では `File > Import > General > Existing Projects into Workspace` から
   `src/pneutouch-solist` を取り込む。
2. ビルド構成を **Debug** にする。ソース変更後はRefreshし、Buildを実行。
3. Consoleのエラー・警告と、`Debug/PneutouchAi.elf` / `.hex` の更新時刻を確認。
4. Debugの起動履歴から **PneutouchAi Write** を選び、書き込みと実行を行う。
   **WriteもDebugとして起動する。** 通常のRunやLocal C/C++ Applicationを選ばない。
5. ソースレベルの確認には **PneutouchAi Debug** を選ぶ。
   `main` で停止したらRun/Resumeして取得を開始する。

どちらも同じ `Debug/PneutouchAi.elf` を使う。Releaseの実機確認は今回の対象外。
元の動作確認済みAIVibrationInferenceベースはコミット `95a453e` に保存済み。
現在のアプリはHX710B取得用で、元デモのLCD表示・AI推論・独自UART通信は起動しない。

デバッガーでは `pneutouch_status` を確認できる。
`state` はSTARTING / SETTLING / STREAMING / SENSOR_TIMEOUT、
`raw` は最新ADC生値、`samples` は取得数、`milliseconds` は最新取得時刻、`timeouts` は累計。
`samples=0` のrawは未取得値。タイムアウト後のrawは前回値なので、stateと一緒に見る。
SCK High中にブレークするとHX710Bがパワーダウンし得るため、通常は走らせたままUARTで観測する。

## 4. PCの波形画面

リポジトリ直下で実行する。

```sh
python3 tools/serve.py
```

Chrome / Edgeで[計測画面](http://localhost:8000/learning-tool/)を開く。
サーバーは127.0.0.1のみで待ち受ける。PC外への公開・データ送信は行わない。

1. 「センサーに接続」でFT2232HのUART Bチャネルを選ぶ。現在のWindowsでは **COM7**（COM6はA側）。
2. 生値、MCU時刻による波形、受信サンプル/秒を確認する。
3. 必要なら手動ラベルを選び「記録開始」。未指定は空欄、0は非接触、1～7は物理区画。
4. 「記録停止」→「CSVを保存」。ダウンロードできたことを確認してから記録をクリアする。
5. 終了時は「切断」。記録停止やCSV保存だけではシリアル接続は閉じない。

グラフは直近400点、記録上限は24,000点が既定値。設定は`config.py`に集約し、
変更後はサーバー再起動と画面再読み込みが必要。未保存データは先にCSVへ保存する。
グラフの縦軸は自動調整で、波形や保存値へ平滑化・自動ゼロ補正はかけない。
記録上限では自動停止する。CSV出力前は記録のクリアを無効にする。
ラベルは受信時点の手動指定なので、USBバッファ分の時間差は後続のイベント収集で扱う。

欠番・再起動推定では線を区切り、欠けた値を0で補わない。
不正行、長すぎる行、未受信、ADC端の値への飽和を表示する。
ブラウザーにWeb Serialがない場合はChrome / Edgeまたは次のCLIを使う。

## 5. CLIで受信・CSV保存する場合

ブラウザーとは別に動作確認できる。Macで以下を実行する。

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
python src/learning-tool/capture.py --list
python src/learning-tool/capture.py --port /dev/cu.usbserial-実際の名前 --output captures/pressure-001.csv --duration 60
```

WindowsではPython起動を`py`、ポートを **COM7** へ読み替える。この環境では次のように実行できる。

```powershell
.venv\Scripts\python.exe src/learning-tool/capture.py --port COM7 --output captures/pressure-001.csv --duration 30
```

`.venv` はリポジトリ直下に作成済みで、pySerial 3.5を導入済み。
ラベルを固定するなら`--label 7`等を指定する。省略は未指定、`--label 0`は非接触。
出力CSVは既存ファイルへ上書きしない。受信できなかった場合は終了コード2で知らせる。
CLIをCtrl+Cで止めるとCSVを閉じる。通信切断時も既に書き込んだ行を保持する。

CSV列はブラウザー・CLI共通で`host_utc,seq,mcu_ms,raw,label`。
PC受信時刻とMCU取得時刻を分けて保存し、実効サンプル周期にはMCU側の時刻を使う。

## 実装と検証結果

- HX710BのMSB順24 bit、符号境界、25 / 27パルスをCホストテストで確認。
  26パルスを圧力値として扱わず、転送後にDOUTがHighへ戻らない場合もサンプルを返さない。
- 初回のモード設定用転送を破棄し、500ms安定待ち。1秒以上読めなければ
  `# ERROR,HX710B_TIMEOUT`を送る。復帰時の再安定待ちと時刻周回をテスト。
- SCK High中だけ割り込みを禁止し、Lowへ戻してから復元。待ち時間は各2µs＋命令時間。
  実際のHigh幅は実機で50µs以下を確認する。ここでデバッガーを停止しない。
- Python/JavaScriptで分割行、数値範囲、長い行からの復帰、CSV、上書き防止、
  欠番・再起動・周回、記録上限、シリアルのロック解除と再接続を検証。
- Macの疑似端末をpySerialで開き、分割送信した生値をCSVへ保存する統合テストも通過。
  実センサーを接続した試験ではない。Cテスト2件、Python 10件、JavaScript 7件を実行済み。
- Debug / Releaseのリンカー入口が生成側のメモリー配置へ解決されることをテスト。
- Mac Clangで9個のCソースをArm向けオブジェクトへコンパイル済み。
  Windowsの最終リンク・HEX生成の成功とは区別する。

2026-09-20、新プロジェクトへ移植し、Windows Debugビルド（エラー・警告0）、書き込み、
COM7から約40 SPSの連続受信を確認。最初の記録は
`captures/pressure-bringup-20260920-2.csv`（315点、MCU時刻7906ms、欠番0）。
Pythonは10件成功・POSIX専用1件スキップ、JavaScriptは7件成功。
追加したCアプリ境界テストはWindows用ホストCコンパイラー未導入のため今回未実行。
上のMacテスト結果は旧プロジェクト時点の履歴である。

追加の30秒記録では1190点、39.72 SPS、欠番0。ユーザーが繰り返し押している間に
2,388,304～7,647,159 countsの変化を確認した。個々の部位・押下時刻は未記録。
10件の新プロジェクト計測・起動ソースをArm向けオブジェクトへコンパイルする検査も成功。

実機で残る確認: 部位・押下時刻との対応と気密の評価、
SCK幅、ノイズ・飽和、センサー未接続時のエラー、再接続後の回復。
これらを満たし、実測CSVを保存できた時点で[実装プランM1](implementation-plan.md)の完了とする。

## 参照

- [ハードウェア資料と配線の根拠](hardware.md)、[開発環境](development-workflow.md)。
- [AVIA HX710データシート](https://bafnadevices.com/wp-content/uploads/2024/04/HX710B-SMD-Datasheet.pdf)。
- [Web Serial仕様](https://wicg.github.io/serial/)、[pySerial公式API](https://pyserial.readthedocs.io/en/latest/pyserial_api.html)。
  実装前にContext7経由でも通信・切断APIを確認した。
