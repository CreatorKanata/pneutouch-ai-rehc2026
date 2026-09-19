<!-- Documentation map: keep confirmed facts, design decisions, and pending tests distinct. -->
# 開発ドキュメント

整理日: 2026-09-20

## 読む順番

| 文書 | 内容 |
|---|---|
| [concept.md](concept.md) | ユーザーが記した構想の原文。変更せず保存 |
| [product-design.md](product-design.md) | 技術仮説、7区画、デモ体験、成功条件 |
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
| STL | `3d-models/dino-air-7segments.stl` を登録済み |
| コンセプト・調査整理 | 基準文書を作成済み。追加資料・内蔵ADC案の調査を反映 |
| Solist-AI分類 | 公式の教師あり/4分類例を確認。DTでの4/7分類は未実装・未検証 |
| MCU/PCアプリ | 構成とPhase 0仕様を定義。リポジトリへの実装追加は次の作業 |
| Mac上の予備検証 | 作業用試作でC読取ロジック・受信処理のテスト、Arm向けオブジェクト生成を確認 |
| Windows/LEXIDE | 起動中の環境を確認。新規プロジェクトの最終リンク・HEX生成は未確認 |
| センサー/恐竜の実機 | 給電、通信、気密、波形、分類性能は未確認 |
| デモ動画 | ユーザーが用意。ファイル名と部位の対応は受領後に確定 |

次の作業は、[実装プランのM1](implementation-plan.md)に従って
`src/pneutouch-solist-ai/`のLEXIDEプロジェクトとPhase 0受信ツールを実装すること。
予備試作のテスト結果は、最終的に採用したソースに対して再確認する。
