# 実機の接続記録

更新: 2026-09-20。現在のWindows環境で確認した接続。COM番号は別PCや再割り当てで変わる。

| 機器 | ポート | 用途・確認方法 |
|---|---|---|
| MCU-LINK側 | COM4 / COM5 | ユーザー申告。書き込み・デバッグはLEXIDEのCMSIS-DAP接続を使う。空気圧データの受信先には選ばない |
| CN9 / FT2232H チャネルA | COM6 | Windowsで `DT-EBMLC5W5R5A` を確認。ボード側SPI用 |
| **CN9 / FT2232H チャネルB** | **COM7** | Windowsで `DT-EBMLC5W5R5B` を確認。**空気圧データのUART受信先。115200 baud / 8N1 / フロー制御なし** |

CN9のUSB IDはVID=0403 / PID=6010。COM5はMCU-LINKのVID=1FC9 / PID=0143、
COM4はVID=291A / PID=8355として認識されている。COM4の機器名はユーザー申告と区別して記録する。

## センサーと造形物

- MPS20N0040D＋HX710Bの4ピンモジュール。
- 恐竜の人形は完成済み（ユーザー申告）。
- 2026-09-20、ユーザーから接続完了の連絡あり。受信と押している間の生値変化を確認。ピン位置・電圧の直接測定と気密評価は別途。
- 配線: VCC→CN3-5、GND→CN3-11、SCK→CN3-12/P40、DOUT→CN3-10/P42。
- JP1=1–2（3.3V）、開発中JP8短絡。通信USBはCN9、SWDはCN2。

## 使用するプロジェクト

`src/pneutouch-solist` の **PneutouchAi**。ビルド構成は **Debug**。
デバッグは **PneutouchAi Debug**、書き込みは **PneutouchAi Write**。
両方とも `Debug/PneutouchAi.elf` を使う。
基準状態はコミット `95a453e`。旧 `src/pneutouch-solist-ai` は移植の参照用に保持する。

## 今回の取得結果

- Windows/LEXIDEのDebugビルド: エラー0、警告0。ELF/HEX生成済み。
- PneutouchAi Writeで書き込み、COM7で受信確認。
- `captures/pressure-bringup-20260920-2.csv`: 315点、MCU時刻7906ms、実効39.7167 SPS、欠番0。
- `captures/pressure-touch-20260920-1.csv`: 1786点、MCU時刻44934ms、実効39.7249 SPS、欠番0。
  生値範囲3,941,168～3,952,084 counts。ユーザーは記録中に3回押して離したと報告。
  個々の押下時刻・部位は未記録のため、どの変化が押下に対応するかは未確定。
- kPaへの校正、SCKパルス幅の実測、気密評価、未接続からの回復の実機試験は未実施。

- `captures/pressure-touch-20260920-2.csv`: **1190点、29933ms、39.7220 SPS、欠番0**。
  2026-09-20 16:11:18–16:11:48 JST。ユーザーが繰り返し押していた間の記録。
  生値は **2,388,304～7,647,159 counts** と大きく変化し、24bitの上下限値は出ていない。
  個々の押下時刻・部位は未記録。単位換算・部位の分類精度を検証した結果ではない。
  CSV SHA-256: `c6a8812fb14148ce6f56f327cf99fa5534622be6856947ec34c11302d17dff89`。
- 書き込んだHEXのSHA-256: `1a462098e8027ba481f9d0b4cebfd3cadadb60d08e8613e3a06e0d465d94c650`。
