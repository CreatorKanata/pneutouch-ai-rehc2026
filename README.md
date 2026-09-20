<!-- Project entry point: concept, investigated constraints, and the implementation roadmap. -->
# PneuTouch AI — REHC2026

TPU製の中空恐竜を7つの空気室に分け、**1個の空気圧センサーの過渡波形から触れた場所を推定**するプロジェクトです。
DT-EBML63Q2557 / Solist-AIでのオンデバイス学習・推論を目指します。

メインのファームウェアは **`src/pneutouch-solist` / `PneutouchAi`** です。
MPS20N0040D＋HX710Bを40 SPS設定で読み、CN9のUARTから生値を送信します。
2026-09-20、Debugビルド・PneutouchAi Writeによる書き込みと、
**COM7で315サンプル／実効39.72 SPS・欠番0の受信**を確認しました。
恐竜の造形も完成済みです。圧力の単位換算、気密、部位判定は別途検証します。

## 最初に使う

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
