<!-- Firmware entry point: import this complete folder into LEXIDE after preparation. -->
# PneuTouchSolistAI — Phase 0

DT-EBML63Q2557 / ML63Q2557でHX710Bを読み、CN9のUART Bから生値を送信する。
既定値は40 SPS、115200 baud / 8N1。AIは後続段階で追加する。

リポジトリ直下で`python3 tools/prepare_vendor.py`を実行すると、
`vendor/`と`generated/`が揃う。設定の原本はルートの`config.py`。
`generated/pneu_config.h`を直接編集せず、設定変更後に準備コマンドを再実行する。

このフォルダー全体、または`python3 tools/package_firmware.py`で作るZIPをWindowsへコピーし、
LEXIDEの`File > Import > General > Existing Projects into Workspace`から取り込む。
プロジェクト名は`PneuTouchSolistAI`。Debug / ReleaseともHEX生成を有効にしている。
配布デモのプロジェクトやDebug設定は上書きしない。

Debug / Releaseのリンカー入口はプロジェクト直下の`ML63Q25x7_lccarm.ld`。
このファイルから`generated/ML63Q25x7_lccarm.ld`を読み込み、メモリー配置は生成側に集約する。
入口ファイルもWindowsへのコピーとGit管理に含める。生成側を直接編集しない。

| ファイル | 役割 |
|---|---|
| `Source/board.c` | 電源保持、クロック、SysTick、WDT、GPIO、UART |
| `Source/hx710b.c` | 24 bit読取、符号拡張、25 / 27パルス |
| `Source/acquisition.c` | 初回・復帰後の安定待ち、タイムアウト |
| `Source/main.c` | PNEU1フレームを送信 |
| `.project`, `.cproject`, `.settings/` | LEXIDEのプロジェクト・ビルド設定 |
| `ML63Q25x7_lccarm.ld` | 生成したメモリー配置を読むリンカー入口 |
| `generated/vendor-manifest.json` | 展開元と生成物のSHA-256 |

配線: VCC→CN3-5（3.3V）、GND→CN3-11、SCK→CN3-12/P40、DOUT→CN3-10/P42。
JP1=1–2、開発中JP8短絡。モジュールの3.3V動作とピン順を実物で確認する。
通信はCN9、デバッガーはCN2。CN8は電源専用。

MacでのホストテストとArmオブジェクト生成は確認済み。
Windowsでの最終リンク・HEX生成、実機書き込み・計測は未確認。
取り込み後はClean Buildし、書き込み後にRun/Resumeして送信を開始する。
