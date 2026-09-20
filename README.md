<!-- Project entry point: concept, investigated constraints, and the implementation roadmap. -->
# PneutouchAI — REHC2026

**Created by Kanata the Kid Creator.**

TPU製の中空恐竜を7つの空気室に分け、**1個の空気圧センサーの過渡波形から触れた場所を推定**するプロジェクトです。
DT-EBML63Q2557 / Solist-AIでのオンデバイス学習・推論を目指します。

メインのファームウェアは **`src/pneutouch-solist` / `PneutouchAi`** です。
MPS20N0040D＋HX710Bを40 SPS設定で読み、CN9のUARTから生値を送信します。
2026-09-20、Debugビルド・PneutouchAi Writeによる書き込みと、
**COM7で315サンプル／実効39.72 SPS・欠番0の受信**を確認しました。
恐竜の造形も完成済みです。圧力の単位換算、気密、部位判定は別途検証します。

同日、保存済み43イベントによる**Solist-AI実機の教師あり4分類**も検証しました。
12特徴量は前半学習で21/22、逆順で19/22。64/128点の波形入力と比較しています。
[実機学習・比較結果](docs/solist-chip-validation-20260920.md)を参照してください。
記録内の探索評価です。現在は実センサーから12特徴量を抽出してチップ上でライブ分類し、
LCDへ `HEAD / BACK / LEGS / TAIL` を3秒表示します。
起動時に追加計測した60イベントから自動学習するため、推定時のPC操作は不要です。
頭・足の混同に対し、HEAD/LEGS専用の12入力モデルを追加しました。
保存記録の探索比較ではHEAD 15/15、全体52/60ですが、実操作ではHEADとLEGSが「半々くらい」と報告されています。
既存65イベントを再検証すると、振幅の大小関係が記録間で逆転し、解放後の形状が重なりました。
[頭と足の波形再検証](docs/head-legs-waveform-analysis-20260920.md)と
[隔壁・穴径によるフィルタ設計の検討](docs/pneumatic-filter-design-20260920.md)に詳しくまとめています。
[頭と足の判別改善](docs/head-legs-refinement-20260920.md)と
[ライブデモの操作・検証記録](docs/live-pressure-demo-20260920.md)を参照してください。

## 最初に使う

動画・部位画像・実測波形を表示する [HTMLデモ](src/demo-visualizer/README.md) を追加しました。
`python tools/serve_demo.py` を起動し、Chrome / Edgeで
[デモ画面](http://localhost:8001/demo-visualizer/)を開いて **COM7** に接続します。

[配線・書き込み・波形表示の手順](docs/phase0-bringup.md)と
[現在のUSB/COM接続記録](docs/local-hardware.md)を参照してください。

1. LEXIDEで **PneutouchAi (in pneutouch-solist)** を選び、**Debug**でビルド。
2. **PneutouchAi Write**をDebugとして起動して書き込み。デバッグには**PneutouchAi Debug**を使用。
3. `python tools/serve.py` を起動し、Chrome / Edgeで
   [波形画面](http://localhost:8000/learning-tool/)を開いて **COM7** に接続。

設定変更時は `python tools/generate_config.py` で設定ヘッダーを更新し、再ビルドします。
別PCへ移す場合は `python tools/package_firmware.py` で `build/PneutouchAi.zip` を作れます。
現在のプロジェクトでは旧 `prepare_vendor.py` の実行は不要です。
旧 `src/pneutouch-solist-ai` は参考用に残しています。

## ドキュメント

- [ドキュメント一覧・現在地](docs/README.md)
- [Phase 0：配線・ビルド・計測手順](docs/phase0-bringup.md)
- [コンセプト原文](docs/concept.md)
- [企画の核と検証方針](docs/product-design.md)
- [ボード・センサー・配線](docs/hardware.md)
- [HX711への変更と高速化の候補](docs/sensor-options.md)
- [ディレクトリ構成とアプリの役割](docs/architecture.md)
- [実装プランと完了条件](docs/implementation-plan.md)
- [Mac / Windowsでの開発方法](docs/development-workflow.md)
- [調査資料と未解決事項](docs/research-notes.md)

3Dモデルは `3d-models/`、配布コードやPDFはローカルの `references/` に置きます。
`references/` はGit管理対象外です。
