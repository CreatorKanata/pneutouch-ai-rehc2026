# FeRAM モデルストアとチップ上の特徴選択（`new-approach` ブランチ）

## なぜ

- 学習データをコードに埋め込まない（旧 `pressure_model_data.h` は廃止）。ボードの FeRAM（MB85RS2MTA、256 KiB）に置くので、**データを替えてもビルド・書き込みは不要**。
- 正規化の平均・スケールも、どの特徴を使うかも、**起動時にチップが自分で決める**（Fisher 比）。PC は生の記録を送るだけ。
- スロットは 4 つ。恐竜を替えたとき・取り直したときに旧データを残したまま切り替えられる。

## 手順

```
.venv/bin/python tools/prepare_retrain.py captures/dino-85a --out docs/analysis/<日付>-dino-85a --replay build/replay_pressure_features
.venv/bin/python tools/feature_select.py docs/analysis/<日付>-dino-85a/pressure-demo-training.json      # PC で選択と LOO を確認（任意）
.venv/bin/python tools/model_store.py --port COM7 load --slot 0 --input docs/analysis/<日付>-dino-85a/pressure-demo-training.json --use
```

`load` は MODE 1 → OPEN → 全件 PUT → COMMIT → 先頭と末尾を GET してビット一致を確認 → （`--use`）USE → TRAIN → MODE 0。
MODE 0 に戻るとファームウェアがもう一度学習し直すので、電源を入れ直さなくてもそのまま使える。`--k 1..12` で特徴数を固定（既定 0 = 自動）、`--name` は 8 文字まで（既定は日付）。

| やりたいこと | コマンド |
|---|---|
| 各スロットの件数と有効スロット | `model_store.py --port COM7 info` |
| スロット切替 | `model_store.py --port COM7 use --slot 1`（MODE 0 復帰時に再学習） |
| 吸い出し | `model_store.py --port COM7 dump --slot 0 --out slot0.json`（`prepare_retrain` と同じ JSON 形。`feature_select.py` にそのまま渡せる） |
| 消去 | `model_store.py --port COM7 erase --slot 1` |
| 今すぐ再学習 | `model_store.py --port COM7 train` |
| FeRAM 自己診断 | `model_store.py --port COM7 selftest`（0x3F000 の作業領域だけを書き換える） |

## 起動時に表示されるもの

- UART: `# PNEM3,READY,slot=0,count=176,err=0,sel=0:4:10:13:12,fisher=45000:0:...`（fisher は ×1000 の整数、14 個、プール順）。`err`: 0 なし、1 FeRAM 異常、2 有効スロットなし、3 記録の CRC/値異常、4 選択不能、5 Solist-AI 異常。`# PNEM1` は従来どおり（件数、ステップ数、学習時間 us）、`# PNEM2,NONE,0,0,single_instance` は旧ツール互換のためだけに残す。
- LCD 1 行目: 学習中は `LEARN<段階> 済/全`（例 `LEARN1  123/ 704`、0 = 統計、1 = 学習）、終わると `READY`。スロットが無ければ `NO MODEL`、その他の失敗は `AI ERROR`。
- LCD 2 行目: 選ばれた特徴を黒■（0xFF）、選ばれなければ空白。右端 2 桁は選んだ数。選択が決まった時点（学習開始時）で表示する。

| 列 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14–15 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 特徴 | rise | decay | width | area_width | slope | asym | rel_rise | recovery | rel_width | rel_area_width | trough_ratio | area_ratio | peak | trough | 選んだ数 |

## 選択アルゴリズム（`feature_select.c`、PC 版 `tools/feature_select.py` と同じ float32 計算）

1. 各記録の 14 値（12 特徴、正圧ピーク、トラフの大きさ。すべて > 0）を `logf` する。
2. 1 回の読み出しでクラスごとの件数・和・二乗和をためる（RAM に記録を持たない）。
3. 特徴 j ごとに全体平均、クラス内平方和 W = Σ(二乗和 − 和²/n)、クラス間平方和 B = Σ n(クラス平均 − 全体平均)²。
4. Fisher 比 = B / W（W ≤ 0 なら 0）。母標準偏差 ×3 をスケールにし、スケール ≤ 1e-6 の特徴は 0 にして選ばない。
5. Fisher 比の降順に並べる（同点は番号の小さい方が先）。
6. k = 1..12 なら上位 k 個。k = 0（自動）なら「最大値の 1/16 以上」のものを全部、ただし 2 個以上 12 個以下。
7. 入力語 = bf16((log 値 − 平均) / スケール)、選んだ順に詰め、残りの入力は 0（Solist-AI の入力は 12 のまま）。
8. 学習順は記録を RAM に並べず、エポック e ごとに `(i × 素数_e + 37e) mod n`（素数は 3,5,7,11,… のうち n を割らないもの）で FeRAM から読み直す。4 エポック。
9. 隠れ 32、seed 1、シグモイド、MSE、Beta = 0・P = I から開始（従来どおり）。
10. 推論は同じ選択・平均・スケールで 1 回（Solist-AI インスタンス 0 のみ。HEAD/LEGS 専用モデルは廃止）。

## FeRAM レイアウト（リトルエンディアン）

0x0000–0x0FFF は旧 `S_System/ConfigData.c` の領域なので触らない。

| アドレス | 内容 |
|---|---|
| 0x1000（16 B） | ディレクトリ: magic `0x54534E50`（'PNST'）u32、version 1 u16、有効スロット u8、予約 u8、CRC16（先頭 8 B）u16、予約 6 B |
| 0x2000 + n × 0xF000 | スロット n（0..3）ヘッダー 32 B: magic `0x4C534E50`（'PNSL'）u32、version 1 u16、pool_count 14 u8、k u8（0 = 自動）、state u8（0 空、1 書き込み中、2 確定）、予約 u8、件数 u16、名前 8 B（空白埋め）、予約 4 B、CRC16（先頭 24 B）u16、予約 6 B |
| ヘッダー + 32 + 60 × i | 記録: ラベル u8（1..4）、予約 u8、f32 × 14、CRC16（先頭 58 B）u16 |
| 0x3F000 | `selftest` の作業領域 |

CRC は CRC-16/CCITT-FALSE（UART と同じ）。書き込みはすべて読み返して比較する。記録 1 件でも CRC が合わなければ学習は失敗（`err=3`）し、黙って飛ばさない。

## UART プロトコル PST1

行形式・CRC は PAI1 と同じ（`本文*crc16hex`）。2 番目のフィールドは 10 進の id で、返信に同じ値が入る。返信は必ず `# PST1,` で始まり、エラーは `# PST1,ERROR,<理由>`。
INFO・SLOT・SELFTEST 以外は `PAI1,MODE,1`（取得停止）中だけ受け付ける（それ以外は `MODE_REQUIRED`）。

| コマンド | 返信 |
|---|---|
| `PST1,INFO,id` | `# PST1,INFO,id,<有効>,<c0>,<c1>,<c2>,<c3>`（空・無効は 0） |
| `PST1,SLOT,id,n` | `# PST1,SLOT,id,n,<state>,<件数>,<pool>,<k>,<名前>` |
| `PST1,OPEN,id,n,k,名前` | `# PST1,OPEN,id,n` |
| `PST1,PUT,id,n,ラベル,<112 桁 hex>` | `# PST1,PUT,id,n,<件数>`（f32 × 14、各 8 桁、上位桁から） |
| `PST1,COMMIT,id,n` | `# PST1,COMMIT,id,n,<件数>` |
| `PST1,GET,id,n,番号` | `# PST1,GET,id,n,番号,<ラベル>,<112 桁 hex>` |
| `PST1,ERASE,id,n` / `PST1,USE,id,n` | `# PST1,ERASE,id,n` / `# PST1,USE,id,n` |
| `PST1,TRAIN,id` | `# PST1,TRAIN,id,<ready 0/1>,<err>,<件数>,<選んだ数>,<i:i:…>,<学習 us>` |
| `PST1,SELFTEST,id` | `# PST1,SELFTEST,id,<結果>`（0 = 合格） |

理由: `MODE_REQUIRED, FIELDS, SLOT, STATE, LABEL, INPUT_SIZE, INPUT_HEX, NONFINITE, RANGE, FULL, INDEX, CRC_RECORD, STORE`、ほかに未知のコマンド `COMMAND`、PAI1 の処理中 `BUSY`。

## 制限

- 1 スロット最大 1000 件（領域は 1023 件分）、4 スロット、k ≤ 12。
- 起動時間は学習 1 ステップ約 1.5 ms（FeRAM 読み出し込み）で件数 × 4 ステップ分のびる（1000 件で約 6 秒）。その間ウォッチドッグは学習ループが更新する。
- 取得中（MODE 0）は FeRAM に書かない。
