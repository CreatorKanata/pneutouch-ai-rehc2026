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
| k をチップ上の交差検証で決める | `model_store.py --port COM7 tune`（有効スロット。TUNE → TRAIN。k ごとの正解率、部位ごとの再現率と件数、「どの部位を追加で取るか」の提案を表示） |
| FeRAM 自己診断 | `model_store.py --port COM7 selftest`（0x3F000 の作業領域だけを書き換える） |

## 起動時に表示されるもの

- UART 起動行 `# PneutouchAi Live; …; fw=<ビルド名>`。`model_store.py info` の `firmware:` にも同じ名前が出る。ファームを変えたら `S_PneuTouch/firmware_tag.h` の名前を手で更新し、書き込み後に一致を確認する（古い .elf が書き込まれたままだと気づける）。
- UART: `# PNEM3,READY,slot=0,count=176,err=0,sel=0:4:10:13:12,fisher=45000:0:...`（fisher は ×1000 の整数、14 個、プール順）。`err`: 0 なし、1 FeRAM 異常、2 有効スロットなし、3 記録の CRC/値異常、4 選択不能、5 Solist-AI 異常。`# PNEM1` は従来どおり（件数、ステップ数、学習時間 us）、`# PNEM2,NONE,0,0,single_instance` は旧ツール互換のためだけに残す。
- UART: `# PNEM4,k=<tuned_k>,recall=<r1:r2:r3:r4>,counts=<n1:n2:n3:n4>,advice=<TAIL|BACK|LEGS|HEAD|NONE>`（PNEM3 の次の行）。tuned_k 0 = 未調整（recall は意味なし、advice は常に NONE）。recall は TUNE 時の held-out 再現率 %（255 = その部位の記録なし）、counts は今回の学習に使った部位ごとの件数（統計の前に失敗したら 0）。
- LCD 1 行目: 学習中は `LEARN<段階> 済/全`（例 `LEARN1  123/ 704`、0 = 統計、1 = 学習）、終わると `READY`。調整済みで提案があれば `READY MORE <部位>`（例 `READY MORE HEAD`）。スロットが無ければ `NO MODEL`、その他の失敗は `AI ERROR`。
- LCD 1 行目（TUNE 中）: `TUNE  7/20  57% `（終わった学習回数/全 20 回、今の段階の進み %）。各 fold の選択が決まるたびに 2 行目のセルが切り替わる。
- LCD 2 行目: 選ばれた特徴を黒■（0xFF）、選ばれなければ空白。右端 2 桁は選んだ数。選択が決まった時点（学習開始時）で表示する。

| 列 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14–15 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 特徴 | rise | decay | width | area_width | slope | asym | rel_rise | recovery | rel_width | rel_area_width | trough_ratio | area_ratio | peak | trough | 選んだ数 |

## 選択アルゴリズム（`feature_select.c`、PC 版 `tools/feature_select.py` と同じ float32 計算）

1. 各記録の 14 値（12 特徴、正圧ピーク、トラフの大きさ。すべて > 0）を `logf` する。
2. 1 回の読み出しでクラスごとの件数・和・二乗和をためる（RAM に記録を持たない）。
3. 特徴 j ごとに全体平均、クラス内平方和 W = Σ(二乗和 − 和²/n)、クラス間平方和 B = Σ n(クラス平均 − 全体平均)²。
4. Fisher 比 = B / W（W ≤ 0 なら 0）。母標準偏差 ×1 をスケールにし、スケール ≤ 1e-6 の特徴は 0 にして選ばない。
5. Fisher 比の降順に並べる（同点は番号の小さい方が先）。
6. k = 1..12 なら上位 k 個。k = 0（自動）なら「最大値の 1/10 以上」のものを全部、ただし 2 個以上 12 個以下。
7. 入力語 = bf16((log 値 − 平均) / スケール)、選んだ順に詰め、残りの入力は 0（Solist-AI の入力は 12 のまま）。
8. 学習順は記録を RAM に並べず、エポック e ごとに `(i × 素数_e + 37e) mod n`（素数は 3,5,7,11,… のうち n を割らないもの）で FeRAM から読み直す。4 エポック。
9. 隠れ 32、seed 1、シグモイド、MSE、Beta = 0・P = I から開始（従来どおり）。
10. 推論は同じ選択・平均・スケールで 1 回（Solist-AI インスタンス 0 のみ。HEAD/LEGS 専用モデルは廃止）。
11. 起動時の k: スロットの k（`load --k`）が 1..12 ならそれ、0 なら TUNE で決めた `tuned_k`、両方 0 なら手順 6 の自動選択。

## k の自動決定（交差検証、`model_tune.c`、PST1 TUNE）

- 記録 i の fold は `i mod 4`。候補 k = 4..8 を小さい順に、fold 0..3 ごとに「その fold 以外の記録だけ」で統計 → 選択（手順 1–7）→ 学習（手順 8、held-out の記録は飛ばす）→ held-out の記録を番号順にチップで推論（通常の推論と同じ経路）。学習は 5 × 4 = 20 回。
- k ごとに held-out の正解数を数え、最も多い k を選ぶ（同点は小さい k）。その k の 4×4 混同行列から部位ごとの再現率 % = round(100 × 正解 / 件数) を出す（記録の無い部位は 255）。
- 提案: 再現率が最も低い部位が 90% 未満ならその部位（同点は件数の少ない方、次に番号の小さい方）。全部 90% 以上なら提案なし。
- 結果（tuned_k と再現率）をスロットヘッダーに書く。失敗したら何も書かない。8 件未満のスロットは `NO_SLOT`。
- TUNE は Solist-AI インスタンス 0 を何度も学習し直すので、終わるとモデルは未学習状態（`ready = 0`）。PC ツールはすぐ TRAIN を送る。MODE 0 に戻っても再学習する。起動時間は変わらない（起動時は保存済みの tuned_k で 1 回学習するだけ）。
- TUNE は PAI1 のアイドル timeout（60 秒）より長くかかることがあるので、PST1 コマンドが終わった時点からアイドル時間を数え直す（TUNE 直後の TRAIN も MODE 1 のまま受け付ける）。
- `S_Bsp/SoftSpi.c` の高速化は FeRAM の読み書きにだけ効く。RTC（RX4111）の経路はこのファームでは使われない（`main.c` は `pressure_app` だけを呼ぶ）。
- `load` は新しく OPEN するので tuned_k は 0 に戻る。データを入れ替えたら `tune` をやり直す。
- 所要時間（推定、1 回の学習を記録数 × 4 ステップとして）: 176 件で 20 × (統計 132 件 + 学習 528 ステップ + 推論 44 件)。下の実測値で FeRAM 高速化前は 1 分強。

## FeRAM レイアウト（リトルエンディアン）

0x0000–0x0FFF は旧 `S_System/ConfigData.c` の領域なので触らない。

| アドレス | 内容 |
|---|---|
| 0x1000（16 B） | ディレクトリ: magic `0x54534E50`（'PNST'）u32、version 1 u16、有効スロット u8、予約 u8、CRC16（先頭 8 B）u16、予約 6 B |
| 0x2000 + n × 0xF000 | スロット n（0..3）ヘッダー 32 B: magic `0x4C534E50`（'PNSL'）u32、version 1 u16、pool_count 14 u8、k u8（0 = 自動）、state u8（0 空、1 書き込み中、2 確定）、tuned_k u8（バイト 9。0 = 未調整、1..12 = TUNE で決めた k）、件数 u16、名前 8 B（空白埋め）、recall u8 × 4（バイト 20..23。TAIL, BACK, LEGS, HEAD の held-out 再現率 %、255 = 記録なし。tuned_k ≠ 0 のときだけ意味がある）、CRC16（先頭 24 B）u16、予約 6 B。旧ヘッダーはこの位置が 0 なので「未調整」として読める（version 1 のまま） |
| ヘッダー + 32 + 60 × i | 記録: ラベル u8（1..4）、予約 u8、f32 × 14、CRC16（先頭 58 B）u16 |
| 0x3F000 | `selftest` の作業領域 |

CRC は CRC-16/CCITT-FALSE（UART と同じ）。書き込みはすべて読み返して比較する。記録 1 件でも CRC が合わなければ学習は失敗（`err=3`）し、黙って飛ばさない。

## UART プロトコル PST1

行形式・CRC は PAI1 と同じ（`本文*crc16hex`）。2 番目のフィールドは 10 進の id で、返信に同じ値が入る。返信は必ず `# PST1,` で始まり、エラーは `# PST1,ERROR,<理由>`。
INFO・SLOT・SELFTEST 以外は `PAI1,MODE,1`（取得停止）中だけ受け付ける（それ以外は `MODE_REQUIRED`）。

| コマンド | 返信 |
|---|---|
| `PST1,INFO,id` | `# PST1,INFO,id,<有効>,<c0>,<c1>,<c2>,<c3>,<fw>`（空・無効は 0。`fw` は `firmware_tag.h` のビルド名） |
| `PST1,SLOT,id,n` | `# PST1,SLOT,id,n,<state>,<件数>,<pool>,<k>,<名前>,<tuned_k>,<r1:r2:r3:r4>`（空・無効は 0 と 0:0:0:0） |
| `PST1,OPEN,id,n,k,名前` | `# PST1,OPEN,id,n` |
| `PST1,PUT,id,n,ラベル,<112 桁 hex>` | `# PST1,PUT,id,n,<件数>`（f32 × 14、各 8 桁、上位桁から） |
| `PST1,COMMIT,id,n` | `# PST1,COMMIT,id,n,<件数>` |
| `PST1,GET,id,n,番号` | `# PST1,GET,id,n,番号,<ラベル>,<112 桁 hex>` |
| `PST1,ERASE,id,n` / `PST1,USE,id,n` | `# PST1,ERASE,id,n` / `# PST1,USE,id,n` |
| `PST1,TRAIN,id` | `# PST1,TRAIN,id,<ready 0/1>,<err>,<件数>,<選んだ数>,<i:i:…>,<学習 us>` |
| `PST1,TUNE,id` | `# PST1,TUNE,id,<ok 0/1>,<err>,<件数>,<best_k>,<c4:c5:c6:c7:c8>,<r1:r2:r3:r4>,<n1:n2:n3:n4>,<混同行列 16 個（行 = 正解、列 = 推定、行優先）>,<提案 0..4>,<us>`（c = k ごとの held-out 正解数。失敗時も同じ形で、分かっていない値は 0） |
| `PST1,SELFTEST,id` | `# PST1,SELFTEST,id,<結果>`（0 = 合格） |

理由: `MODE_REQUIRED, FIELDS, SLOT, STATE, LABEL, INPUT_SIZE, INPUT_HEX, NONFINITE, RANGE, FULL, INDEX, CRC_RECORD, STORE`、ほかに未知のコマンド `COMMAND`、PAI1 の処理中 `BUSY`。

## 制限

- 1 スロット最大 1000 件（領域は 1023 件分）、4 スロット、k ≤ 12。
- 起動時間は件数 × 4 ステップの学習と件数分の統計でのびる。その間ウォッチドッグは学習ループが更新する。実測（48 MHz Cortex-M0+、-Oz、ソフト浮動小数点、UART ログから）:

| 項目 | 1 回あたり |
|---|---|
| Solist-AI 学習 1 ステップ（ODL_StartTrain → busy 解除） | 1.29 ms |
| Solist-AI 推論 | 0.11 ms |
| `pneu_store_get` 1 件（ヘッダー 32 B + 記録 60 B をビットバング SPI で読み CRC 確認） | 約 1.6 ms |
| 特徴 1 個の `logf` + 正規化（`pneu_select_words`） | 約 0.30 ms |

  つまり k = 4 の学習 1 ステップ ≈ 1.6 + 1.2 + 1.3 ≈ 4.1 ms、統計は 1 件 ≈ 5.8 ms（logf 14 回）。高速化として (1) `pneu_store_get` はヘッダーを 1 件分キャッシュして記録 60 B だけ読む、(2) `S_Bsp/SoftSpi.c` は 1 ビットにつき P8DO への単純書込み 2 回（読み-変更-書きと遅延ループを廃止）、(3) 正規化は割り算をやめ 1/scale を掛ける、を入れた。高速化後の数値は**要実測**。
- 取得中（MODE 0）は FeRAM に書かない。
- スケールは母標準偏差 ×1。3σ では隠れ層が線形のままになり、振幅軸で中間にある TAIL を学習できなかった（2026-10-23 実機実験）。
