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
| 使う特徴の組をチップ上で決める | `model_store.py --port COM7 tune`（有効スロット。TUNE → TRAIN。2 段階の部分集合探索の結果、部位ごとの再現率と件数、「どの部位を追加で取るか」の提案を表示。下の「特徴の組の自動決定」） |
| FeRAM 自己診断 | `model_store.py --port COM7 selftest`（0x3F000 の作業領域だけを書き換える） |

## 起動時に表示されるもの

- UART 起動行 `# PneutouchAi Live; …; fw=<ビルド名>`。`model_store.py info` の `firmware:` にも同じ名前が出る。ファームを変えたら `S_PneuTouch/firmware_tag.h` の名前を手で更新し、書き込み後に一致を確認する（古い .elf が書き込まれたままだと気づける）。
- UART: `# PNEM3,READY,slot=0,count=176,err=0,sel=0:4:10:13:12,fisher=45000:0:...`（fisher は ×1000 の整数、14 個、プール順）。`err`: 0 なし、1 FeRAM 異常、2 有効スロットなし、3 記録の CRC/値異常、4 選択不能、5 Solist-AI 異常。`# PNEM1` は従来どおり（件数、ステップ数、学習時間 us）、`# PNEM2,NONE,0,0,single_instance` は旧ツール互換のためだけに残す。
- UART: `# PNEM4,k=<tuned_k>,mask=<hex4>,recall=<r1:r2:r3:r4>,counts=<n1:n2:n3:n4>,advice=<TAIL|BACK|LEGS|HEAD|NONE>`（PNEM3 の次の行）。tuned_k 0 = 未調整（recall は意味なし、advice は常に NONE）。mask は今使っている選択（ビット j = プール特徴 j、k や自動で選んだときも同じ形、失敗時 0000）。recall は TUNE 時の held-out 再現率 %（255 = その部位の記録なし）、counts は今回の学習に使った部位ごとの件数（統計の前に失敗したら 0）。
- LCD 1 行目: 学習中は `LEARN<段階> 済/全`（例 `LEARN1  123/ 704`、0 = 統計、1 = 学習）、終わると `READY`。調整済みで提案があれば `READY MORE <部位>`（例 `READY MORE HEAD`）。スロットが無ければ `NO MODEL`、その他の失敗は `AI ERROR`。
- LCD 1 行目（TUNE 中）: 段階 1 は `TUNE 37/198  57%`（"TUNE" + 済んだ学習数を右詰め 3 桁 + "/" + 全数 3 桁 + 空白 + % 3 桁 + "%"。全数 = 候補数 × 2 パス。198 回目は `TUNE198/198`）、段階 2 は `FINAL 3/6    57%`（済んだ候補 / 候補数、10 以上なら 2 桁ずつ `FINAL 10/12  57%`）。% = 今の段階（統計または学習）の進み。各回の始まり（段階 2/3 の通知そのもの）は列 12..15 が空白。最後の FINAL が終わると `READY` などに戻る。候補の選択が決まるたびに 2 行目のセルが切り替わる。
- LCD 2 行目: 選ばれた特徴を黒■（0xFF）、選ばれなければ空白。右端 2 桁は選んだ数（`k4` の形、10 以上は数字 2 桁）。選択が決まった時点（学習開始時）で表示する。

| 列 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14–15 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 特徴 | rise | decay | width | area_width | slope | asym | rel_rise | recovery | rel_width | rel_area_width | trough_ratio | area_ratio | peak | trough | 選んだ数 |

## 選択アルゴリズム（`feature_select.c`、PC 版 `tools/feature_select.py` と同じ float32 計算）

1. 各記録の 14 値（12 特徴、正圧ピーク、トラフの大きさ。すべて > 0）の自然対数をとる。`logf` ではなく表引きの `pneu_log`（`fast_log.c`）: 指数部 e と仮数の上位 8 ビットで log2(1 + i/256) の 257 項の表（`fast_log_table.h`、`tools/make_log_table.py` が生成）を引き、下位 15 ビットで線形補間、(e + log2 m) × 0.69314718f。補間誤差は最大約 2.2e-6、float32 の丸めを含めた logf との差は 1e-4..1e8 の掃引で最大 3.8e-6（試験の上限 4e-6）、2 のべき乗では厳密に e × ln2。ソフト浮動小数点の logf（約 0.30 ms）より桁違いに速い。PC 版 `fs.fast_log` は同じ表・同じ float32 の手順で**チップとビット一致**する（Python の np.log ではない）。
2. 1 回の読み出しでクラスごとの件数・和・二乗和をためる（RAM に記録を持たない）。
3. 特徴 j ごとに全体平均、クラス内平方和 W = Σ(二乗和 − 和²/n)、クラス間平方和 B = Σ n(クラス平均 − 全体平均)²。
4. Fisher 比 = B / W。W ≤ 0 で B > 0（クラス内のばらつきが無い完全な分離）なら `PNEU_FISHER_MAX` = 1e9 で最上位、W ≤ 0 で B = 0 なら 0。母標準偏差 ×1 をスケールにし、スケール ≤ 1e-6 の特徴は 0 にして選ばない。
5. Fisher 比の降順に並べる（同点は番号の小さい方が先）。
6. k = 1..12 なら上位 k 個。k = 0（自動）なら「最大値の 1/10 以上」のものを全部、ただし 2 個以上 12 個以下。
7. 入力語 = bf16((log 値 − 平均) / スケール)、選んだ順に詰め、残りの入力は 0（Solist-AI の入力は 12 のまま）。
8. 学習順は記録を RAM に並べず、エポック e ごとに `(i × 素数_e + 37e) mod n`（素数は 3,5,7,11,… のうち n を割らないもの）で FeRAM から読み直す。4 エポック。
9. 隠れ 32、seed 1、シグモイド、MSE、Beta = 0・P = I から開始（従来どおり）。
10. 推論は同じ選択・平均・スケールで 1 回（Solist-AI インスタンス 0 のみ。HEAD/LEGS 専用モデルは廃止）。
11. 起動時の選択: スロットの k（`load --k`）が 1..12 なら上位 k 個。0 でヘッダーの mask ≠ 0（TUNE 済み、version 2）なら mask の特徴をちょうど（スケールが使えるものだけ、Fisher 順。使えるのが 2 個未満なら `err=4`）。mask = 0 で tuned_k ≠ 0（version 1 のスロット）なら上位 tuned_k 個。どれも 0 なら手順 6 の自動選択。

## 特徴の組の自動決定（2 段階の部分集合探索、`model_tune.c` + `model_screen.c`、PST1 TUNE）

なぜ: 2026-10-24 の実機実験で、PC の安い代理指標（LDA の散布比、最近傍重心の LOO）で「最良」の組がチップ上の Solist-AI（hard-sigmoid ELM）では負けた。組の良し悪しはチップの学習器そのもので測る。1 候補のコストは学習（1.29 ms/ステップ）と logf が支配するので、log は表引きにし（上の手順 1）、小さな学習セットで多くの候補をふるい、4 分割の交差検証は少数の決勝だけに使う。Fisher 自動選択（上位 k）をチャンピオンとし、挑戦者は余裕をもって勝たなければ採用しない。

0. 有効スロット（確定、8..1000 件）。全記録で統計 → Fisher 順位。ショートリスト = Fisher 比 > 0 の上位 `PNEU_TUNE_SHORTLIST` = 7 個（足りなければ少なく、3 個未満は `err=4`）。チャンピオン = 手順 6 の自動選択（k = 0）のマスク。
1. ふるい分け用の学習セット（パス p = 0, 1）: 部位 c の記録が n_c 件のとき、n_c ≤ 10 なら全部。そうでなければ部位内の通し番号（スロット順、0 から）が `offset_c + j × n_c / 10`（j = 0..9、整数除算）の 10 件。`offset_c = p × ceil(n_c / 20)`（切り上げなので 11..20 件の部位でもパス 0 と 1 が違う組になる。例 n_c = 12: パス 0 は 0,1,2,3,4,6,7,8,9,10、パス 1 は 1,2,3,4,5,7,8,9,10,11）。125 B のビットマップ 2 枚（パスごと）。
2. 候補: ショートリストの 3..7 個の部分集合すべて（7 個なら 35 + 35 + 21 + 7 + 1 = 99 個）。大きさの昇順、同じ大きさの中ではショートリスト上のビット値の昇順。各候補・各パスで、全記録の統計から mask の特徴を選び（Fisher 順）、学習セットだけで 4 エポック学習（学習順は手順 8、セット外は飛ばす）、セット外の全記録をチップで推論して正解数を足す。上位 `PNEU_TUNE_FINALISTS` = 5 個を（正解数の多い順、同点は特徴の少ない方、次にマスク値の小さい方）で残す。
3. 決勝: チャンピオン + 決勝候補（チャンピオンと同じマスクは飛ばし、その決勝値はチャンピオンの値）。それぞれ 4 分割の交差検証（記録 i の fold は `i mod 4`、統計は学習 fold だけ、mask の特徴を Fisher 順、held-out を番号順に推論）。
4. 決定: 挑戦者 = 決勝で正解数最多の候補（同点は特徴の少ない方、次にマスク値の小さい方）。余裕 = max(1, round(件数 × 2 / 100))（176 件なら 4）。挑戦者の正解数 > チャンピオン + 余裕のときだけ挑戦者、それ以外はチャンピオン。選んだ方の 4×4 混同行列から部位ごとの再現率 % = round(100 × 正解 / 件数)（記録の無い部位は 255）。
5. 提案: 再現率が最も低い部位が 90% 未満ならその部位（同点は件数の少ない方、次に番号の小さい方）。全部 90% 以上なら提案なし。
6. 結果（mask、tuned_k = mask のビット数、再現率）をスロットヘッダー（version 2）に書く。失敗したら何も書かない。8 件未満のスロットは `NO_SLOT`。

- 進捗: 段階 1 は phase 2（済んだ候補 × パス / 候補数 × 2）、段階 2 は phase 3（`PNEU_PHASE_FINAL`、済んだ決勝 / 決勝数）。それぞれ開始時（0）と 1 回ごと。中の統計・学習は phase 0/1 で従来どおり。
- 所要時間（推定、176 件、**要実測**）: 段階 1 ≈ 99 候補 × 2 パス × (160 学習ステップ × 約 1.85 ms + 136 推論 × 約 0.7 ms) ≈ 80 秒、段階 2 ≈ 6 × 約 4.5 秒 ≈ 27 秒、合計 約 2 分。PC ツールの timeout は 900 秒。
- TUNE は Solist-AI インスタンス 0 を何度も学習し直すので、終わるとモデルは未学習状態（`ready = 0`）。PC ツールはすぐ TRAIN を送る。MODE 0 に戻っても再学習する。起動時間は変わらない（起動時は保存済みの mask で 1 回学習するだけ）。
- TUNE は PAI1 のアイドル timeout（60 秒）より長くかかるので、PST1 コマンドが終わった時点からアイドル時間を数え直す（TUNE 直後の TRAIN も MODE 1 のまま受け付ける）。
- `S_Bsp/SoftSpi.c` の高速化は FeRAM の読み書きにだけ効く。RTC（RX4111）の経路はこのファームでは使われない（`main.c` は `pressure_app` だけを呼ぶ）。
- `load` は新しく OPEN するので mask と tuned_k は 0 に戻る。データを入れ替えたら `tune` をやり直す。

`tune` の表示例（数値は例）:

```
tune: 176 records, shortlist 7 features, 99 subsets screened (10/class × 2 passes), 5 finalists (113.2 s)
  champion (top-4): release_width_ms, release_area_width_ms, peak, trough  4-fold 167/176 = 94.9%
  finalists:  3 features width_ms, peak, trough   screen 258/272  4-fold 165/176
              4 features width_ms, release_width_ms, peak, trough   screen 255/272  4-fold 170/176
              …
  chosen: champion (no challenger beat it by more than 4 records)
  TAIL 97% (37)  BACK 100% (33)  LEGS 94% (69)  HEAD 89% (37)
  提案: 頭（HEAD）のデータを追加してください（足（LEGS）と混同 4/37）
READY: 176 records, 4 features selected (…), training …
```

挑戦者が勝ったときの行は `chosen: 4 features width_ms, release_width_ms, peak, trough (beat champion by 5)`。shortlist の数は候補数から逆算、特徴名はプール順（Fisher 順ではない）、screen の分母は（件数 − 学習セット）× 2 パス。決勝の数はチャンピオンを含む。

## FeRAM レイアウト（リトルエンディアン）

0x0000–0x0FFF は旧 `S_System/ConfigData.c` の領域なので触らない。

| アドレス | 内容 |
|---|---|
| 0x1000（16 B） | ディレクトリ: magic `0x54534E50`（'PNST'）u32、version 1 u16、有効スロット u8、予約 u8、CRC16（先頭 8 B）u16、予約 6 B |
| 0x2000 + n × 0xF000 | スロット n（0..3）ヘッダー 32 B（下の表）。書き込みは常に version 2、読み込みは version 1 も受け付ける |
| ヘッダー + 32 + 60 × i | 記録: ラベル u8（1..4）、予約 u8、f32 × 14、CRC16（先頭 58 B）u16 |
| 0x3F000 | `selftest` の作業領域 |

スロットヘッダー（version 2）:

| バイト | 内容 |
|---|---|
| 0–3 | magic `0x4C534E50`（'PNSL'）u32 |
| 4–5 | version u16（2） |
| 6 | pool_count u8（14） |
| 7 | k u8（0 = 自動、1..12 = `load --k`） |
| 8 | state u8（0 空、1 書き込み中、2 確定） |
| 9 | tuned_k u8（0 = 未調整。version 2 では mask のビット数） |
| 10–11 | 件数 u16 |
| 12–19 | 名前 8 B（空白埋め） |
| 20–23 | recall u8 × 4（TAIL, BACK, LEGS, HEAD の held-out 再現率 %、255 = 記録なし。tuned_k ≠ 0 のときだけ意味がある） |
| 24–25 | mask u16（ビット j = TUNE が選んだプール特徴 j。0 = なし。ビット 14/15 や 13 個以上は不正ヘッダー） |
| 26–29 | 予約 0 |
| 30–31 | CRC16（先頭 30 B） |

version 1（`na6-k` まで）のヘッダーは 24–25 が CRC16（先頭 24 B）で mask が無い。そのまま読めて mask = 0（tuned_k があれば上位 tuned_k 個で起動）、次にヘッダーを書いたとき（TUNE など）に version 2 になる。

CRC は CRC-16/CCITT-FALSE（UART と同じ）。書き込みはすべて読み返して比較する。記録 1 件でも CRC が合わなければ学習は失敗（`err=3`）し、黙って飛ばさない。

## UART プロトコル PST1

行形式・CRC は PAI1 と同じ（`本文*crc16hex`）。2 番目のフィールドは 10 進の id で、返信に同じ値が入る。返信は必ず `# PST1,` で始まり、エラーは `# PST1,ERROR,<理由>`。
INFO・SLOT・SELFTEST 以外は `PAI1,MODE,1`（取得停止）中だけ受け付ける（それ以外は `MODE_REQUIRED`）。

| コマンド | 返信 |
|---|---|
| `PST1,INFO,id` | `# PST1,INFO,id,<有効>,<c0>,<c1>,<c2>,<c3>,<fw>`（空・無効は 0。`fw` は `firmware_tag.h` のビルド名） |
| `PST1,SLOT,id,n` | `# PST1,SLOT,id,n,<state>,<件数>,<pool>,<k>,<名前>,<tuned_k>,<r1:r2:r3:r4>,<mask hex4>`（空・無効は 0 と 0:0:0:0 と 0000） |
| `PST1,OPEN,id,n,k,名前` | `# PST1,OPEN,id,n` |
| `PST1,PUT,id,n,ラベル,<112 桁 hex>` | `# PST1,PUT,id,n,<件数>`（f32 × 14、各 8 桁、上位桁から） |
| `PST1,COMMIT,id,n` | `# PST1,COMMIT,id,n,<件数>` |
| `PST1,GET,id,n,番号` | `# PST1,GET,id,n,番号,<ラベル>,<112 桁 hex>` |
| `PST1,ERASE,id,n` / `PST1,USE,id,n` | `# PST1,ERASE,id,n` / `# PST1,USE,id,n` |
| `PST1,TRAIN,id` | `# PST1,TRAIN,id,<ready 0/1>,<err>,<件数>,<選んだ数>,<i:i:…>,<学習 us>` |
| `PST1,TUNE,id` | `# PST1,TUNE,id,<ok 0/1>,<err>,<件数>,<mask hex4>,<best_k>,<正解数>,<チャンピオン mask hex4>,<チャンピオン正解数>,<決勝候補 m1=s1=c1:m2=s2=c2:…>,<候補数>,<1 パスで判定した件数>,<r1:r2:r3:r4>,<n1:n2:n3:n4>,<混同行列 16 個（行 = 正解、列 = 推定、行優先）>,<提案 0..4>,<us>`（mask・best_k・正解数・混同行列は選んだ組の 4 分割の結果。決勝候補は ふるいの順に `マスク=ふるいの正解数=4 分割の正解数`。失敗時も同じ形で、分かっていない値は 0、決勝候補は空） |
| `PST1,SELFTEST,id` | `# PST1,SELFTEST,id,<結果>`（0 = 合格） |

理由: `MODE_REQUIRED, FIELDS, SLOT, STATE, LABEL, INPUT_SIZE, INPUT_HEX, NONFINITE, RANGE, FULL, INDEX, CRC_RECORD, STORE`、ほかに未知のコマンド `COMMAND`、PAI1 の処理中 `BUSY`。

## 制限

- 1 スロット最大 1000 件（領域は 1023 件分）、4 スロット、k ≤ 12、mask は 12 個まで。
- 起動時間は件数 × 4 ステップの学習と件数分の統計でのびる。その間ウォッチドッグは学習ループが更新する。実測（48 MHz Cortex-M0+、-Oz、ソフト浮動小数点、UART ログから）:

| 項目 | 1 回あたり |
|---|---|
| Solist-AI 学習 1 ステップ（ODL_StartTrain → busy 解除） | 1.29 ms |
| Solist-AI 推論 | 0.11 ms |
| `pneu_store_get` 1 件（ヘッダー 32 B + 記録 60 B をビットバング SPI で読み CRC 確認） | 約 1.6 ms |
| 特徴 1 個の `logf` + 正規化（`pneu_select_words`） | 約 0.30 ms（`ss1-search` で表引き `pneu_log` に置換。置換後は**要実測**） |

  つまり logf のころは k = 4 の学習 1 ステップ ≈ 1.6 + 1.2 + 1.3 ≈ 4.1 ms、統計は 1 件 ≈ 5.8 ms（logf 14 回）。高速化として (1) `pneu_store_get` はヘッダーを 1 件分キャッシュして記録 60 B だけ読む、(2) `S_Bsp/SoftSpi.c` は 1 ビットにつき P8DO への単純書込み 2 回（読み-変更-書きと遅延ループを廃止）、(3) 正規化は割り算をやめ 1/scale を掛ける、を入れた。高速化後の数値は**要実測**。
- 取得中（MODE 0）は FeRAM に書かない。
- スケールは母標準偏差 ×1。3σ では隠れ層が線形のままになり、振幅軸で中間にある TAIL を学習できなかった（2026-10-23 実機実験）。
