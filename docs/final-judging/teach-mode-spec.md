# 仕様: 「恐竜に教える」モード（teach mode）

状態: 提案（2026-10-14）。10/16 の効果検証（[plan-10days.md](plan-10days.md)）の結果で「追加学習」か「ゼロから教える」かを決めてから実装する。
実装は implementer エージェントに渡す。設計判断はこの文書で固定する。

## 目的

PC もファームウェアの書き換えもなしに、恐竜を押して部位を教え、Solist-AI がその場で学び直し、電源を切っても覚えていること。
審査で「追加学習まで単体で完結」（公式FAQ）を実演する。

## 使うハードウェア（すべて現在未使用）

| 資源 | ピン | 用途 |
|---|---|---|
| PSW1〜PSW4 | P50〜P53（`S_Input/Sw.c:9-11,36`） | 教える部位の指定: PSW1=TAIL, PSW2=BACK, PSW3=LEGS, PSW4=HEAD |
| DIP SW bit0 | P34（`Sw.c:34`） | ON で起動時に FRAM の学習結果を無視し、埋め込みデータから学習（工場出荷状態） |
| LED1〜3 | P54〜P56（`S_Output/smpl_common_led.c`） | LED1: 教え待ち点滅、LED2: 学習完了、LED3: FRAM 保存完了 |
| LCD 16文字 | I2CF0（`pressure_display.c`） | 「TEACH HEAD」「LEARNED HEAD 3」「SAVED」 |
| FRAM MB85RS2MTA | SoftSpi P80〜P85, WP P30（`S_Bsp/SoftSpi.c:22-39`, `Fram.c:22-24`） | Beta/P と教えたサンプルの保存 |

HX710B は P40/P42 で、衝突しない（`board.c:53-56`）。

## 操作の流れ

1. 待機中に PSW を1回押す → LCD「TEACH HEAD」、LED1 点滅。10 秒以内に恐竜を押さなければ解除。
2. 次の有効イベント（`PNEU_EVENT_COMPLETE`）が来たら、その12特徴量を指定ラベルで学習する。INVALID / TIMEOUT なら LCD「RETRY」で待ち続ける。
3. 学習後、同じ特徴量で推論し、LCD「LEARNED HEAD 3 (HEAD)」のように、通算回数と今の推論結果を 3 秒表示。
4. 学習結果（Beta/P）とサンプルを FRAM に書き、LED3 を点灯。
5. PSW を長押し（2秒）で、そのラベルの教え込みを連続モードにする（押すたびに学習、再度長押しで終了）。デモで「5回教える」をテンポよく見せるため。

UART には `# PNET1,<label>,<count>,<train_us>,<predicted>` を出し、demo-visualizer にも「教えました」を出せるようにする（表示側の変更は任意）。

## 学習の仕様

- 正規化（`model_mean` / `model_scale`）は**ビルド時の値を固定で使う**。追加学習で正規化を変えると過去の Beta/P と整合しなくなる。
- 1段目（instance 0）には常に学習させる。ラベルが HEAD/LEGS のときは専用モデル（instance 1）にも、入力5を正圧ピークの絶対値に差し替えた同じサンプルを学習させる（`pressure_model.c:146-152` と同じ変換）。
- 1サンプルあたりの `ODL_StartTrain` 回数（1〜4）と `forgettingFactor` は 10/16 の検証結果で決め、`config.py` → `pneu_config.h` に定数として置く。
- `ODL_Reset` は呼ばない（学習済み重みが消える: `docs/solist-ai-implementation.md:76-77`）。
- 学習中（`ODL_StartTrain` 〜完了待ち）は HX710B の取得を止め、終了後に `pneu_features_reset()` で検出器を初期化する。学習待ちは1回 20 ms 以内（`pressure_model.c:15-26`）で、ウォッチドッグは待ちループ内で更新する。
- `PAI1` 検証モードから戻るとき、現在は `pneu_model_init()` で固定モデルに戻している（`pressure_app.c:142-149`）。FRAM に学習結果があればそちらを復元する。

## FRAM レイアウト（案）

| オフセット | 内容 |
|---|---|
| 0x00000 | ヘッダー: magic `PNEU`, version, 埋め込みデータの SHA256 先頭4バイト（`pressure_model_data.h:2`）, Beta/P サイズ, サンプル数, CRC16 |
| 0x00100 | instance 0 の Beta（32×4×2 = 256 B）と P（32×32×2 = 2,048 B） |
| 0x01000 | instance 1 の Beta と P |
| 0x02000〜 | 教えたサンプル: {12 float, label, peak float, ms, crc} 約 64 B × 最大 1,000 件 |

- 起動時: ヘッダーの magic / version / SHA / CRC が一致し、DIP bit0 が OFF なら Beta/P を `ODL_SetWeightBeta/P` で復元して学習をスキップ（ベンダー実装 `S_AI/AIWeight.c:82-110` を流用）。不一致なら埋め込みデータから学習し、FRAM を初期化する。
- 埋め込みデータを作り直してビルドすると SHA が変わり、古い FRAM は自動的に無効になる。
- 書き込みは CRC 付きで、ヘッダーは最後に書く（途中で電源が落ちても旧状態が残る）。
- `FramWriteBlock` は戻り値がない（`S_Bsp/Fram.h`）。書いた直後に読み戻して一致を確認し、不一致なら LCD「FRAM ERROR」で保存を諦めて RAM 上の学習だけ続ける。
- 教えたサンプルは正規化前の生特徴12個と正圧ピーク（専用モデルの入力5に必要）を保存する。`tools/generate_pressure_model.py` と同じ形式にしておくと、PC に吸い出して次の埋め込みデータに使える。

## 「ゼロから教える」モード（10/16 で追加学習が効かなかった場合）

- DIP bit1 ON で、埋め込みデータを使わず Beta=0, P=I から起動（`pressure_model.c:68-75` の初期化のみ）。
- LCD「EMPTY: TEACH ME」。4部位それぞれ 6 回以上教えるまで推論結果を出さない。
- デモでは録画を倍速にして「最初は何も知らない恐竜が、教えると分かるようになる」を見せる。

## テスト

- `tests/test_pressure_model.c` に、FRAM をメモリで模した復元 → 推論一致のテストを追加（Beta/P の保存→復元で同じ入力に同じ出力）。
- `tests/test_pressure_app.c` に、PSW → イベント → 学習 → LCD 文字列 の状態遷移テスト。
- ヘッダー破損（CRC 不一致、SHA 不一致）で埋め込み学習にフォールバックするテスト。
- 実機: 電源 OFF→ON 後に教えた部位が当たること、DIP bit0 で初期化されること。

## 作業量の見積もり

| 項目 | 目安 |
|---|---|
| ボタン・LED・LCD の状態機械（`pressure_app.c` から分離した新ファイル） | 0.5 日 |
| 学習呼び出しの追加（instance 0/1） | 0.5 日 |
| FRAM 初期化、保存、復元、CRC、フォールバック | 1 日 |
| テスト | 0.5 日 |
| 実機検証・調整 | 1 日 |

RAM: Beta/P の一時バッファは既存の `reset_row[64]` と同程度。サンプル保存は FRAM に直接書くので RAM に溜めない。
