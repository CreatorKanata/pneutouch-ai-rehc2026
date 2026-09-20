<!-- Project entry point: concept, investigated constraints, and the implementation roadmap. -->
# PneuTouch AI — REHC2026

TPU製の中空恐竜を7つの空気室に分け、**1個の空気圧センサーの過渡波形から触れた場所を推定**するプロジェクトです。
DT-EBML63Q2557 / Solist-AIでのオンデバイス学習・推論を目指します。

Phase 0のファームウェアとPC計測ツールを作成しました。
通常のMPS20N0040D＋HX710Bの4ピンモジュールを40 SPS設定で読み、
UART経由で生値・波形を表示してCSVへ保存する構成です。
ホストテストとArm向けCコンパイルを確認済み。Windowsの最終ビルドと実機計測は未確認です。

## 最初に使う

配線、LEXIDEへの取り込み、起動方法は[Phase 0の手順](docs/phase0-bringup.md)を参照してください。
ローカルの`references/`を配置し、リポジトリ直下で実行します。

```sh
python3 tools/prepare_vendor.py
python3 tools/package_firmware.py
python3 tools/serve.py
```

`build/PneuTouchSolistAI.zip`をWindowsで展開し、LEXIDE-Ωへ取り込んでビルドします。
PC側はChrome / Edgeで[波形画面](http://localhost:8000/learning-tool/)を開きます。
CLIでのCSV収集も用意しています。AI学習・部位判定と動画デモは後続の実装です。

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
