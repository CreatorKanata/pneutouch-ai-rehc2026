<!-- Development workflow: separate Mac-side checks from Windows builds and hardware evidence. -->
# Mac / Windowsでの開発方法

## 役割分担

**Macでも開発できる。** 編集、Git、PCツール、データ解析、ハード非依存テストをMacで行い、
公式環境を使ったファームウェアのビルド・書き込み・デバッグはWindowsのLEXIDE-Ωで行う。
調査時のMacはIntel（x86_64）。Apple SiliconでのWindows/USB互換性を確認した結果ではない。

| 作業 | 使用環境 | 確認状況 |
|---|---|---|
| C / Python / JavaScriptの編集、Git | Mac | 利用可能 |
| Cホストテスト、通信処理のテスト | Mac | 採用コードで実施。pySerialと疑似端末による受信も確認 |
| Arm向けCオブジェクト生成 | MacのClang | Phase 0と必要な配布コードの9件で確認 |
| 最終リンク、HEX生成 | Windows / LEXIDE-Ω | PneutouchAiのDebugで成功、エラー・警告0 |
| SWD書き込み・実行 | Windows / MCU-LINK | PneutouchAi WriteでHX710B取得ファームを書き込み、実行確認 |
| 圧力波形・CSV | Mac / 対応ブラウザーまたはPython | COM7の実機UARTで約39.72 SPS、CSV保存を確認 |
| デモ動画表示 | Macを第一候補 | 後続の実装 |

MacのClangでCをコンパイルできても、Solist-AIライブラリーとの最終リンク、
メモリー配置、Option領域、HEX生成、デバッガーの動作が保証されるわけではない。
Mac単独のビルド環境作成は後続の選択肢とし、まず配布資料が想定する環境で基準を作る。

## LEXIDE-Ωの準備と取り込み方針

現在のメインは **`src/pneutouch-solist` / PneutouchAi**。
動作確認済み基準（`95a453e`）のプロジェクト、リンカー、起動コード、ドライバーを使い、
`S_PneuTouch` へHX710B計測コードを移植している。旧 `pneutouch-solist-ai` は参考用。

1. WindowsのLEXIDE-Ω、CMSIS 5.9.0、ROHM ML63Q25x7デバイスパックを用意する。
   この環境で確認したROHMパックは1.1.0、LAPISビルドツールはVer.20260317。
2. `src/pneutouch-solist` を既存プロジェクトとして取り込み、**PneutouchAi** を選ぶ。
3. 設定変更時はルートの `config.py` を編集し `python tools/generate_config.py` を実行。
   ヘッダーはGit管理対象。`--check` で設定との一致を検査できる。
4. IDEでRefreshし **Debug** 構成でビルド。更新されたELF/HEXとビルドログを確認する。
5. **PneutouchAi Write** をDebug起動して書き込み、**PneutouchAi Debug** でデバッグする。
   両方とも `Debug/PneutouchAi.elf` を使う。起動時に停止したらResumeする。
6. CN9のUART Bへ115200 / 8N1で接続。現在は **COM7**。
   COM6はCN9のA側、COM4/COM5はMCU-LINK側として区別する。

詳しい[配線・計測手順](phase0-bringup.md)と[実機接続記録](local-hardware.md)を参照。
別PCへ渡す場合、`python tools/package_firmware.py` が `build/PneutouchAi.zip` を作る。
launchには現在のPCのTCL・パックへの絶対パスがあるため、新環境では該当箇所を確認する。
`prepare_vendor.py` は旧プロジェクト専用で、現在のプロジェクト準備には不要。

## 追加SDKを取り込む際の注意

- [追加資料の索引](reference-catalog.md)を起点に、採用パッケージとヘッダー/ライブラリーのハッシュを記録する。
- 新AIサンプルのDebugは新ライブラリー、Releaseは同梱されていない旧ライブラリー名を参照している。
  両構成を揃えてClean Buildし、HEX/mapを確認する。詳細は[要修正箇所](reference-integration.md)。
- RB用のUARTF0/P20/P21と、DTのUARTF1/P70/P71・電源保持を分けて移植する。
- Python例にはMacのポート探索と学習/保存APIの修正点がある。配布物の無修正動作を前提にしない。
- AIVibrationの説明書Rev.20260417/20260421と、同梱HEX/EXEのバージョンは別に記録する。

## 既存ファームウェアと移行

配布パッケージはAIVibrationInference向け。D3 / D4にはAISignalInferenceからの移行がある。
ガイド時点の名称はAISignalInference V2.0.25.1107とAIVibrationInference V1.2.25.0530。
2026-09-20時点の本実機にはPneutouchAiのHX710B計測ファームウェアを書き込み済み。
以下の再構成手順は配布資料の履歴であり、今回実行していない。

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

リポジトリ直下で`python3 tools/serve.py`を実行し、Chrome / Edgeで
`http://localhost:8000/learning-tool/`を開く。CLIの準備とCSV保存は[計測手順](phase0-bringup.md)を参照。

Macで`python3 tools/test.py`によりC/Python/JavaScriptのテストを実行できる。
pySerialを導入したPythonでは、Macの疑似端末を通す受信テストも含める。
`python3 tools/check_firmware.py --cmsis-include <CMSISのCore/Include>` は現在の計測経路のArmオブジェクトを生成する。
これらは最終リンク・書き込み・実機試験を置き換えるものではない。

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
