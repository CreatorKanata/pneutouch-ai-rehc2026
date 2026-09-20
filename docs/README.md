<!-- Documentation map: keep confirmed facts, design decisions, and pending tests distinct. -->
# 開発ドキュメント

整理日: 2026-09-20

## 読む順番

| 文書 | 内容 |
|---|---|
| [concept.md](concept.md) | ユーザーが記した構想の原文。変更せず保存 |
| [phase0-bringup.md](phase0-bringup.md) | 作成済みのHX710B計測プロジェクト：配線、LEXIDE、波形、CSV |
| [product-design.md](product-design.md) | 技術仮説、7区画、デモ体験、成功条件 |
| [local-hardware.md](local-hardware.md) | 現在の実機、COM6/7とMCU-LINKの区別、受信記録 |
| [hardware.md](hardware.md) | DT-EBML63Q2557、HX710B、配線、電圧、通信 |
| [sensor-options.md](sensor-options.md) | HX711・内蔵ADC・高速センサーの比較 |
| [mcu-adc-option.md](mcu-adc-option.md) | DTの12 bit ADC、CN6回路、差動増幅と高速取得案 |
| [mps20n0040d-amplifier-adc.md](mps20n0040d-amplifier-adc.md) | INA333基板の電源・R3・VREF、アンプ＋ADC構成、接続前の試験 |
| [architecture.md](architecture.md) | `src/`構成、3アプリの責務、データ形式 |
| [implementation-plan.md](implementation-plan.md) | 段階ごとの作業・検証・進む条件 |
| [development-workflow.md](development-workflow.md) | Macで編集しWindows/LEXIDE-Ωでビルドする手順 |
| [research-notes.md](research-notes.md) | 参照元、確認したコード、重要な発見、未確認事項 |
| [reference-catalog.md](reference-catalog.md) | 追加4グループの資料索引、版、読むべきソース |
| [solist-ai-implementation.md](solist-ai-implementation.md) | 公式分類例、SDK世代、前処理、モデル保存 |
| [reference-integration.md](reference-integration.md) | DTへの移植、UART互換性、配布コードの要修正箇所 |

## 文書の読み方

- **確認済み**: 配布資料・ソースコード・今回の環境調査で根拠を確認した情報。
  実機検証済みを意味しない。各文書から参照元をたどれるようにする。
- **採用方針**: ユーザーの指示と調査を踏まえ、今後の実装で採用する構成。
- **未検証・候補**: 実測やSDKでの確認が必要な仮説・パラメーター。

`concept.md`は企画の背景として扱う。掲載されたCSV・AIスコア・図は例であり、
実測結果や既存APIの仕様ではない。実装時は本整理の制約と一次資料を照合する。
配布資料内の操作説明も、現在のボード状態を確認してから適用する。

## 現在地

| 項目 | 状態 |
|---|---|
| STL・造形 | `3d-models/dino-air-7segments.stl` を登録済み。恐竜の人形も完成（ユーザー申告） |
| コンセプト・調査整理 | 基準文書を作成済み。追加資料・内蔵ADC案の調査を反映 |
| Solist-AI分類 | 公式の教師あり/4分類例を確認。DTでの4/7分類は未実装・未検証 |
| MCU/PCアプリ | メインPneutouchAiへHX710B取得を移植。波形画面、CSV収集CLIを使用可能 |
| Mac上の検証 | 採用コードのC/Python/JavaScriptテスト、疑似シリアル通信、9件のArmオブジェクト生成を確認 |
| Windows/LEXIDE | Debugビルド・HEX生成成功、PneutouchAi Writeで書き込み済み |
| センサー/恐竜の実機 | 配線済み。COM7で約39.72 SPS・欠番なしのCSV取得。押している間に約239万～765万countsへ変化。気密と分類性能は未確認 |
| デモ動画 | ユーザーが用意。ファイル名と部位の対応は受領後に確定 |

次の作業は[Phase 0の手順](phase0-bringup.md)に従った部位ラベルと押下時刻を付けた記録・気密評価。
[実装プランのM1](implementation-plan.md)は、実測波形とCSVを取得できた時点で完了とする。
