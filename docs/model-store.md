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

**基準データ**: `docs/analysis/2026-10-24-fram-slot2-176/pressure-demo-training.json`（2026-10-24 にスロット 2 から吸い出した 176 件。TAIL 37 / BACK 33 / LEGS 69 / HEAD 37、実機 4 分割で 94.9%、k=4）。ボタンで追記して崩れたら `load --slot 3 --input <このファイル> --name 1023-176 --use` で戻せる。

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
- LCD 1 行目: 学習中は `LEARN<段階> 済/全`（例 `LEARN1  123/ 704`、0 = 統計、1 = 学習）。学習に成功するたび（起動、PAI1 MODE 0 復帰、SW3 のスロット切替、SW2 の TUNE 後）まず 2 秒間 `SLOT 2  176  k4 `（スロット番号、件数を右詰め 4 桁、選んだ数。10..12 は `k` なしの 2 桁で `SLOT 2  176  12 `）を出し、そのあと `READY`。調整済みで提案があれば `READY MORE <部位>`（例 `READY MORE HEAD`）。スロットが無ければ `NO MODEL`、その他の失敗は `AI ERROR`。
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
  混同行列（行 = 正解、列 = 判定）
        TAIL BACK LEGS HEAD
  TAIL    36    1    0    0
  BACK     0   33    0    0
  LEGS     0    0   65    4
  HEAD     0    0    4   33
  提案: 頭（HEAD）のデータを追加してください。HEAD → LEGS 4 件、LEGS → HEAD 4 件（件数 HEAD 37 対 LEGS 69）
  （件数の少ない部位に境界が不利になるので、まず HEAD を LEGS と同程度まで増やす）
READY: 176 records, 4 features selected (…), training …
```

提案は再現率が最低の部位 A と、A の行で誤りが最多の部位 B を両方向の件数つきで示す。A の件数が B より少なければ「まず A を B と同程度まで増やす」、そうでなければ「境界付近の押し方（弱め・位置違い）を増やす」と添える（最小二乗の学習器は件数の多いクラス側に境界が寄るため）。挑戦者が勝ったときの行は `chosen: 4 features width_ms, release_width_ms, peak, trough (beat champion by 5)`。shortlist の数は候補数から逆算、特徴名はプール順（Fisher 順ではない）、screen の分母は（件数 − 学習セット）× 2 パス。決勝の数はチャンピオンを含む。

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

## ボタン操作（PC なし）

ボードの押しボタン SW2..SW5（P50..P53、プルアップ、押すと L）を `board_buttons()` で読み、`app_buttons.c` が 30 ms 安定したら確定とする（チャタリング除去）。通常取得中だけ見る。PAI1/PST1 の検証モード中は無視し、状態も消す（押したまま MODE 0 に戻っても、一度離すまで何も起きない。起動時に押していた場合も同じ）。

| 操作 | 動作 | LCD 1 行目 | UART |
|---|---|---|---|
| SW2 を押す | 長押しの案内 | `HOLD 2s: TUNE   `（2 秒前に離すと元の `READY` などに戻る） | なし |
| SW2 を 2 秒押し続ける | 有効スロットで TUNE（PST1 TUNE と同じ、約 2 分）→ 再学習 → 取得再開。1 回押すと 1 回だけ（押したままでも繰り返さない） | `TUNE …` / `FINAL …` → `LEARN…` → `SLOT …` 2 秒 → `READY …` | `# PNET1,START`、終了時 `# PNET1,<ok>,<err>,…`（PST1 TUNE 返信の id より後ろと同じ並び）、`# PNEM1..4`、`# PneutouchAi live acquisition resumed; sequence restarts` |
| SW3 を短く押す（1 秒以内に離す） | 有効スロットの次（巡回、COMMITTED で 4 件以上）を有効にして再学習 → 取得再開 | `SLOT 1   141    `（スロット、件数を右詰め 4 桁）→ `LEARN…` → `SLOT …` 2 秒 → `READY …` | `# PNES1,slot=<n>,count=<件数>`、`# PNEM1..4`、resumed 行 |
| SW3（ほかに使えるスロットなし） | 何もしない | `SLOT 2 ONLY     `（今の有効スロット）を 2 秒 | `# PNES1,ERROR,NO_OTHER_SLOT` |
| SW3（ディレクトリ書込み失敗） | 何もしない | `STORE ERROR` を 2 秒 | `# PNES1,ERROR,STORE` |
| SW4 を短く押す（記録していないとき） | 有効スロットに**追記する記録**を始める。COMMITTED は OPEN に戻す（`pneu_store_reopen`、件数・名前・k・mask・tuned_k・再現率はそのまま）。OPEN ならそのまま続け、EMPTY（またはヘッダーが読めない）なら `BTN` で新しく開く。部位ごとの件数をラベル 1 巡で数える | `REC TAIL   2/  8`（部位、その部位の件数を右詰め 3 桁、`/`、全件数を右詰め 3 桁） | `# PNER1,START,<slot>,<件数>,<新規 0/1>` |
| SW4 を 2 秒押し続ける（記録していないとき） | 最初の EMPTY スロット（ヘッダーが読めないスロットも EMPTY 扱い）を `pneu_store_open(slot, 0, "BTN")` で開き、有効にして（`pneu_store_use`）記録を始める | `REC TAIL   0/  0` | `# PNER1,START,<slot>,0,1` |
| SW4 長押し（EMPTY なし） | 何もしない | `NO EMPTY SLOT   ` を 2 秒 | `# PNER1,ERROR,NO_EMPTY_SLOT` |
| 記録中に押して離す（押下イベント完了） | 今の部位で 14 候補値を記録（`pneu_store_put`）。`# PNEC1` の推定は記録前のモデルで続くが、LCD の結果表示の代わりに件数を出す | `+HEAD 39        `（部位と新しい件数）を 1 秒 → `REC …` | `# PNER1,REC,<slot>,<部位>,<全件数>` |
| 記録の書込み失敗（満杯・FeRAM 不良） / 特徴が範囲外 | 記録しない（記録の開始・終了で FeRAM を読み書きできないときも `STORE ERROR`） | `STORE ERROR     ` / `BAD PRESS       ` を 2 秒 | `# PNER1,ERROR,STORE` / `# PNER1,ERROR,POOL` |
| SW5 を短く押す（記録中のみ） | 次の部位 TAIL → BACK → LEGS → HEAD → TAIL。起動直後は TAIL、同じ起動中は最後の部位を覚えている | `REC BACK   2/  9` | なし |
| SW5 を 2 秒押し続ける（記録中のみ） | 最後の 1 件を取り消す（`pneu_store_undo`: 件数 −1、ヘッダーだけ書き直し）。今回の記録で足した分だけ | `UNDO TAIL 2     ` を 1 秒（取り消した記録の部位と残り件数） | `# PNER1,UNDO,<slot>,<部位>,<全件数>` |
| SW5 長押し（今回足した記録なし） | 何もしない | `NOTHING TO UNDO ` を 2 秒 | なし |
| SW4 を短く押す（記録中） | 確定（`pneu_store_commit`）→ 再学習 → 取得再開 | `LEARN…` → `SLOT 2  180  k4 ` 2 秒 → `READY …` | `# PNER1,STOP,<slot>,<件数>,<今回足した数>`、`# PNEM1..4`、resumed 行 |
| SW4 短押し（記録中、4 件未満） | 記録を続ける | `NEED 4 RECORDS  ` を 2 秒 | なし |
| 記録中の SW2 長押し / SW3 | 何もしない（TUNE とスロット切替は確定済みスロットが必要）。SW2 の `HOLD 2s` 案内も出さない | `STOP REC FIRST  ` を 2 秒 | なし |
| SW4 を 2 秒押し続ける（記録中） | 今回の記録を取り消す: 今回足した記録をすべて消し（`pneu_store_truncate` で開始時の件数に戻す、ヘッダーだけ書き直し）、新しく開いたスロット（SW4 長押し・EMPTY から）は消して（`pneu_store_erase`）前の有効スロットに戻す。COMMITTED から追記していたスロットは確定し直す。もともと OPEN（4 件未満）なら OPEN のまま。スロットが戻ったときと確定し直したときは再学習 → 取得再開 | （`LEARN…` →）`DISCARDED       ` を 2 秒 | `# PNER1,DISCARD,<slot>,<消した件数>`（再学習時は `# PNEM1..4`、resumed 行） |
| 記録中に PAI1/PST1 検証（MODE 1）に入る | 記録を終える: 4 件以上なら確定（通常の終了と同じ）、4 件未満なら OPEN のまま。MODE 0 に戻ると再学習するので、確定した場合はそのスロットで学習される | `VALIDATION` | `# PNER1,STOP,<slot>,<件数>,<今回足した数>` / `# PNER1,ABORT,<slot>,<件数>` |
| 記録外の SW5 | 何もしない | — | — |

- SW3 のスロット切替（ディレクトリの有効スロット）と SW2 の TUNE（スロットのヘッダーに mask を書く）は、直後に再学習して取得を最初からやり直すので、書込み中に取得が止まっても問題にならない。SW4 の記録開始・終了も同じ（開始時のラベル 1 巡の読み出しと reopen、終了時の commit と再学習）。
- **例外**: 記録中は取得を続けたまま FeRAM に書く。書くのは押下イベントが完了したときに 1 回だけ（記録 60 B とヘッダー 32 B、それぞれ書いて読み戻して比較、今の SPI 速度でおよそ 1 ms）で、サンプリングのループの中では書かない。SW5 の取り消しもヘッダー 32 B を 1 回書くだけ。
- 記録中も推定（`# PNEC1`）は記録前に学習したモデル（インスタンス 0）で続く。スロットのヘッダーは OPEN になるが、インスタンス 0 はそのまま。PAI1/PST1 の検証に入ると記録は先に終わる（上の表）。
- 記録中に電源が落ちると有効スロットは OPEN のまま残る。起動時（学習の前）に有効スロットが OPEN で 4 件以上なら確定して `# PNER1,AUTOCOMMIT,<slot>,<件数>` を送り、そのまま学習する。4 件未満なら何もしない（今までどおり `NO MODEL`。SW4 短押しで続きを記録できる）。
- 2 秒（記録の `+…`・`UNDO …` は 1 秒）の表示は `notice_until`（ミリ秒）を毎回の poll で確認して戻す。止まるのは TUNE と学習そのものだけ。記録中の待ち受け表示は `REC …` 行。
- ボタンの判定は 1 つの表で: 押した時刻を押しボタンごとに覚え、1 秒以内に離せば短押し、2 秒押し続けると長押し（押している間 1 回だけ）。1〜2 秒で離すと何もしない（SW2 は離すと案内を消す）。

### PC なしの学習手順

1. SW4 を 2 秒長押し（空きスロットに新しく記録）、または SW4 を短く押す（有効スロットに追記）。LCD は `REC TAIL   0/  0`。
2. SW5 を短く押して部位を選ぶ（TAIL → BACK → LEGS → HEAD）。
3. 恐竜のその部位を約 1 秒押して離す。1 回ごとに `+TAIL 1` などが出て 1 件記録される。間違えたら SW5 を 2 秒長押しで最後の 1 件を取り消す。
4. 部位ごとに 2〜4 を繰り返し、SW4 を短く押して終了（4 件以上で確定 → 再学習 → `SLOT …` → `READY`）。やり直したいときは SW4 長押しで今回の記録をすべて取り消し（`DISCARDED`、記録を始める前に戻る）。
5. SW2 を 2 秒長押しで TUNE（特徴の組を選び直してスロットに保存）。足りない部位は `READY MORE <部位>` で分かるので、SW4 短押しで追記する。

波形を見る学習ツール（PC）は今までどおり使えるが、なくても学習できる。
- PAI1 MODE 0 での復帰も同じ処理（`resume_live()`）になったので、resumed 行の前に `# PNEM1..4` が出る。

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
- 取得中（MODE 0）は FeRAM に書かない。例外は SW2（TUNE）、SW3（スロット切替）、SW4 の記録開始・終了で、どれも取得を止めてから書き、終わったら取得を最初から再開する。記録中の押下ごとの書込み（1 回 約 1 ms、サンプリングのループの外）だけは取得を止めない（上の「ボタン操作」）。
- スケールは母標準偏差 ×1。3σ では隠れ層が線形のままになり、振幅軸で中間にある TAIL を学習できなかった（2026-10-23 実機実験）。
