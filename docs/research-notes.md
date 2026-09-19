<!-- Research ledger: sources, conclusions, and unresolved questions for reproducible implementation. -->
# 調査資料と重要な発見

調査日: 2026-09-20。資料に書かれた仕様、コードからの確認、実装の提案を区別する。
`references/`はGit除外なので、下記のローカル資料は別途手元に用意する。

## ローカル資料

| ID | `references/`内の資料 | 使用箇所 |
|---|---|---|
| H1 | `DT-EBML63Q2557_hardware_users_manual_Rev.20250527.pdf` | 電源、MCU、CN3、USB、SWD |
| D1 | `DT-EBML63Q2557デモソフトウェア開発手順書_Rev.20260316.pdf` | LEXIDE、デバッガー、ビルド手順 |
| D2 | `Solist-AIアイデアコンテスト配布キット_開発環境構築ガイド_260709.pdf` | 配布環境とツール導入 |
| D3 | `DT-EBML63Q2557対応ソフトウェアパッケージガイド_Rev.20260126.pdf` | AISignal / AIVibrationの違い |
| D4 | `DT-EBML63Q2557対応ソフトウェア移行ガイド_Rev.20260126.pdf` | 再構成ファームウェアを使う移行 |
| D5 | `DT-EBML63Q2557_firmware_writing_procedure_manual_Rev.20250528.pdf` | ファームウェア書き込み |
| A1 | `AISignalInferenceHost_users_manual_Rev.20260421.pdf` | 配布ホストのデータ・学習・推論操作 |
| A2 | `ai-hyper-parameters.pdf` | 異常検知の損失・活性化・初期値等 |
| A3 | `AIVibrationInference_users_manual_Rev.20250522.pdf` | デモの正常学習・異常度表示 |
| C1 | `AIVibrationInference`のLEXIDEプロジェクト | 起動、UART、電源保持、AI API |
| C2 | `ARM.CMSIS.5.9.0.pack` | Cortex-M0+のCMSISヘッダー |

D1 / D3 / D4は`REHC向けソフトウェア-2026-03-16/はじめに/`内。
D5はその中の`AISingalInferenceからAIVibrationInferenceへ移行する/`内。
A3は同配布物の`ソフトウェア取扱説明書/`内。
実際のファイル名にはUnicode結合文字があるため、スクリプトは正規化や探索で扱う。

C1の起点:

```text
references/REHC向けソフトウェア-2026-03-16/
  ソフトウェア/AIVibrationInferenceLexide/AIVibrationInferenceLexide/
  Workspace/AIVibrationInference/
```

## Webの一次資料・仕様資料

| ID | 資料 | 確認内容 |
|---|---|---|
| S1 | [AVIA HX710Bデータシート（販売店配布PDF）](https://bafnadevices.com/wp-content/uploads/2024/04/HX710B-SMD-Datasheet.pdf) | 10/40 SPS、25～27パルス、電圧、タイミング |
| S2 | [AVIA HX711データシート（SparkFun配布PDF）](https://cdn.sparkfun.com/datasheets/Sensors/ForceFlex/hx711_english.pdf) | RATE、10/80 SPS、ゲイン、差動入力範囲 |
| S3 | [MPS20N0040D-S参考仕様（SparkFun配布PDF）](https://cdn.sparkfun.com/assets/home_page_posts/1/9/0/2/Pressure_Sensor.pdf) | ブリッジ構成の参考。手持ち型番の同定には使わない |
| S4 | [TE MS4525DOデータシート、2019-04（国内代理店配布）](https://www.krone.co.jp/pdf/te/MS4525DO.pdf) | 更新時間、インターフェース、電源・型番体系 |
| W1 | [MDN Web Serial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API) | ブラウザーの制約、ポート選択、接続 |
| W2 | [MDN SerialPort.open](https://developer.mozilla.org/en-US/docs/Web/API/SerialPort/open) / [close](https://developer.mozilla.org/en-US/docs/Web/API/SerialPort/close) | baud・フロー制御・終了処理 |
| W3 | [pySerial API](https://pyserial.readthedocs.io/en/latest/pyserial_api.html) | シリアル設定、タイムアウト、受信 |
| W4 | [Arm CMSIS](https://arm-software.github.io/CMSIS_5/Core/html/index.html) | Cortex-Mコア、SysTick、割り込み制御 |

S1/S2/S4はメーカー作成資料の再配布。S3はメーカー・型番末尾の一致が不十分で、参考扱い。
Amazonの商品情報は入手候補・基板写真の確認に使い、IC仕様の根拠と混ぜない。
CMSIS / pySerialはContext7でもAPIを確認した。実装時は採用バージョンと実物を再照合する。

## 読んだコードと採用上の注意

| C1内のパス | 確認内容 / 使い方 |
|---|---|
| `.project`, `.cproject`, `.settings/` | LEXIDE構成、Debug/Release、LLVM、MCU指定 |
| `RTE/` | 起動、割り込み、リンカー、デバイス定義。既存ライセンス表示を保持 |
| `RTE/system_ML63Q25x7.c` | SystemCoreClock更新を確認。PLL変更後の値を明示的に整合させる |
| `S_System/main.c` | WDTとPLLを含む起動順 |
| `S_Driver/smpl_common.c` | PLL安定待ちとWDTクリア |
| `S_Output/PowerKeep.c` | P45を使った電源保持 |
| `S_LCD/Lcd.c` | P72の外部リセット解除 |
| `S_Uartf/Uart1.c` | P70/P71とUARTF1、9600と115200の設定差 |
| `S_Driver/solistAi.h` | ODL API、教師データ、入出力数、bfloat16変換 |
| `S_AI/AI.c` | 再構成学習と損失による異常検知 |

ROHMのドライバーにはROHMマイコン用途の使用条件がある。
配布パッケージ全体を公開リポジトリへコピーせず、必要なものをローカル展開する。
自作コードと元資料の出所・ライセンス表示を分けて残す。

## 発見1: 配布デモは7クラス分類器ではない

`S_AI/AI.c`では出力数を入力数と同じにし、`ODL_StartTrain(model, x, x)`で
入力を教師として学習する。推論後は`ODL_GetLoss`による損失を閾値と比較している。
これは再構成に基づく異常検知の構成で、恐竜の部位IDを返す処理ではない。

一方、`solistAi.h`には独立した`inputSize` / `outputSize`、教師ベクトル引数、
`ODL_GetResult`がある。そこで1モデル・4/7出力の教師あり構成を検証候補にする。
APIがあることだけで分類精度、対応サイズ、性能を確認したとはしない。

配布設定は`ODL_MAX_INST_NUM=2`、`ODL_MAX_INPUTS=256`、`ODL_MAX_UNITS=64`と
対応するライブラリー構成を照合して使う。マクロ名・定義は実装時に原本へ合わせる。
インスタンス数2はクラス数2という意味ではない。ライブラリーバイナリーの制約を
ヘッダーの数値変更だけで拡張できるとは考えない。

`ODL_ToBfloat16`にはint16入力とQ形式の指定がある。
24 bit ADCを単純にint16へキャストすると情報を失う。
baseline、スケール、クリップと変換形式を定義して学習・推論で一致させる。
bfloat16の格納型がint16であっても、数値の整数キャストで変換できるわけではない。

## 発見2: 時間窓と応答速度を同時に設計する

HX710Bの40 SPSでは1点約25 ms、128点で約3.2秒分。
原案の16点前＋112点後では、接触後約2.8秒を待つ構成になる。
即座に動画を返す体験には長い可能性があり、窓長・サンプル速度・精度を同時に比較する。
HX711の80 SPSでも128点は約1.6秒分。交換だけで即時反応が保証されるわけではない。

静圧だけでなく、オリフィスを通る過渡応答が位置推定の情報源。
穴径・配管・取得レートを変えたデータは区別して保存する。

## 未解決事項と解消方法

| 未確認事項 | 確認方法 / 段階 |
|---|---|
| 手持ちMPSモジュールの回路・3.3V動作・ピン順 | 実物印字、導通、電圧をM1前に確認 |
| HX711への変更可否 | RATEと励起・入力経路、飽和を測定。変更は未採用 |
| 現在のボードファームウェア / 再構成の必要性 | 実機の版をD3/D4と照合 |
| Windows側の新規プロジェクトの最終ビルド | LEXIDEでHEX・mapを生成 |
| USB UARTの実受信と実効レート | CN9/B側でMCU時刻と連番を保存 |
| 気密、各区画容量、穴の実径 | 造形後の検査・保持試験・比較測定 |
| 約20 cmの模型で40/80 SPSが十分か | 同条件の押下をレート別に収集 |
| 分類APIとAIメモリー容量 | M4の教師あり最小例で実機確認 |
| 4/7分類の精度・誤反応・遅延 | 別試行・別セッションの評価データで測定 |
| 動画形式と各Zoneへの対応 | ユーザー提供後に再生・応答を確認 |

判断が変わったら、この表と対応する設計・実装プランを同じコミットで更新する。
