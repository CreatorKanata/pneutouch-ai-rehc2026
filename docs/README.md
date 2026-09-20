<!-- Documentation map: keep confirmed facts, design decisions, and pending tests distinct. -->
# 開発ドキュメント

整理日: 2026-09-20

## 読む順番

| 文書 | 内容 |
|---|---|
| [live-pressure-demo-20260920.md](live-pressure-demo-20260920.md) | 実圧力→12特徴→Solist-AIのライブ推定とLCDの3秒表示 |
| [head-legs-refinement-20260920.md](head-legs-refinement-20260920.md) | HEAD/LEGS混同の分析、圧力ピークを使う専用モデル、実機検証と制限 |
| [concept.md](concept.md) | ユーザーが記した構想の原文。変更せず保存 |
| [phase0-bringup.md](phase0-bringup.md) | 作成済みのHX710B計測プロジェクト：配線、LEXIDE、波形、CSV |
| [product-design.md](product-design.md) | 技術仮説、7区画、デモ体験、成功条件 |
| [local-hardware.md](local-hardware.md) | 現在の実機、COM6/7とMCU-LINKの区別、受信記録 |
| [pressure-validation-20260920.md](pressure-validation-20260920.md) | 7区画43回の実測分析。原仮説、代替特徴、開始時刻未知の再生、約1秒のデモ条件 |
| [solist-chip-validation-20260920.md](solist-chip-validation-20260920.md) | 保存CSVで実機を自動学習。12特徴量と64/128点波形の4分類・時間・メモリ比較 |
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
| センサー/恐竜の実機 | 配線済み。COM7で7区画14,232点・約39.72 SPS・欠番0。完全波形43回を解析。気密と部位差の物理的原因は未確定 |
| 部位特徴・デモ条件 | 立ち上がり、負側幅、正負面積比が候補。開始時刻なしの逐次再生を検証。約1秒で握って離すデモを提案。独立記録の分類評価・実機の連続推論は未完了 |
| デモ動画 | ユーザーが用意。ファイル名と部位の対応は受領後に確定 |

次の作業は[実測分析の提案](pressure-validation-20260920.md#10-次に何を検証するか)に沿って、
約1秒で握って離す条件で部位を混ぜた追加記録、保持・気密の切り分け、独立記録での評価。
接触開始は圧力から自動検出し、外部の接触時刻をデモの入力にしない。
[実装プランのM2](implementation-plan.md)は予備試験段階で、最低30回/部位の収集は未完了。
