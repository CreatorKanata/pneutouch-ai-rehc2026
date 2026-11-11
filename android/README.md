# PneutouchAI Androidアプリ

Created by Kanata the Kid Creator.

`src/demo-visualizer` のHTMLデモを、AndroidのWebViewでそのまま表示します。
ボードとはUSBケーブル（USB OTG）で直接つなぎ、USB UARTをアプリが読みます。PCとPythonのサーバーは不要です。

- ページは `https://appassets.androidplatform.net/demo-visualizer/` としてAPK内から配信します。
  URLの構成と `/config.json` は `tools/serve_demo.py` と同じです。
- Web Serialの代わりに、`app/src/main/assets/web-serial-shim.js` が `navigator.serial` を定義し、
  ネイティブ側（[usb-serial-for-android](https://github.com/mik3y/usb-serial-for-android)）につなぎます。
  HTML/JavaScript側の受信・送信処理はPC版と同じです。

## ビルド

JDK 17とAndroid SDK（compileSdk 36）が必要です。`android/local.properties` に `sdk.dir=...` を書くか、
`ANDROID_HOME` を設定してください。

```sh
cd android
./gradlew assembleDebug
```

APKは `android/app/build/outputs/apk/debug/app-debug.apk`（約75 MB。動画を含むため）です。

HTML・JavaScript・画像・動画はGitの `android/` には置きません。ビルドのたびに `prepareWebAssets` タスクが
リポジトリの `src/demo-visualizer`・`src/shared`・`videos/demo`・`images/demo`・`images/logo` を
`app/build/generated/assets/` にコピーし、ルートの `config.py`（`UART_BAUD`・`NO_DATA_WARNING_S`）から
`config.json` を作ります。値を読めないときはビルドが失敗します。MP4は圧縮せずにAPKへ入れます。

テスト:

```sh
cd android
./gradlew testDebugUnitTest
# リポジトリ直下で、shimのテスト
node --test tests/android-serial-shim.test.mjs
```

## インストール

端末の開発者向けオプションでUSBデバッグを有効にしてから実行します。

```sh
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
```

## つなぎかた

1. ボードのUSB-C（**CN9**）と端末を、USB OTG対応のケーブル（USB-C to USB-C、またはOTGアダプター）でつなぎます。
2. 「PneuTouch AIで開きますか」と出たら許可します（アプリを先に開いていてもかまいません）。
3. 「恐竜につなぐ」を押します。初回はUSBの使用許可を求められるので許可します。
4. 恐竜を約1秒押して離します。

CN9はFTDI FT2232Hで、2つあるポートのうち2つ目（チャネルB）がUARTです。アプリは自動でこちらを開きます
（115200 baud・8N1・フロー制御なし。値は `config.py` から）。
ボードが見つからない、または許可されなかったときは、画面に理由を表示します。
ケーブルを抜くと切断として扱い、もう一度つないで「恐竜につなぐ」を押すと再開できます。

画面は横向き固定・全画面で、表示中はスリープしません。回転してもページは読み込み直しません。

## 制限

- USBホスト（OTG）に対応した端末が必要です。端末からボードへ給電できない場合は動きません。
- 端末のUSB-CをボードにつなぐのでUSBデバッグ用のケーブルは同時に使えません。ログを見るときは
  `adb` をWi-Fi経由にしてください。
- 「⛶ 全画面」ボタンは使えません（アプリ自体が全画面で表示します）。
- 対応するUSB機器はFTDI（ベンダーID 0x0403）だけです。
