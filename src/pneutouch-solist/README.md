# PneutouchAi — HX710B計測

このフォルダーがメインのファームウェア開発プロジェクト。
LEXIDE表示名は **PneutouchAi (in pneutouch-solist)**。

- **Debug**でビルドし、**PneutouchAi Write**をDebug起動して書き込み。
- ソースデバッグは **PneutouchAi Debug**。両方とも `Debug/PneutouchAi.elf` を使う。
- CN3のHX710Bを40 SPS設定で読み、CN9のUART Bへ `PNEU1,seq,ms,raw` を送る。
- 115200 baud / 8N1 / フロー制御なし。現在のWindowsの受信先は **COM7**。
- `S_System/main.c` が `S_PneuTouch/pressure_app.c` を起動する。
- `S_PneuTouch/board.c` は電源保持、PLL、SysTick、GPIO、UARTを初期化。
- `hx710b.c` は24ビット読み出し、`acquisition.c` は安定待ちとタイムアウト。
- `pneutouch_status` をデバッガーで監視できる。未取得・タイムアウト時のrawを最新計測値と混同しない。

設定はルートの `config.py` を編集し、`python tools/generate_config.py` で
`S_PneuTouch/pneu_config.h` を更新する。リンカー、起動コード、ドライバー、
AIライブラリーとDebug/Write設定は動作確認済みのプロジェクトを引き継いでいる。
元の振動デモのLCD・AI・センサー・独自通信の初期化は現在呼び出さない。

2026-09-20: Windows/LEXIDEのDebugビルド成功（エラー・警告0）。
PneutouchAi Writeで書き込み、COM7から315点・実効39.72 SPS・欠番0を確認。

詳しい[配線・計測手順](../../docs/phase0-bringup.md)と
[この実機のUSB/COM接続記録](../../docs/local-hardware.md)を参照。
元の動作確認済み基準はコミット `95a453e`。
現在は `pressure_features.c` による開始時刻未知の12特徴抽出と、
`ai_validation.c` によるCSV由来入力の実機学習・推論にも対応。
4分類はしっぽ／背中／足全体／首と頭、約1秒押して離す操作を想定する。
[実機比較と再実行手順](../../docs/solist-chip-validation-20260920.md)を参照。
通常取得時は `# PNEE1` / `# PNEF1` を追加し、生値の `PNEU1` は維持する。
AIは明示的なPAI1検証モードで初期化し、その間はセンサー取得を停止する。
検証終了後は通常取得へ戻る。ライブ分類結果の自動出力・モデル永続保存はまだ行わない。
旧 `../pneutouch-solist-ai` は実機での移植確認が落ち着くまで参考用に保持する。
