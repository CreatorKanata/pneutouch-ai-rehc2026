<!-- Phase 0 operation: build on Windows, display on PC, and distinguish tests from hardware evidence. -->
# Phase 0：HX710Bの圧力値をPCで表示する

作成日: 2026-09-20。MPS20N0040D＋HX710Bの4ピンモジュールを使用。
ファームウェア、LEXIDE設定、PC波形画面、CSV収集を実装した。
**Windowsでの最終ビルド・実機書き込み・圧力測定は未確認。**
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

## 2. 依存物を準備する（Mac）

リポジトリ直下で実行する。Python 3.10以降、テストにはCコンパイラーとNode.js 20以降を使う。
PC画面の通常起動にはPython標準ライブラリーのみでよく、Node.jsは不要。

```sh
python3 tools/prepare_vendor.py
python3 tools/test.py
python3 tools/check_firmware.py
python3 tools/package_firmware.py
```

`check_firmware.py`はMacのClang等によるArmオブジェクトの検査で、HEXを作る処理ではない。
`test.py`はC/Python/JavaScriptのホストテスト。pySerialがない場合のみ疑似端末試験をスキップする。
各ソースの役割は[ファームウェアREADME](../src/pneutouch-solist-ai/README.md)を参照。

準備元は`config.py`の`VENDOR_PROJECT`に固定したDT用AIVibrationInferenceパッケージと
`ARM.CMSIS.5.9.0.pack`。大量のRB用サンプルから任意の`.cproject`を選ばない。
別の場所の資料を使う場合は`--references /path/to/references`を指定する。
不足ファイルはエラーで示し、元の`references/`は編集しない。

ベンダーコードは著作権表示を残して`src/pneutouch-solist-ai/vendor/`へ展開する。
設定ヘッダー・リンカー・出典ハッシュは同じプロジェクトの`generated/`に保存する。
両ディレクトリともGit管理外。設定変更後はprepareを再実行する。
Phase 0ではAIライブラリーをリンクせず、後続の新Solist-AI SDK選定と分ける。

## 3. WindowsでLEXIDEプロジェクトをビルドする

1. `build/PneuTouchSolistAI.zip`をWindowsへコピーして展開する。
   例: `C:\PneuTouch\PneuTouchSolistAI`。このZIPはソース一式で、書き込み済みHEXではない。
2. LEXIDE-Ω、ROHM ML63Q25x7デバイスパック、MCU-LINKドライバーを確認する。
   配布ガイドの基準は`ROHM.ML63Q25x7_DFP_1.0.1.pack`とCMSIS 5.9.0。
3. `File > Import > General > Existing Projects into Workspace`から展開先を選ぶ。
   `.project`、`.cproject`、`.settings/`、`vendor/`、`generated/`を含める。
   プロジェクト直下の`ML63Q25x7_lccarm.ld`も必要。生成したリンカースクリプトを読む入口になる。
4. `PneuTouchSolistAI`を選び、MCUがML63Q25x7 / ML63Q2557であることを確認。
   DebugでClean / Buildする。Releaseを使う場合もその構成でClean / Buildする。
5. Consoleのエラー、HEX、map、ROM/RAM量を確認する。
   古いHEXの更新時刻を取り違えず、ビルドログと生成物を保存する。
6. プロジェクト用のLAPIS GDB Debugging (Arm)設定を作り、実際のプローブCMSIS-DAPを指定。
   既存デモの書込設定ではなく今回の成果物を選び、配布ガイドに従って書き込む。
7. Run/Resumeする。デバッガーで停止したままでは測定値は送られない。

初回はWindowsローカルの短いASCIIパスを使う。共有フォルダーのUNCパスや同期の影響を避けられる。
元のデモHEXは復元用に保持する。AISignal→AIVibrationの専用再構成HEXを通常のアプリHEXと混同しない。
コード修正後はMac側で再度prepare/packageし、Windows側の編集差分を確認してコピー・Refreshする。

## 4. PCの波形画面

リポジトリ直下で実行する。

```sh
python3 tools/serve.py
```

Chrome / Edgeで[計測画面](http://localhost:8000/learning-tool/)を開く。
サーバーは127.0.0.1のみで待ち受ける。PC外への公開・データ送信は行わない。

1. 「センサーに接続」でFT2232HのUART Bチャネルを選ぶ。
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

WindowsではPython起動を`py`、ポートを`COM5`等へ読み替える。
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

実機で残る確認: モジュール電源・配線、書き込み、約40 SPSの実受信、押下/解放時の変化、
SCK幅、ノイズ・飽和、センサー未接続時のエラー、再接続後の回復。
これらを満たし、実測CSVを保存できた時点で[実装プランM1](implementation-plan.md)の完了とする。

## 参照

- [ハードウェア資料と配線の根拠](hardware.md)、[開発環境](development-workflow.md)。
- [AVIA HX710データシート](https://bafnadevices.com/wp-content/uploads/2024/04/HX710B-SMD-Datasheet.pdf)。
- [Web Serial仕様](https://wicg.github.io/serial/)、[pySerial公式API](https://pyserial.readthedocs.io/en/latest/pyserial_api.html)。
  実装前にContext7経由でも通信・切断APIを確認した。
