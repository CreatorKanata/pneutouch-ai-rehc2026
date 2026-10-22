# 最終審査（2026-10-24）向け資料

| 文書 | 内容 |
|---|---|
| [strategy-review.md](strategy-review.md) | **まず読む。** GPT案と実装の照合、3基準での現在地、10日間で賭けるもの（teach モード、独立評価） |
| [plan-10days.md](plan-10days.md) | 日程、独立評価の取り方、バックアップ方針 |
| [retrain-85a.md](retrain-85a.md) | **85A 恐竜（v2）の再学習手順（10/21、10/22 更新）**: 収録 → `prepare_retrain.py` → `generate_pressure_model.py` → ビルド → 独立評価 |
| [test-pieces-results.md](test-pieces-results.md) | **試験片 ti1〜ti5 の検証結果（10/21）**: あそびで減衰時間が 2.4〜2.9 倍変わり 3 分類 97%、go |
| [test-pieces-protocol.md](test-pieces-protocol.md) | 試験片 ti1〜ti5 のテスト手順（Codex / 作業者向け。接続、記録、ファイル名、解析コマンド、go/no-go 判定） |
| [what-discriminates.md](what-discriminates.md) | **何で場所を見分けられるか**: 文献3調査と計算の総合判断（受動空気では皮の内側だけ。音響なら空気室の形と穴の両方） |
| [survey-A-passive-fluidic.md](survey-A-passive-fluidic.md) | 受動・流体で1センサー位置推定の先行研究（Goshtasbi 2026 ほか16件） |
| [survey-B-acoustic.md](survey-B-acoustic.md) | 空気室の音響で位置を当てる先行研究（Zöller/Wall/Brock、CMU 2025 ほか22件）とMCU実装の見積もり |
| [survey-C-flow-and-structure.md](survey-C-flow-and-structure.md) | ポンプ・穴・構造符号化の先行研究（AirTouch 詳細、Sariola、Blowhole、Lamello）と分圧設計の計算 |
| [reprint-design.md](reprint-design.md) | 恐竜 v2 再印刷の検討: 先行研究（AirTouch ほか）、星形流路の設計原理、試験片で go/no-go |
| [teach-mode-spec.md](teach-mode-spec.md) | 「恐竜に教える」モードの仕様（implementer に渡す） |
| [presentation-5min.md](presentation-5min.md) | 5分の構成、想定質疑と根拠、載せる数字 |
| [second-opinion-codex.md](second-opinion-codex.md) | 独立した第二意見（Codex）の記録と、それへの判断 |
| [gpt-strategy-draft-20261014.md](gpt-strategy-draft-20261014.md) | GPT作成の戦略案（原文保存。競合分析と審査員分析はここ） |
| [rehc2026-official-site.md](rehc2026-official-site.md) | 公式サイト rehc.jp の記録（審査基準、賞、FAQ） |
| [final-judging-event.md](final-judging-event.md) | 最終審査会イベントページの記録（ルール、審査員、10作品） |
