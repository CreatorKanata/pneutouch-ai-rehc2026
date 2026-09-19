<!-- Integration notes: adapt vendor examples to DT hardware and keep protocols explicit. -->
# 追加サンプルの移植・PC連携メモ

調査日: 2026-09-20。N番号と`REF` / `ANOM` / `UART`は[資料索引](reference-catalog.md)に対応する。
配布物は参考として読み取り、実行・書き込み・変更はしていない。
ここではM1からM4へ進む際の流用箇所と、事前に修正・検証すべき点を整理する。

## RBボード用のコードをDTボードへ移す

ROHMサンプルのRB-D63Q25系と、手持ちのDT-EBML63Q2557は別ボード。
MCUが同じでも、USB・電源保持・外部デバイス・ピン割当は一致しない。
根拠: N10 PDF p.6–7、N5 PDF p.4、N7の`board/RB_D63Q25.c`、既存資料H1/C1。

| 項目 | ROHM参考実装 | PneuTouch / DTでの扱い |
|---|---|---|
| UART | UARTF0、P20=RX / P21=TX | CN9のFT2232H B側はUARTF1、P70=RX / P71=TX |
| UART割り込み | UAF0とUART0用HAL | ピンだけでなく周辺許可、初期化、IRQ、送受信呼出しをUARTF1へ合わせる |
| LED | P50/P51/P52 | DTの回路と照合。参考サンプルの点灯位置を動作判定へ使わない |
| 電源・外部リセット | RBの初期化 | DTのP45電源保持、P72外部リセット解除を維持 |
| センサー | KXTJ3等のI2C/SPI加速度 | CN3 P40/P42のHX710BをGPIOで読む |
| クロック | RBには40 MHz/32.768 kHz水晶。サンプルには48 MHz PLL設定あり | DTの32.768 kHz→48 MHzを基準に実際の設定・時刻を照合 |
| 保存先 | AI RAM→プログラムFlashの例あり | DTのFeRAMを使う保存設計と区別 |

M1は既存DTデモC1のボード初期化を参考にする。
M4でAIライブラリーを追加する際に、N7のAI/UART処理から必要部分を組み込む。
ROHMのHEXをそのままDTへ書けば動く、とは扱わない。

## 取得・前処理・AIの分離を参考にする

N4の`ANOM/samples/Solist-AI/AnomalyDetectionDemo/`には次の分割がある。

| ファイル | 参考になる構造 | HX710B向けの変更点 |
|---|---|---|
| `board/SensorSettings.h` | Init/Start/Stop/GetValueでセンサーを選ぶ | 圧力用ドライバーを独立させる。単なるマクロの差替えでは不十分 |
| `src/SensorControl.c` | 2つの取得バッファを交代し、完成した窓を渡す | int16加速度からint32圧力へ。DOUT準備完了・取得時刻・欠測を扱う |
| `src/Preprocess.c` | 固定小数点→bfloat16、FFT、完了確認を分離 | 圧力のbaseline・スケール・bin選択を定義する |
| `src/MlTask.c` | 停止/学習/推論と、取得/前処理/AI待機の状態を分離 | 正常再構成の教師を、イベントのZone教師へ置換する |

N3の800 Hz取得例は加速度向けの設定で、HX710Bを800 SPSにする方法ではない。
HX710Bは新しい変換ができた時だけ読み、同じ値の再読取りでサンプル数を増やさない。
参考コードの2バッファにも処理遅延時の上書き防止を自動で期待せず、所有状態・連番・欠落を検証する。
逐次学習の「正常なら自動更新」を分類へそのまま持ち込まず、正解ラベルが確認されたイベントで学習する。

## 3種類のPC連携を区別する

| 組合せ | 通信・データ | 用途と互換性 |
|---|---|---|
| PneuTouch Phase 0 + learning-tool | 115200、8N1、ASCII `PNEU1` | 圧力の生値・時刻・連番。既定方針を維持 |
| N7 SolistAI_UART_IF + 付属Python | 115200、**8E1**、バイナリー要求/応答 | PCの入力・教師をMCUで学習/推論。PNEU1とは別形式 |
| DT AIVibrationInference + N13 Host | DT独自の設定・評価データ・重み操作 | 対応するDTファームウェア用。上記2種類と互換とは確認できない |

M1で既存プロトコルを変更する必要はない。
M4の検証時は別の検証用ファームウェア/通信モードとして明示し、後続の双方向通信仕様を確定する。
同じストリームへASCIIログとバイナリー応答を無条件に混在させない。

N6で参考になる通信仕様:

- PDF p.4–6: 8E1、フロー制御なし。コマンド・可変長データ・加算チェックサムで構成。
  フレーム全体の和の下位8 bitが0になる。資料上の応答/受信間隔タイムアウトは100 ms。
- PDF p.12: モデル数設定はAI設定より前。資料上は最大4モデルで、7分類=7モデルではない。
- N7の`SolistAi.py`: 入力/教師/重みを最大255要素ずつ分割、16 bit値をlittle-endianで転送。
  学習・推論開始後に状態を確認して結果を読む。
- PCラッパーは読み取りタイムアウト1秒、応答不正時の再送を実装している。
  仕様書の100 msと同一ではない。学習更新の再送による二重実行も設計・テスト対象にする。

これらは新しい通信の参考になるが、チェックサム・再送を追加するだけで既存PNEU1との互換性は得られない。

## 付属Python・LEXIDE設定の要修正箇所

以下は配布ソースで確認した問題。原本は変更せず、採用時に自作の適応層で修正する。
対象Python: `UART/samples/Solist-AI/SolistAI_UART_IF/script/SolistAi.py`。
行番号は今回の配布物の値で、更新後は関数名でも照合する。

| 箇所 | 確認した挙動 | 採用前の修正・テスト |
|---|---|---|
| `Open`、L94–104 | 既定説明名`USB Serial Port`を探索し、見つからないと`COM12`。`port.name`使用 | 明示的なポート指定、Macのデバイスパス、FT2232H B側識別、未発見エラー |
| `TrainAfterFft`、L339–340 | `Train`に未定義キーワード`__command`を渡しTypeError | 内部メソッド呼出しを修正し、0x13のフレームと応答をテスト |
| `ResetWeight`、L365–371 | 長さ5のbytearrayの`[2:3]`へ2 byteを代入し、長さ6になる | seedを2 byte幅へ格納し、N6 PDF p.19の5 byte形式へ一致させる |
| `LoadModel`、L463–475 | `__init__(n,m,o,af,lf,seed)`のseedが現シグネチャの`l2param`位置に入る | 名前付き引数にし、seed・正則化・その他設定の復元を確認 |
| `SaveModel`、L434–461 | 保存する設定にl2Param、scaleAlpha/Gamma、leakRate等が含まれない | 前処理とSDK版も含む保存形式を定義し、往復で一致確認 |
| N4/N7の`.cproject` | Debugは`SolistAi_Library.a`、Releaseは旧`SolistAi_Library_2_256_64.a`を参照。旧名のファイルは各配布物にない | 同じSDK一式へ揃え、Debug/Release両方をClean Build |

`.cproject`の対象は`ANOM/samples/Solist-AI/AnomalyDetectionDemo/.cproject`と
`UART/samples/Solist-AI/SolistAI_UART_IF/.cproject`。
相対ライブラリーパスはDebug/Releaseビルドフォルダー基準で照合した。

**検証済みの範囲:** ソースから対象関数のみを切り出したモック検証とAST/XML検査で、
FFT学習のTypeError、リセット6 byte化、seedの誤対応、Release参照先欠落、N4/N7ライブラリー同一性の
5項目を確認した。シリアルポート・配布アプリは起動していない。
これは問題の再現確認であり、修正版やファームウェアが実機で動いたという意味ではない。

## AIVibrationInference / Hostから参考にすること

N13はWindows 11 64 bit、.NET 8.0-windows、RAM16 GB以上等を動作環境に挙げる（PDF p.6）。
Macでその`.exe`を直接使う前提にはせず、利用するならParallels側へUSB通信を割り当てる。
HX710B用PneuTouchファームウェアのホストとしてそのまま接続することはできない。

| 機能 / 仕様 | 根拠 | PneuTouchへの反映 |
|---|---|---|
| 入力ソースと取得条件を明示 | N13 PDF p.11–14。内蔵ADC/加速度、100～25,600 Hz等 | センサー型番・実測SPS・前処理を表示/保存。値をHX710Bへ転用しない |
| 生波形・CSV | N13 PDF p.33–36、高速リアルタイム転送 | learning-toolに波形とCSV。さらに時刻・連番・ラベルを付ける |
| FFTと異常度のブロック表示 | N13 PDF p.24–26 | 生データとAI入力・出力を別に表示。ブロックCSVを生波形と誤認しない |
| 重みのPC保存/読込 | N13 PDF p.30–32 | 保存形式・版整合・復元検証のUXを参考にする |
| 学習/推論/停止・エラー表示 | N14 PDF §7 | MCU状態を画面へ伝える。通信だけつながった状態と区別 |
| シャットダウンで重み保存 | N14 PDF p.35 | 自作アプリでも保存完了と電源操作を設計する |

N13の「FFTなし入力最大256、FFTあり512」「入力層=出力層」はAIVibrationの異常検知設定の制約。
新しいSolist-AI SDKや7分類全体の上限とは別である。
N14の正常/黄色/赤色は異常度の判定で、3つの触覚部位を学習する分類例ではない。
重み書込み後の電源操作は配布アプリ固有なので、その操作手順を自作アプリへ無条件に適用しない。

## Solist-AI ScopeとSimの位置づけ

N3のScope連携はFT232H/FT2232H等の**SPI**経由でMCUの変数を参照し、ビルドのmapを使う方式。
UART波形表示の代替として、ポート名だけ変えて使えるものではない。
DTにもFT2232Hはあるが、ピン接続・HAL・競合する周辺機器・同一HEXに対応するmapを照合する必要がある。
PneuTouchではまずUARTで取得し、ScopeはAI内部変数の調査が必要になった時の追加手段にする。

追加されたSimインストーラーは`Solist-AI_Sim_AnomalyDtection_3`内にある異常検知版。
教師あり4分類サンプルが参照するデータ・機能と同じものが使えるとは未確認。
インストールや起動は未実施。MCU用Python例を使う検証とは別に管理する。

## 実装時の順序

1. M1: DT用起動・GPIO・HX710B・8N1/PNEU1を実装し、実測CSVまで通す。
2. M2/M3: 圧力の部位差と時間窓を評価する。加速度用前処理を先に固定しない。
3. M4: N7の依存版を固定し、ボードHAL・Release設定・Python不整合を修正して最小教師あり例を動かす。
4. モデル保存、前処理一致、コマンド失敗・再送・タイムアウトを確認する。
5. 4→7分類へ進め、推論イベントをdemo-visualizerの強調表示・動画へ接続する。

WindowsでのHEX生成、USB実通信、7区画の判別はまだ検証していない。
参考コードの調査結果と、今後の実機合格条件を混同しない。
