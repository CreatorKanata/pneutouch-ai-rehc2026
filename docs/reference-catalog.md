<!-- Reference catalog: local evidence, version boundaries, and reuse priorities. -->
# 追加された参考資料の索引

調査日: 2026-09-20。ユーザー指定の4グループを中心に再調査した。
`references/`全体のファイル一覧を取得し、34 PDFをテキスト化して検索、必要な章とソースを読んだ。
全ソースの動作検証や全文レビューではない。容量表の不一致とDTのADC回路はPDF画像でも照合した。

## 今回の重要な更新

- 公式の教師あり学習・4分類例が見つかり、1モデル4/7出力での開発を具体化できた。
- 旧SDKと新SDKのAPI・ライブラリー、RBとDTのピン、UARTの8N1/8E1を分けて扱う必要がある。
- Pythonラッパーの引数/フレーム不整合、LEXIDE Releaseの旧ライブラリー参照を確認した。
- DT内蔵12 bit ADCと基板上アナログ回路を、高速圧力取得の別案として整理した。
- 40 SPSで7区画を判別できるか、実機でのメモリー・速度・精度は未確認のまま。

詳細は[AI実装メモ](solist-ai-implementation.md)、[移植・PC連携](reference-integration.md)、
[内蔵ADC案](mcu-adc-option.md)を参照。既存の根拠H1/C1等は[調査ノート](research-notes.md)。

## 追加資料と用途

N番号は本調査の参照ID。ページは特記しない限り**PDF先頭を1とするページ番号**。
N6は表紙の分だけ本文の印刷ページとずれる（例: PDF p.19 = 本文p.18）。
リンク先はGit除外のローカル資料。別環境では原資料を同じ位置へ用意する。

| ID | 資料 | 参照目的・版 |
|---|---|---|
| N1 | [FJXT63Q2500_REFSOFT_STARTUP-03.pdf](<../references/rohm/ML63Q2500_ReferenceSoftware_v120/Document/FJXT63Q2500_REFSOFT_STARTUP-03.pdf>) | ReferenceSoftwareの起動・API・教師あり/FFT例。FJXT63Q2500_REFSOFT_STARTUP-03。PDF p.8, 24, 32–36 |
| N3 | [Solist-AI_AnomalyDetection_AppNote.ja.pdf](<../references/rohm/Solist-AI_FW_AnomalyDetectionSample/Document/Solist-AI_AnomalyDetection_AppNote.ja.pdf>) | 新AIライブラリー・前処理・保存。CTD67-S03E-0001 Rev.003、2026.7。PDF p.17–19, 31–36 |
| N5 | [Solist-AI_Uart_IF_AppNote.ja.pdf](<../references/rohm/Solist-AI_FW_UART_IF_Sample/Document/Solist-AI_Uart_IF_AppNote.ja.pdf>) | UART I/FとPython分類例の使い方。CTD67-S03E-0003 Rev.001、2026.3。PDF p.4, 7–8 |
| N6 | [Solist-AI_CommandSpec_CTD67-S03S-0006.pdf](<../references/rohm/Solist-AI_FW_UART_IF_Sample/Document/Solist-AI_CommandSpec_CTD67-S03S-0006.pdf>) | UARTバイナリー通信の仕様。CTD67-S03S-0006 Rev.001。PDF p.4–6, 10–12, 19 |
| N8 | [ML63Q2500-datasheet.pdf](<../references/ML63Q2557/ML63Q2500-datasheet.pdf>) | ML63Q2557の電気・メモリー・ADC仕様。FJDL63Q2500-03、2025-01-20。PDF p.1–3, 15, 28 |
| N9 | [ML63Q2500グループ ユーザーズマニュアル.pdf](<../references/ML63Q2557/ML63Q2500グループ ユーザーズマニュアル.pdf>) | レジスターと周辺回路の詳細。FJUL63Q2500-02、2026-01-20。第3章メモリー、第18章UART、第26章ADC |
| N10 | [RB-D63Q2557TB64 ユーザーズマニュアル.pdf](<../references/ML63Q2557/RB-D63Q2557TB64 ユーザーズマニュアル.pdf>) | RBボードとDTボードの違い。FJBL63Q2557TB64_RB-02、2025-12-19。PDF p.6–7 |
| N11 | [Solist-AI のアルゴリズムと学習 AxlCORE-ODLの特徴.pdf](<../references/ML63Q2557/Solist-AI のアルゴリズムと学習 AxlCORE-ODLの特徴.pdf>) | Alpha/Beta・教師あり分類の考え方。特にPDF p.28–36の4種類・one-hot教師・学習/評価例 |
| N12 | [Solist-AI 活用事例集.pdf](<../references/ML63Q2557/Solist-AI 活用事例集.pdf>) | Solist-AIの活用範囲。異常検知/教師あり/前処理の用途。PDF p.4–8, 14。恐竜での精度の証拠ではない |
| N13 | [AIVibrationInferenceHost_users_manual_Rev.20260421.pdf](<../references/AIVibrationInferenceHost Rev.20260421/AIVibrationInferenceHost_users_manual_Rev.20260421.pdf>) | DTのPCホスト操作・波形・CSV・重み。説明書Rev.20260421。PDF p.6, 11–14, 24–36 |
| N14 | [AIVibrationInference_users_manual_Rev.20260417.pdf](<../references/AIVibrationInferenceRev.20260417/AIVibrationInference_users_manual_Rev.20260417.pdf>) | DTの異常検知ファームウェア操作。説明書Rev.20260417。PDF §7、p.35の改訂履歴 |

## ソースコードの起点

以下の略号は各文書内のソースパスの先頭に使用する。
パッケージ名のv120とヘッダー履歴のVer 1.3.0は異なる番号体系なので、読み替えない。

| ID / 略号 | 実際のディレクトリ | 役割 |
|---|---|---|
| N2 / `REF` | `references/rohm/ML63Q2500_ReferenceSoftware_v120/SourceCode` | 周辺ドライバー、教師あり/FFT/ESN/ADCの単機能例 |
| N4 / `ANOM` | `references/rohm/Solist-AI_FW_AnomalyDetectionSample/SourceCode` | 新AIライブラリー、取得→前処理→AI、Scope連携 |
| N7 / `UART` | `references/rohm/Solist-AI_FW_UART_IF_Sample/SourceCode` | 新AIライブラリー、PCからの教師あり学習・推論 |

### 優先して読むファイル

| 相対パス（上記略号が起点） | このプロジェクトで参考にすること |
|---|---|---|
| `REF/samples/Solist-AI/SupervisedLearning/src/MlTask.c` | 教師ベクトルを渡すC API、完了待ち、期待値照合 |
| `REF/samples/Solist-AI/FftSample/src/main.c` | none/HannのFFT、付属入力と期待値での照合 |
| `REF/samples/Solist-AI/EsnSample/` | 状態を持つ時系列モデルの後続候補。初期採用はしない |
| `REF/driver/src/ai_ram_save.c` | Beta/Pの取得・復元。ただし保存先はプログラムFlash |
| `REF/samples/SA-ADC/TimerTrigger/src/main.c` | タイマで周期を決めるADC取得と結果読取り |
| `REF/samples/SA-ADC/SoftwareTrigger/` | ADC単体確認の入口 |
| `ANOM/samples/Solist-AI/AnomalyDetectionDemo/board/SensorSettings.h` | センサー初期化・読取インターフェースの分離 |
| `ANOM/samples/Solist-AI/AnomalyDetectionDemo/src/SensorControl.c` | ダブルバッファ、窓完成通知 |
| `ANOM/samples/Solist-AI/AnomalyDetectionDemo/src/Preprocess.c` | 数値変換、FFTと特徴数の扱い |
| `ANOM/samples/Solist-AI/AnomalyDetectionDemo/src/MlTask.c` | 学習/推論と処理シーケンスの分離 |
| `UART/driver/inc/solistAi.h`、`UART/driver/inc/mlacc_odl.h` | 新SDKの宣言、構造体、モデル初期化と保存API |
| `UART/samples/Solist-AI/SolistAI_UART_IF/src/CommunicationTask.c` | コマンド処理の入口。PNEU1の受信実装とは別 |
| `UART/samples/Solist-AI/SolistAI_UART_IF/src/MlTask.c` | UART要求からAI処理を呼ぶ部分 |
| `UART/samples/Solist-AI/SolistAI_UART_IF/script/SolistAI_Classification.py` | 520→66→4の分類例、ExcelデータをMCUで学習 |
| `UART/samples/Solist-AI/SolistAI_UART_IF/script/SolistAi.py` | フレーム分割、bfloat16、学習/推論、重み操作。要修正箇所あり |
| `UART/samples/Solist-AI/SolistAI_UART_IF/script/bf16.py` | PC側bfloat16変換の参考 |
| `UART/samples/Solist-AI/SolistAI_UART_IF/board/RB_D63Q25.c` | RBのピン/起動設定。DTへ移植が必要 |
| `UART/samples/Solist-AI/SolistAI_UART_IF/.cproject` | Debug/Release両方のリンク対象確認 |

## AIVibration配布物の版を区別する

| 追加フォルダー | 説明書 | 同梱実行物の名称 |
|---|---|---|
| `AIVibrationInferenceHost Rev.20260421` | Host説明書Rev.20260421 | `v1.0.25.0428exe/AIVibrationInferenceHost.exe` |
| `AIVibrationInferenceRev.20260417` | ファームウェア説明書Rev.20260417 | `AIVibrationInferenceV1.2.25.0530.hex` |

2026年のフォルダー名・説明書改訂日を、実行物のバージョンと取り違えない。
N13の改訂履歴は2026-04-17が構成見直し、04-21が表の表示変更。
N14の2026-04-17改訂は構成見直し・クイックスタート追加。
新しい説明書を確認したことは、新機能がファームウェアへ追加されたことの確認ではない。
現在の実機のファームウェア版は引き続き未確認。書き込み用HEXと再構成専用HEXも区別する。

## 読み取れること・まだ読めないこと

- N8/N9はMCU単体の仕様、N10はRBボードの仕様。DTの回路・コネクターはH1を使う。
- N11のone-hot分類は直接利用できる設計上の根拠。N12の用途例は恐竜での実測結果ではない。
- Simの追加物は`references/ML63Q2557/Solist-AI_Sim_AnomalyDtection_3/`内の
  `SolistAI_Sim_V10101_Installer_web.exe`と利用条件。インストール・動作は未確認。
- Hostの高速リアルタイム転送は生センサーデータ、ブロック転送例はFFTと異常度。
  CSVの意味を区別してlearning-toolの設計へ反映する。
- 旧資料とSolistpanの既存調査は残し、今回解消した「公式分類例の有無」だけを更新する。

## 原本と再現性

`references/`はGit除外を維持する。配布PDF・EXE・HEX・ライブラリー全体をdocsへ複製しない。
docsには出典・要約・採用方針・未検証事項を残す。実装で必要な依存物は利用条件と著作権表示を保持し、
ローカル展開手順・採用版・ハッシュを管理する。実機計測やビルド結果を得たら同じ資料IDへ追記する。
