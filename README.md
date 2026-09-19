<!-- Project entry point: concept, investigated constraints, and the implementation roadmap. -->
# PneuTouch AI — REHC2026

TPU製の中空恐竜を7つの空気室に分け、**1個の空気圧センサーの過渡波形から触れた場所を推定**するプロジェクトです。
DT-EBML63Q2557 / Solist-AIでのオンデバイス学習・推論を目指します。

現在は調査・設計の整理段階です。このコミットはドキュメントの基準点であり、
ファームウェアの実機動作や7区画分類の成立を示すものではありません。

- [ドキュメント一覧・現在地](docs/README.md)
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
