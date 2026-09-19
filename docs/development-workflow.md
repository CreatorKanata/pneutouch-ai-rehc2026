<!-- Development workflow: separate Mac-side checks from Windows builds and hardware evidence. -->
# Mac / Windowsでの開発方法

## 役割分担

**Macでも開発できる。** 編集、Git、PCツール、データ解析、ハード非依存テストをMacで行い、
公式環境を使ったファームウェアのビルド・書き込み・デバッグはWindowsのLEXIDE-Ωで行う。
調査時のMacはIntel（x86_64）。Apple SiliconでのWindows/USB互換性を確認した結果ではない。

| 作業 | 使用環境 | 確認状況 |
|---|---|---|
| C / Python / JavaScriptの編集、Git | Mac | 利用可能 |
| Cホストテスト、通信処理のテスト | Mac | 作業用試作で実施。採用コードで再確認する |
| Arm向けCオブジェクト生成 | MacのClang | 試作と必要な配布コードで確認 |
| 最終リンク、HEX生成 | Windows / LEXIDE-Ω | 新規プロジェクトでは未実施 |
| SWD書き込み・停止・Run | Windows / MCU-LINK | 今回のファームウェアでは未実施 |
| 圧力波形・CSV・動画表示 | Macを第一候補 | 実機UARTでは未確認 |

MacのClangでCをコンパイルできても、Solist-AIライブラリーとの最終リンク、
メモリー配置、Option領域、HEX生成、デバッガーの動作が保証されるわけではない。
Mac単独のビルド環境作成は後続の選択肢とし、まず配布資料が想定する環境で基準を作る。

## LEXIDE-Ωの準備と取り込み方針

参照: [資料D1 / D2 / D3 / D4](research-notes.md)。以下はM1実装後に使う手順の設計。
準備スクリプトや自作プロジェクトは、この文書コミットにはまだ含まれない。

1. WindowsのLEXIDE-Ω、デバッグドライバー、ROHMのデバイスパックを確認する。
   配布手順の対象は`ROHM.ML63Q25x7_DFP_1.0.1.pack`とCMSIS 5.9.0。
   導入済みバージョンとの組み合わせはWindows上で記録する。
2. Macで`references/`の資料から必要なROHM/CMSISファイルをローカル展開する。
   著作権表示を保ち、自作コードと`vendor/`を分ける。
3. `src/pneutouch-solist-ai/`全体をWindowsのローカルフォルダーへコピーする。
   例: `C:\PneuTouch\PneuTouchSolistAI`。`.project`、`.cproject`、`.settings/`、
   `generated/`、`vendor/`も含める。参照先だけMac側に残すコピーはしない。
4. LEXIDEの既存プロジェクトのインポートから取り込み、MCUがML63Q2557であることを確認する。
   デバイスファミリーはML63Q25x7。元のAIVibrationInferenceを上書きしない。
5. Clean / Buildを実行し、エラー、リンカーマップ、ROM/RAM量、HEX出力を保存する。
6. ソース変更時はGitのコミットを揃えてコピーし、IDEをRefreshして再ビルドする。
   Windows側だけで変更したファイルを上書きしないよう、差分を先に確認する。

共有フォルダーから直接ビルドする方法も考えられるが、初回はWindowsローカルの短い
ASCIIパスを使い、UNCパス・日本語パス・ファイル同期の問題を切り分けやすくする。

## 既存ファームウェアと移行

配布パッケージはAIVibrationInference向け。D3 / D4にはAISignalInferenceからの移行がある。
ガイド時点の名称はAISignalInference V2.0.25.1107とAIVibrationInference V1.2.25.0530。
**現在の実機のソフトウェア種類・版は未確認**なので、先に表示・配布物と照合する。

移行ガイドでは専用の`ReconfigurationAIVibrationInferenceV1.2.25.0530.hex`を使い、
JP8と電源再投入、完了表示の確認を経て対象ファームウェアへ進む。
これを通常の自作アプリHEXと混同しない。該当する移行が必要な場合はD4と
同梱のファームウェア書込手順書に沿って行う。未確認の実機へ自動適用しない。

## USBをMacとWindowsへ割り当てる

| USB機器 | 推奨接続先 | 用途 |
|---|---|---|
| MCU-LINK → CN2 | ParallelsのWindows | LEXIDEからSWDデバッグ |
| ボードCN9（FT2232H） | Mac | learning-toolまたはCLIでUART受信 |
| CN8 | 給電のみ | 通信ポートにはならない |

別々のUSB機器なので、デバッガーをWindows、ボードの通信をMacへ割り当てられる構成。
同じUSB機器を両OSへ同時に割り当てない。CN9をWindows側へ渡す場合はPCツールもWindowsで動かす。
FT2232HはA/Bの複数インターフェースを持つため、UARTのB側を識別する。[H1]

デバッガーで停止中はサンプルが送られない。書き込み完了だけでなくRun状態を確認する。
シリアルモニター、ブラウザー、CLIが同じポートを占有していないことも確認する。

## PCツールの実装・運用方針

- CLI: PythonとpySerialで受信し、画面なしでもCSVを保存できるようにする。
- 画面: Chrome/Edge等のWeb Serial対応環境を使用する。localhostまたはHTTPSで配信する。
- 接続ボタンからユーザーがポートを選択し、115200 baud / 8N1 / フロー制御なしで開く。
- 切断時は読み取りを解除してポートを閉じる。対応しないブラウザーではCLIへ案内する。
- learning-toolとdemo-visualizerは同じ受信処理を共有するが、初期は片方ずつ接続する。

具体的な起動コマンドは実装後に追記する。まだないスクリプトを実行済みの手順として記載しない。

## 実機での最初の確認

1. [配線と電圧](hardware.md)を確認し、JP1を3.3V、JP8を開発用の状態へ設定する。
2. 初回はAIと動画を切り離し、生値・時刻・連番だけ送る。
3. 起動メッセージ、約40 SPSの受信、押したときの変化、解放時の戻りを保存する。
4. 未接続時のエラー、再接続、電源再投入、受信欠番を確認する。
5. コミットID、HEX、ビルド結果、配線、センサー、サンプル周期、CSVを対応付ける。

HX711へ変更する場合は[別の配線・RATE設定](sensor-options.md)を使い、
40 SPSの記録と80 SPSの記録をメタデータで区別する。

## 今回の環境確認の限界

Parallels上でWindows 11とLEXIDE-Ωの起動を確認した。
`prlctl exec`によるゲスト内実行は導入エディションの制約で利用できなかった。
この制約は手動のLEXIDE操作を妨げるものではない。
今回、新規プロジェクトのWindowsビルド、実機書き込み、UART計測は実施していない。
