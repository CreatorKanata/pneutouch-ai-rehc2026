# PneutouchAI HTMLデモ

Created by Kanata the Kid Creator.

実際の恐竜から受け取った圧力とSolistAIの推定結果を、動画・状態画像・波形で表示します。
推定は `src/pneutouch-solist` の実機で行い、HTML/JavaScriptがWeb SerialでUSB UARTを直接読みます。
Pythonは静的ファイルの配信だけを担当します。

## 起動

リポジトリ直下で実行します（Python標準ライブラリのみ）。

```powershell
python tools/serve_demo.py
```

1. Chrome / Edgeで <http://localhost:8001/demo-visualizer/> を開きます。
2. 「恐竜につなぐ」を押し、**USB Serial Port (COM7)** を選んで接続します。
3. 恐竜を約1秒押して離します。

現在の配線では、CN9のUSB SerialがCOM6/COM7、圧力受信は **COM7・115200 baud・8N1・フロー制御なし** です。
MCU-LINKのCOM4/COM5とは別です。ほかの計測画面・シリアルモニターはCOM7を切断してください。
LEXIDEのPneutouchAi Write / Debugでは、実機がRun中である必要があります。
ファイルを直接ダブルクリックせず、localhostのURLから開いてください。
既存の計測アプリ（port 8000）と別ポートなので、サーバー同士は共存できます。

## 表示

| 状態 | 画像 | 動画・波形 |
|---|---|---|
| 待機 | `normal.jpg` | `static.mp4`を無限ループ |
| 押下検出 | `pressed.jpg`（全体黄色） | 部位の確定を待つ |
| 推定確定 | 部位のJPEG（該当部位が赤） | 部位のMP4を5秒再生し、そのイベントの実測波形を表示 |
| 推定から3秒後 | `normal.jpg` | 動画は5秒まで継続、波形は次の推定まで残る |
| 動画再生から5秒後 | 状態画像のタイマーに従う | 待機動画を再開 |

現在のモデルは解放後の波形まで使います。黄色はファームウェアの押下検出時、赤は**解放後に推定が確定した時**です。
指が離れた瞬間を外部センサーで検出するものではありません。
次の押下で黄色に切り替わり、次の推定が確定すると前の動画を中断して最新の部位を再生します。
UNKNOWN、無効波形、タイムアウトでは部位の赤表示・新しい部位動画を出しません。
3秒間データが来ない場合や切断時は、状態と波形をリセットします。
音は初期状態でOFFです。画面右下の「音 OFF」でONにできます。

## 画像・動画の対応

設定は `assets.json`。画像は4:3、切り抜きや変形なしで表示します。

| 用途 | 画像（`images/demo/`） | 動画（`videos/demo/`） |
|---|---|---|
| 待機 | `normal.jpg` | `static.mp4` |
| 押下中 | `pressed.jpg` | — |
| HEAD | `head.jpg` | `head.mp4` |
| BACK | `back.jpg` | `back.mp4` |
| LEGS | `legs.jpg` | `legs.mp4` |
| TAIL | `tail.jpg` | `tail.mp4` |

ユーザー提供のJPEG6枚は1024×768、MP4全5本は1660×1244・約5.042秒です。
反応動画は5秒で待機に戻します。差し替え動画が5秒未満なら終端で戻ります。
`pushing.jpg` は `pressed.jpg` に改名しました。

## 波形と通信

- `PNEU1`: 実測ADC生値。受信順序を検査し、最大800サンプルを保持。
- `PNEE1 START`: 全体黄色。IDで後続の推定と対応付け。
- `PNEC1`: 実機のHEAD/BACK/LEGS/TAILに従って表示。ブラウザー内で再分類しません。
- `PNEF1`: 推定直後に届くイベント開始・終了時刻を使って表示区間を確定。

グラフは検出前約0.85秒から波形取得完了後約0.35秒までです。
縦軸は直前の基準値（検出前0.825〜0.325秒の中央値）を引いたADC counts、横軸は圧力による押下検出からの秒数です。
波形の平滑化や時間伸縮は行いません。基準区間を受信していなければ生値表示とし、一部のみの記録と明記します。
「波形取得完了」の線は指の解放時刻ではありません。現在のHEAD/LEGSの混同はモデルの制限として残ります。

## 検証

```powershell
node --test tests/demo.test.mjs tests/protocol.test.mjs tests/serial.test.mjs
python -m unittest discover -s tests -p test_demo_server.py -v
# PlaywrightとChromeがある環境。上記サーバーを起動して実行
node tests/demo-browser.cjs
```

2026-09-20に、Node 14件・Python 2件が成功。Chromeのブラウザー統合試験でも、
全JPEG/MP4の読込、実記録のUART再生、4部位の対応、3秒/5秒の独立した切替、
待機ループ、切断、スマートフォン幅の表示を確認しました。
試験用のUART入力は隔離したテストブラウザーだけに注入します。本番ページに自動再生データや疑似推定は含めません。
ブラウザー統合試験は配線や新しい実押下の評価を代替するものではありません。

テストの画面・結果は `build/demo-browser/` に出力します（Git管理対象外）。
