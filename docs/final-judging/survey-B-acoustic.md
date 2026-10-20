# 調査B: 空気室の音響応答による接触位置推定 — 文献サーベイ

作成日: 2026-10-18 / 対象: ピンク恐竜（TPU 中空ボディ、全長 ~20 cm、内部空気 ~70 cm³、外形固定・内部空気室/仕切りのみ設計自由）
方式: 単一ポートの 1 本のマイク（または高速圧力センサ）だけで、押された場所を音響・振動音響的に識別できるか。
凡例: **未確認** = 一次資料の本文を確認できず、検索結果・抄録のみに基づく項目。

---

## 1. 結論（1 段落）

できる。しかも先行研究の条件は本件よりむしろ厳しい。TU Berlin の Zöller/Wall/Brock は、シリコーン製空気圧アクチュエータ（PneuFlex, 空気室長 ~90 mm）の空気室内に **$5 の MEMS マイク 1 個**を埋めるだけで、受動（タップ音）で 8 箇所 74.7 %、能動（小型スピーカからのスイープ/ホワイトノイズ 20 ms 以上）で 6 箇所 93–100 %・回帰誤差 3.7 mm、さらに 2.5×5 mm の点字 26 文字 88 % を達成し、90 dB の騒音下でも精度が落ちない（シリコーン壁が遮音材として働く）ことを示した。CMU の Xu/Chan らは 3D プリント中空ケース＋バルーン膜の「柔らかいゲームコントローラ」で、**対称な内部構造だと左右が区別できない**ことを見つけ、ランダムな非対称内部構造を追加して 5 点 98.7 %・RMSE 4.5 mm を得ており、これは本件の「内部仕切り設計で音響署名を作る」方針を直接裏付ける。物理的には、押圧は (a) 空気室体積・開口の変化によるヘルムホルツ/定在波共鳴周波数のシフト、(b) 柔壁接触による特定モードの減衰（Q 低下）、(c) タップ過渡の低周波スペクトル形状、として現れる。ただし一様な 70 cm³ の単一空気室では軽い押圧による共鳴シフトは 1–3 % 程度にとどまり、48 MHz M0+ の粗い FFT では読み取りにくいので、**空気室を 3–5 個の小室に仕切り、それぞれ異なる首（ネック）寸法で 300–900 Hz に共鳴を割り当てる「周波数分割」設計**にすると、押した小室だけが数十 Hz シフト＋減衰するため、12–32 個の帯域エネルギー＋小型 NN で識別できる見込みが高い。最大のリスクは (1) 個体差（別アクチュエータへのモデル転移は 35 %、チャンス 25 %）、(2) 温度ドリフト（音速 0.17 %/°C、能動方式では温度を 4.5 °C RMSE で逆推定できるほど署名が変わる）、(3) TPU 壁の減衰とヘルムホルツ共鳴の低下（定量値は未確認）である。

---

## 2. 一覧表

| # | 文献 | 年/会場 | 方式 | センサ・配置 | 励振 | 位置を符号化する特徴 | 場所数 / 精度 | 柔壁 |
|---|---|---|---|---|---|---|---|---|
| 1 | Zöller, Wall, Brock「Acoustic Sensing for Soft Pneumatic Actuators」 | IROS 2018 | 受動 | MEMS マイク Adafruit SPW2430 を空気室の根元に 1 個、48 kHz/24 bit USB I/F | 指でタップ | 振幅スペクトル（**200 Hz 以下を 1000 点**が最良）→ RBF-SVM | 8 箇所 74.7 %（根元 92 %、左右混同 25 %）、力 3 段 78 %、材質 3 種 69.4 %、騒音 50–90 dB で不変 | シリコーン Dragon Skin 10 |
| 2 | Zöller, Wall, Brock「Active Acoustic Contact Sensing for Soft Pneumatic Actuators」 | ICRA 2020 | 能動 | 同マイク＋Knowles バランスドアーマチュア SP を空気室の両端、48 kHz/32 bit | 20 Hz–20 kHz 対数スイープ 1 s | 0–24 kHz 振幅スペクトル（位相は捨てる）→ k-NN | 6 箇所 93 %（受動 47 %、動的 58 %）、30 点 3 mm 間隔の回帰 RMSE 3.7 mm（受動 18 mm）、20–80 dBA 騒音で不変 | 同上 |
| 3 | Wall, Zöller, Brock「Passive and Active Acoustic Sensing for Soft Pneumatic Actuators」 | IJRR 2023 (arXiv 2208.10299) | 受動/能動 | 同上 | スイープ・白色雑音・2–4 kHz 帯域雑音・2580 Hz 正弦、5 ms–1 s | 共鳴ピークのシフト（最大ピーク 2–4 kHz）、k-NN/SVC/RF/MLP | 6 箇所 93 %、位置+力+膨張同時 95/97/100 %、温度 RMSE 4.5 °C、**音種・長さに依存せず（20 ms 以上で 97–99.7 %）**、**別個体への転移 35 %** | 同上 |
| 4 | Wall, Brock「A Virtual 2D Tactile Array for Soft Actuators Using Acoustic Sensing」 | IROS 2022 | 能動 | 同上 | 1 s スイープ | 平滑化スペクトル（STFT 時間和）が波形/スペクトログラム/メル/MFCC より良い | 点字ピン 2.45 mm ピッチで x RMSE 1.67 mm・y 0 mm、26 文字 88 % | 同上 |
| 5 | Laput, Brockmeyer, Hudson, Harrison「Acoustruments」 | CHI 2015 (Best Paper) | 能動 | スマホの SP→3D プリント管→マイク | 16.5–22.05 kHz 線形スイープ 100 ms | 15.5 kHz 以上の FFT を 100 ビンに縮約＋1/2 階微分＋帯域比 → SMO-SVM | 各プリミティブ 99.5 %、ノブ ±7.8°、57 dB 室内で差なし | 硬質樹脂（「柔らかい空洞で握り圧」プリミティブあり） |
| 6 | Murray-Smith, Williamson, Hughes, Quaade, Strachan「Stane」 | CHI 2008 | 受動 | ピエゾ接触マイクを SLA 樹脂ケース内側に 1 個 | 擦る・引っ掻く・叩く | 瞬時特徴→2 段分類、リアルタイム | 4 クラス（精度の数値は**未確認**） | 硬質 |
| 7 | Ono, Shizuki, Tanaka「Touch & Activate」 | UIST 2013 (Best Paper) | 能動 | 振動 SP＋ピエゾマイクを物体に貼付 | 20–40 kHz スイープ | 共鳴スペクトル→SVM | プラ玩具 5 ジェスチャ 99.6 %、6 姿勢 86.3 % | 硬質（固体伝搬） |
| 8 | Watanabe, Sasaki, Terada, Tsukamoto「ゲル板上の押圧位置・せん断力」 | Sensors 2022 | 能動 | 接触 SP＋接触マイクをゲル板に斜め配置、96 kHz | 20–40 kHz スイープ | LFCC 19 次元（FFT 8192）→SVM | 4 位置 F1 89.4 %、せん断 4 方向 69.4 % | スチレン系エラストマー 5 mm |
| 9 | 岩瀬, 伊藤ら「アクティブ音響センシングによる日常物体識別と位置推定」 | Interaction 2018 | 能動 | アクリル板にピエゾ SP/マイク、48 kHz | 20–40 kHz スイープ | 20–40 kHz の振幅 107 点 | 単一物体 種類 98.2 %・位置 85.5 %、複数物体 93.8 % | 硬質板 |
| 10 | Katakura, Watanabe「ProtoHole」 | CHI 2018 LBW | 能動 | 3D プリント物体内に SP＋マイク | 高周波スイープ | 穴を塞いだときの共鳴変化→ML | 穴＝ボタン（数・精度は**未確認**） | 硬質 |
| 11 | Xu, Sitaraman, Yuan, Agarwal, Goel, Chan「Acoustic Resonance Sensing for Game Controller」 | IEEE SENSORS 2025 | 能動 | 中空 3D プリントケース（9×10 cm）底に SP＋マイク、上面はバルーン膜、48 kHz スマホ | ガウスパルス 15.8 ms を 20 Hz 繰返し | 64 ms 窓 FFT の **7–16 kHz 576 点**＋移動平均→線形 SVM | **対称構造では左右不可→ランダム非対称内部構造**で 5 点 98.7 %、RMSE 4.5 mm、圧力 3 段 98.3 % | バルーン（ラテックス）膜 |
| 12 | Xu, Chan「Soft Tactile Sensors for Robot Grippers Using Acoustic Sensing」 | CMU 2024–25（会場**未確認**） | 能動 | ラテックス管 10 cm の両端にイヤホン SP＋マイク（$0.72 の管） | 50 ms ガウスパルス 20 Hz | 定在波ピークのシフト/消失→SVM | 管に沿った接触位置 RMSE 6 mm、平面型（マイク 3 本）0.57–0.62 mm、6 物体 97.7 % | ラテックス |
| 13 | Mikogai, Kazumi, Takemura「Contact Point Estimation along Air Tube…」 | RA-L 2020 | 受動（コンプレッサ騒音利用） | 空気チューブ端のマイク | コンプレッサ騒音 | スペクトログラム→CNN | 16 m チューブ上の接触点（誤差は**未確認**） | 樹脂チューブ |
| 14 | Drew ら「Acoustic Communication and Sensing for Inflatable Modular Soft Robots」 | ICRA 2021 | 能動 | ラテックス膜（φ45 cm）内側にピエゾ円板、Teensy 4 | 6 kHz トーン | 接触で中央ノードの受信振幅が初期値の 10 % 未満に低下 | 接触有無・方向（1 Hz 更新） | ラテックス 0.4 mm |
| 15 | Li, Huh, Yahnker, Stuart「Resonant Pneumatic Tactile Sensing for Soft Grippers」 | RA-L 2022 | 空気流励振（無電子） | 1 m 離れた外部マイク 44.1/48 kHz | 空気流による管共鳴 ~3 kHz（L=90 mm） | 共鳴周波数（33 ms ハミング窓、σ=1.65 Hz）、穴で 0–2 mm プレタッチ | 指ごとに管長を変えて周波数分離 | シリコーン管 |
| 16 | Li, Stuart「AcousTac」／Aderibigbe, Li, Lee, Stuart「Milli-Scale AcousTac」 | Soft Robotics 2025 / ICRA 2025 | 空気流励振 | 外部マイク | 管共鳴 3–5.5 kHz / ヘルムホルツ 4–5.5 kHz（空洞 ~31–38 mm³、1.5 L/min） | f ∝ 1/√V_eff、V_eff = V − αF、RMS 1.6 Hz、~25 N まで線形 | 力センサ（位置は管長で多重化） | シリコーンキャップ |
| 17 | Shinoda, Matsumoto, Ando「Acoustic Resonant Tensor Cell」 | ICRA 1997 | 能動（超音波） | エラストマー内球状空洞＋超音波送受 | 超音波 | 空洞共鳴周波数が主応力に比例してシフト | 応力テンソル | エラストマー |
| 18 | Alonso-Martín ら／Gamboa-Montero ら「social robot + contact microphones」 | Sensors 2017 / EAAI 2020 | 受動 | ロボット外殻内側に接触マイク（1 殻部位 1 個） | 触れる | 音響特徴→ロジスティック回帰 | 4 ジェスチャ F 87 %、2020 版は部位同定（数値**未確認**） | 硬質殻 |
| 19 | Tejada, Ramakers, Boring, Ashbrook「AirTouch」 | CHI 2020 | 空気圧（音響ではない） | 単一気圧センサ＋3D プリント内空気路 | 押圧 | 空気路ごとの圧力応答（詳細**未確認**） | 最大 12 箇所 98 % | 硬質（単一構造プリント） |
| 20 | Rajendran ら「AST Skin」 | arXiv 2023 | 能動 | 柔膜内の音響チャネル、SP→マイク | 300/500/700/900 Hz 正弦 | 各周波数成分の振幅 | 3 位置 96 %、力 0–30 N（±1.5 N 内 93 %） | シリコーン膜 |
| 21 | SonicBoom（比較用） | arXiv 2024 | 受動 | ピエゾ接触マイク **6 本** | 衝突 | マイク間相対特徴 | 0.43 cm（学習 18,000 サンプル） | PVC |
| 22 | Edge Impulse / CMSIS-DSP ベンチ | — | 組込み | — | — | RP2040(M0+ 125 MHz) 256 点 Q15 FFT 0.87 ms、MFCC は ~1 s/推論 | — | — |

---

## 3. 文献ごとのメモ（数値つき）

### 3.1 Zöller, Wall, Brock, "Acoustic Sensing for Soft Pneumatic Actuators", IROS 2018, pp. 6986–6991
URL: https://www.static.tu.berlin/fileadmin/www/10002220/Publications/Zoeller-18-IROS_acousticsensing.pdf
- センサ: MEMS コンデンサマイク Adafruit SPW2430（$5、線形域 100 Hz–10 kHz だが**それよりはるかに低い周波数も記録する**）。PneuFlex 指（Smooth-On Dragon Skin 10 シリコーン）の根元を切って空気室内に挿入し、シリコーン接着剤で再封止。根元は膨張時の変形が最小。USB オーディオ I/F で 48 kHz/24 bit。
- 励振: 受動。アクリル製支柱に指をタップ。8 接触箇所（先端/中央/根元の腹側、左/右/上、支柱、など）× 4 膨張圧（0/15/30/50 kPa）× 5 回 × 6 セット。
- 特徴: 可変長の録音を振幅スペクトルに変換し固定長化。カットオフ周波数（100 Hz/200 Hz/1 k/3 k/10 kHz）と点数（100/1000）をハイパーパラメータとして探索。**最良は「200 Hz 以下を 1000 点」+ RBF-SVM（γ=0.001, C=100）**。k-NN は −2 %、ロジスティック回帰は −20 %。NN はデータ不足で未採用。
- 結果: 位置 8 クラス **74.7 %**。根元（マイク直近）92 %、支柱（指外）70 %。左→右 25 %、中央→上 10 % の誤り（マイクから等距離の対称点）。力 3 段（<1, 1–2, 2–3 N）CV 80 %/テスト 78 %（2–3 N は 96 %）。材質（木/シリコーン/アルミ）69.4 %。騒音（事務所音 50/70/90 dB を 10 cm から、白色雑音も）で分類率はほぼ不変。
- 実装: 1 s 移動窓で 13 Hz のオンライン推定を RBO Hand 2 の人差し指で実演。
- 含意: 受動タップで有効だったのは **200 Hz 以下**。すなわち「高速圧力センサ 1 個」でもタップ位置が識別できる可能性を示す。対称位置の混同は内部構造の非対称化で解消すべき課題。

### 3.2 Zöller, Wall, Brock, "Active Acoustic Contact Sensing for Soft Pneumatic Actuators", ICRA 2020, pp. 7966–7972
URL: https://www.static.tu.berlin/fileadmin/www/10002220/Publications/Zoeller-20-ICRA_activeacoustic.pdf（コード/データ: doi:10.14279/depositonce-9711）
- センサ: 同マイクを空気室の根元端、Knowles バランスドアーマチュア SP（RAB-32063-000, 80 Hz–10.8 kHz）を先端端に。両端配置で変調を最大化。SP ケーブルは曲線配線でコンプライアンス維持。MAYA44 USB+ で 48 kHz/32 bit。
- 励振: 20 Hz–20 kHz 対数スイープ 1 s（低域強調は 2018 の知見から）。録音は前後 0.2 s を含む 1.4 s。
- 特徴: 0–24 kHz 振幅スペクトル全点、位相は捨てる（タイミング非依存）。**スペクトルのピークは空気室の共鳴周波数に対応し、接触位置ごとに目視で分かるシフト**が出る。既定 k-NN（ユークリッド）で十分。
- 結果: 6 箇所（腹側 先端/中央/根元＋左/上/右）× 25 回、90 学習/60 テストで **93 %**（受動静接触 47 %、動的タップ 58 %、チャンス 17 %）。87 mm の腹側に 3 mm 間隔で 30 点、k-NN 回帰で **RMSE 3.7 mm**（受動 18 mm）。騒音 20–80 dBA でも不変（むしろ微増）。異物体（木/ほか）にも頑健。Panda アームでのモータ雑音・隣接する能動センサ 4 本同時でも無影響。
- 限界: 未膨張のみ。1 s の音が更新レートを制限。

### 3.3 Wall, Zöller, Brock, "Passive and Active Acoustic Sensing for Soft Pneumatic Actuators", IJRR 2023（arXiv 2208.10299）
URL: https://arxiv.org/pdf/2208.10299 / https://journals.sagepub.com/doi/10.1177/02783649231168954
- 統合版。追加の重要知見:
  - **音種・長さ依存性 (6.1)**: スイープ/白色雑音/2–4 kHz 帯域雑音/2580 Hz 正弦 × 5 ms/20 ms/50 ms/500 ms/1 s。平均 97.4 %（正弦）～99.7 %（白色雑音）。**20 ms 以上なら差がほぼない**。音量 25 % でも高精度。→ MCU 向けに 20–50 ms の短い雑音バーストで十分。
  - 最大共鳴ピークは **2–4 kHz**（2580 Hz）。90 mm 空気室なら半波長 c/2L ≈ 1.9 kHz と桁が合う。
  - **個体転移 (6.3)**: 5 本の同型アクチュエータ、4 クラス。自個体 97 %、**他個体 35 %（チャンス 25 %）**、複数個体で学習しても 47 %。製造ばらつきが音響署名を大きく変える → 個体ごとの較正が必須。
  - 温度: 20–95 °C を能動（白色雑音）で RMSE 4.5 °C、受動で 10.9 °C で推定できる = **温度が署名を動かす交絡因子**。
  - 同時推定: 7 位置 × 2 力 × 2 膨張圧の 700 サンプルで、位置 95 %/力 97 %/膨張 100 %（SVC）。各性質が異なる周波数帯を変調している。
  - 騒音: 外部 10 cm に SP、30→90 dB で分類率不変、内部 SNR 47–48 dB を維持。**「シリコーン外皮が遮音材として働く」**と解釈。
  - 学習器 (6.4): 材質課題では k-NN が弱く SVC/RF/MLP が大きく改善。位置課題は k-NN で十分。
  - 解析モデル: 「空洞寸法変化による共鳴周波数変化は解析的にモデル化できるが、変形や接触による変化は非対称な柔軟体では複雑すぎる」→ データ駆動を選択。
  - 限界 (7): 自ら音を出す物体との接触、個体転移の悪さ（「較正音」での補正を提案）、材質など変調の弱い量は複雑な学習器が必要。

### 3.4 Wall, Brock, "A Virtual 2D Tactile Array for Soft Actuators Using Acoustic Sensing", IROS 2022
URL: https://arxiv.org/pdf/2208.10218
- 同ハードウェア。プログラマブル点字ディスプレイ（32×4 ピン、ピッチ 2.45 mm、押上力 0.17 N、高さ 0.7 mm）で接触パターンを生成。
- 表現比較: 波形/周波数スペクトル/**平滑化スペクトル（STFT の時間和）**/スペクトログラム/メル/MFCC。**平滑化スペクトルが最良**（雑音が減り点数が少ない）。k-NN/SVM/NN/CNN を比較。
- 結果: x 方向 29 ライン RMSE 1.67 mm、y 方向 4 ライン 0 mm。単一ピン（0.17 N）で x 76 %/y 79 %、2×2 ピンで 85 %/94 %。2.5×5 mm 領域の点字 26 文字 **88 %**。
- 含意: 0.17 N の極軽い接触でも検出可能。仮想タクセルの形状はソフトで再定義できる。

### 3.5 Laput, Brockmeyer, Mahler, Hudson, Harrison, "Acoustruments", CHI 2015（Best Paper）
URL: https://chrisharrison.net/projects/acoustruments/PrintedAcoustics.pdf / https://dl.acm.org/doi/10.1145/2702123.2702414
- スマホ SP→3D プリント（Objet260, VeroClear 硬質）管（φ3.5 mm 程度）→スマホマイク。44.1 kHz。
- 励振: **16.5–22.05 kHz 線形スイープ 100 ms**（50–100 ms が良い）。
- 特徴: 15.5 kHz 以上の FFT を 100 ビンに縮約＋1 次/2 次微分＋帯域比・最大/最小位置・重心 → SMO-SVM（多項式核）、回帰は ν-SVR。
- プリミティブ: 管長（長いほど低音）、曲がり（回数は影響小）、穴（**φ3.0–7.0 mm が最も識別的**）、断面変更、分岐、ギャップ、ボール弁、空洞（**柔らかい空洞を握ると圧力センサになる**）、スライド笛式スライダ、共鳴器。
- 結果: 200 サンプルで平均 **99.5 %**、ノブ ±7.8°、57 dB 事務所騒音で差なし（管が閉系で外部雑音を減衰）。
- 限界: 分岐間のクロストーク、スマホ音響 HW の制約。
- 含意: 「内部の管・穴・空洞の幾何で音響署名を設計する」語彙集として本件に直接使える。

### 3.6 Murray-Smith, Williamson, Hughes, Quaade, Strachan, "Stane", CHI 2008
URL: https://mural.maynoothuniversity.ie/id/eprint/1735/1/MurWilHugQuaStr08.pdf
- SLA 3D プリント樹脂ケース内側にピエゾ接触マイク 1 個。表面にディンプル/放射状/勾配/リッジのテクスチャ。擦る・引っ掻く・叩く振動を実時間で 2 段分類（4 クラス: 前面円周、右ディンプル、先端爪、その他）。大音量の外部雑音の影響は無視できたと記述。精度の数値は本文抜粋からは**未確認**。
- 含意: 硬質殻・固体伝搬。テクスチャ設計で「触った場所を音で分ける」最初期の例。

### 3.7 Ono, Shizuki, Tanaka, "Touch & Activate", UIST 2013（Best Paper）／"Sensing Touch Force using Active Acoustic Sensing", TEI 2015
URL: https://dl.acm.org/doi/10.1145/2677199.2680585（TEI）
- 振動 SP＋ピエゾマイクを対象物に貼付、**20–40 kHz スイープ**、共鳴スペクトル→SVM。プラ玩具で 5 タッチジェスチャ 99.6 %、6 把持姿勢 86.3 %。TEI 2015 は SVC+SVR で力も推定。
- 含意: Touché（容量式スイープ）の音響版。固体伝搬のため柔らかく減衰の大きい TPU では超音波帯が使いにくい（**未確認**）。

### 3.8 Watanabe, Sasaki, Terada, Tsukamoto, Sensors 2022（ゲル板）
URL: https://www.mdpi.com/1424-8220/22/24/9951
- 5 mm スチレン系エラストマーゲル板に接触 SP/接触マイク、20–40 kHz スイープ、96 kHz、LFCC 19 次元（FFT 8192）→SVM。4 押圧位置 F1 89.4 %、せん断 4 方向 69.4 %。「実用には改善が必要」と自己評価。
- 含意: 柔軟材でも超音波スイープで位置識別は可能だが精度は硬質より落ちる。

### 3.9 岩瀬, 伊藤, 秦, 山下, 尾上, Interaction 2018
URL: http://www.interaction-ipsj.org/proceedings/2018/data/pdf/INT18008.pdf
- アクリル板の角にピエゾ SP/マイク、48 kHz、20–40 kHz の振幅 107 点。板上の単一物体: 種類 98.2 %・位置 85.5 %、複数物体 93.8 %。

### 3.10 Xu, Sitaraman, Yuan, Agarwal, Goel, Chan, "Acoustic Resonance Sensing for Game Controller", IEEE SENSORS 2025
URL: https://www.cs.cmu.edu/~justinc3/docs/acoustic_game.pdf
- **本件に最も近い構成**: 3D プリント中空ケース（底 9×10 cm）＋上面にバルーン膜、底に Fielect ミニ SP とマイク、スマホ 48 kHz。
- 励振: 15.8 ms ガウスパルス、20 Hz 繰返し。64 ms 窓 FFT、**7–16 kHz の 576 点**＋移動平均 12。
- **核心の知見**: 中空円筒のままでは対称点（左ボタン/右ボタン）の FFT が同一で区別不可、さらに事前録音では高精度でも実時間で劣化（過学習）。**底面にランダムな非対称構造を 3D プリントで追加**して空間多様性を上げたところ、5 点＋非押下 **98.7 %**、タッチバー回帰 **RMSE 4.5 mm**、圧力 3 段 98.3 %（線形 SVM、学習 120–240 サンプル）。2 段構成（押下検出→位置回帰）。
- 含意: 「外形固定・内部のみ自由」という本件の制約下で、**内部仕切り/柱の非対称配置が位置識別の鍵**になることを実証。

### 3.11 Xu, Chan, "Soft Tactile Sensors for Robot Grippers Using Acoustic Sensing", CMU（2024–25, 会場**未確認**）
URL: https://www.cs.cmu.edu/~justinc3/docs/soft_gripper.pdf
- 10 cm の天然ラテックス管（$0.72）の両端にイヤホン SP（Sony MDR-EX15AP）とマイク。50 ms ガウスパルス 20 Hz。管の定在波ピークが押圧で減衰・モード結合してシフト/消失→SVM。接触位置 RMSE 6 mm（5 mm 刻み）。平面型（バルーン膜＋マイク 3 本）0.57–0.62 mm、6 物体 97.7 %。
- 含意: 極薄の柔壁（バルーン）でも空気室内の共鳴は十分検出可能。

### 3.12 Mikogai, Kazumi, Takemura, RA-L 5(3):4618–4625, 2020（doi:10.1109/LRA.2020.3002196）
- 空気圧システムのコンプレッサ騒音を音源として、16 m の空気チューブ上の接触点をスペクトログラム＋CNN で推定。専用音源不要。誤差値は**未確認**。国内発表: 御子貝, 竹村「アクティブ音響センシングを用いたチューブの接触位置推定」SICE SI 2019。
- 含意: 本件のポンプ/バルブ騒音も「無料の励振源」として使える可能性。

### 3.13 Drew ら, "Acoustic Communication and Sensing for Inflatable Modular Soft Robots", ICRA 2021
URL: https://arxiv.org/pdf/2101.11817
- φ45 cm、0.4 mm ラテックス膜の内側にピエゾ円板（φ27 mm 真鍮、φ20 mm セラミック）を磁気コネクタに固定。Teensy 4（Cortex-M7 600 MHz）、60 dB アンプ、3–20 kHz で約 60 mW。アンプの整合不良で 2 kHz のハイパス。
- 加圧膜は空気伝搬音の全方向ピックアップとして働き、接触マイクが真のマイクとして機能。6 kHz トーンを周辺ノードが出し中央ノードで受信、接触で振幅が初期値の **10 % 未満**に低下、1 Hz 更新。空間分解能はノード密度と膨張体積で決まる。
- 含意: 柔膜への接触は特定伝搬経路の減衰として強く現れる（減衰ベースの特徴）。

### 3.14 Li, Huh, Yahnker, Stuart, RA-L 2022／Li, Stuart, Soft Robotics 2025（AcousTac）／Aderibigbe, Li, Lee, Stuart, ICRA 2025（Milli-Scale）
URL: https://escholarship.org/uc/item/8ht881qh / https://doi.org/10.1089/soro.2023.0082 / https://edg.berkeley.edu/wp-content/uploads/2025/05/ICRA25_3910_FI-1.pdf
- 電子部品を接触点に置かず、**空気流で共鳴器を鳴らし外部マイク（1 m 先）で周波数を読む**。管長 90/130 mm で基本 ~3 kHz・2 倍音 6 kHz、44.1/48 kHz、33 ms ハミング窓、σ 1.65 Hz。管に φ3.2 mm の穴を開けると接触前 0–2 mm のプレタッチが読める。指ごとに管長を変えて周波数で分離（周波数分割多重）。
- Milli 版: ヘルムホルツ式に補正係数を入れ、**V_eff = V_c − αF** として有効体積の減少で周波数上昇をモデル化。ネック半径 1.59 mm・長さ 4.76 mm、空洞 ~31–38 mm³、4–5.5 kHz、1.5 L/min、RMS 誤差 1.6 Hz、~25 N まで線形。**柔壁（シリコーン）では剛壁モデルの補正が必要**と明記。
- 含意: 「体積 → 周波数」の設計式と、タクセルごとの周波数割当の実例。

### 3.15 Shinoda, Matsumoto, Ando, "Acoustic Resonant Tensor Cell", ICRA 1997
- エラストマー内の球状空洞の超音波共鳴周波数が周囲の主応力と明示的関係を持つ。空洞変形→共鳴シフトの原理論文。

### 3.16 Alonso-Martín ら, Sensors 17(5):1138, 2017／Gamboa-Montero ら, EAAI 92:103670, 2020
- 社会ロボット Maggie の外殻内側に接触マイク（殻の 1 部位に 1 個で足りる）。撫でる/叩く/平手/くすぐりの 4 種を F 87 %（ロジスティック回帰）。2020 版は「detecting, locating and recognising」で部位同定を追加（数値**未確認**）。接触マイクは音声など空気伝搬雑音に鈍感。

### 3.17 Tejada, Ramakers, Boring, Ashbrook, "AirTouch", CHI 2020
URL: https://dl.acm.org/doi/10.1145/3313831.3376136
- 音響ではなく**単一気圧センサ＋3D プリント内空気路**で最大 12 箇所 98 %。単一構造で印刷、組立・較正最小。空気路ごとに異なる圧力応答を作る点で「単一ポート＋内部設計」の本件に近い（詳細は**未確認**）。

### 3.18 その他
- AST Skin（Rajendran ら, 2023–24）: 柔膜内チャネルに 300/500/700/900 Hz 正弦を流し各成分の振幅で 3 位置 96 %、力 0–30 N。周波数ごとに別チャネルを割り当てる FDM 的設計。
- SonicBoom（2024）: 接触マイク 6 本・18,000 サンプルで 0.43 cm。単一マイクではない比較対象。
- VibeCheck（IROS 2025）: ピエゾ指 2 本、20 Hz–20 kHz 1 s スイープ、FFT→カーネル PCA 5–10 成分で汎化。特徴次元を 5–10 に落とせる示唆。
- Knocker（IMWUT 2019）: スマホで叩いて 23 物体 98 %（実環境 83 %）。
- Acoustic Barcodes（UIST 2012）/ Lamello（CHI 2015）: 刻み・櫛歯の受動音響タグ。
- Shinoda 系の国内研究、空気圧人工筋の安全化（JRSJ 2010）などは「空気室共鳴で接触位置」という直接の国内文献としては見つからず（**未確認**）。

---

## 4. 設計への含意: 内部幾何が音響署名をどう変えるか（~20 cm TPU 恐竜、空気 ~70 cm³）

### 4.1 支配的な物理（3 つの手がかり）
1. **共鳴周波数シフト**（能動向き）: ヘルムホルツ f_H = (c/2π)·√(A/(V·L_eff))、定在波 f_n ≈ n·c/(2L)。押圧で V が減る/開口 A が塞がる/L が変わる。
2. **モード減衰（Q 低下）**（能動・受動共通）: 柔壁に指が触れると、その壁が担うモードのピークが鈍る・消える（Xu/Chan の管実験、Drew の振幅 10 % 低下）。
3. **タップ過渡の低周波形状**（受動向き）: IROS 2018 で有効だった帯域は **<200 Hz**。壁の局所剛性・マイクまでの経路で過渡波形が変わる。高速圧力センサでも拾える帯域。

### 4.2 数値例（c = 343 m/s、V = 70 cm³ = 7.0×10⁻⁵ m³、L_eff = L + 1.7r）
| 構成 | 計算 | f |
|---|---|---|
| 密閉 70 cm³・ポート無し | ヘルムホルツ共鳴なし。空気室長 15–18 cm の半波長モード | 950–1140 Hz（2 次 1.9–2.3 kHz） |
| ポート φ4 mm, L=5 mm（L_eff 8.4 mm） | 54.6·√(1.26e-5 / (7e-5·8.4e-3)) | **≈ 250 Hz** |
| ポート φ8 mm, L=5 mm（L_eff 11.8 mm） | 54.6·√(5.03e-5 / (7e-5·1.18e-2)) | **≈ 430 Hz** |
| 押圧で体積 −7 cm³（−10 %） | f ∝ V^−1/2 → +5.1 % | 250 Hz で +13 Hz |
| 軽い押圧で −1 cm³（−1.4 %） | +0.7 % | 250 Hz で **+1.8 Hz**（検出困難） |
| 温度 +20 °C | c +3.5 % → f +3.5 % | 250 Hz で +9 Hz（押圧と同程度 → 交絡） |

→ **単一の大きな空気室では、軽い押圧の共鳴シフトは温度ドリフトに埋もれる。** 位置情報は「どのモードが減衰したか」「どの小室が変形したか」で取るべき。

### 4.3 推奨: 小室分割＋周波数分割（FDM）
70 cm³ を **4 小室（各 ~15 cm³）** に TPU 仕切りで分け、共通マニホールド（マイク/SP ポート側）へ**首径の異なる開口**でつなぐ。各小室はヘルムホルツ共鳴器になる（L=3 mm の仕切り厚として）:

| 小室 | 首径 | A | L_eff | f_H |
|---|---|---|---|---|
| 頭 | φ2 mm | 3.14 mm² | 4.7 mm | ≈ 365 Hz |
| 背中 | φ3 mm | 7.07 mm² | 5.6 mm | ≈ 500 Hz |
| 腹 | φ4 mm | 12.6 mm² | 6.4 mm | ≈ 625 Hz |
| 尾 | φ5 mm | 19.6 mm² | 7.3 mm | ≈ 730 Hz |

- 15 cm³ の小室を 1 cm³ 押すと −6.7 % → **+3.4 %（365 Hz で +12 Hz、730 Hz で +25 Hz）**、同時にその小室の壁減衰でピーク高さが落ちる。隣接小室はほぼ不変 → **「どのピークが動いた/鈍ったか」が位置ラベルに直結**。温度は全ピークを同率に動かすので、ピーク間の比（f_i/f_j）やピーク高さ比で相殺できる。
- 帯域は 300–900 Hz に収まるため、**8 kHz サンプリング**で足り、TPU の高域減衰も問題にならない。
- Xu/Chan の教訓: **左右対称な小室配置は避ける**（鏡像位置は同一署名）。仕切り位置・首径・小室体積を意図的に非対称にする。
- Acoustruments の語彙（穴径 3–7 mm が最も識別的、分岐、柔らかい空洞で圧力）と AcousTac の「管長/体積でタクセルごとに周波数を割当」を踏襲。
- 柔壁補正: 弾性壁のヘルムホルツ共鳴は剛壁より低く帯域が狭い（Milli-AcousTac、Sci. Rep. 2026 の記述）。**TPU 壁での実測シフト量は未確認** — 試作で較正する前提。
- 既存の空気圧アクチュエーション（膨張圧）があるなら、IJRR で膨張圧が 100 % 分離できたように署名に入るので、**膨張状態ごとに学習**するか、圧力を NN 入力に加える。

### 4.4 受動（タップ）方式の設計
- マイク/圧力ポートは変形の少ない根元（恐竜なら胴体中央下部）に置き、タップ箇所から壁伝搬＋空気伝搬で届く過渡を <500 Hz で見る。
- 仕切りは「伝搬経路の非対称化」として効く（根元 92 % vs 対称点 25 % 誤り、の教訓）。
- 期待精度は能動より低い（8 点 75 %、回帰 18 mm）。**能動＋受動の併用**（タップ検出は受動、位置は能動パルスで確認）が IJRR の「2 段」や Xu らの「押下検出→回帰」と整合。

---

## 5. Cortex-M0+（48 MHz, 16 KB RAM）での実現性

### 5.1 フロントエンド
| 要素 | 推奨 | 根拠 |
|---|---|---|
| マイク | **アナログ MEMS**（SPW2430 系、または ICS-40180 等）→ MCU の 12 bit ADC。PDM マイクは M0+ での間引きフィルタが重く不可 | Zöller らは SPW2430（$5）で全結果を達成。SPW2430 は 100 Hz 以下も記録する |
| 代替 | 既存の高速圧力センサ（帯域 ≥200 Hz が必要） | IROS 2018 の最良特徴は <200 Hz |
| サンプリング | **8 kHz**（FDM 設計 300–900 Hz なら 4 kHz でも可）、DMA で 256 点ダブルバッファ（1 KB） | 必要帯域が <4 kHz |
| 能動音源 | 小型ダイナミック/BA スピーカを PWM（or DAC）で駆動、**20–50 ms の帯域雑音バースト or 300–1000 Hz 短チャープ**、25 % 音量 | IJRR 6.1: 20 ms 以上・音種不問・25 % 音量で 97–99.7 % |

### 5.2 計算量・メモリ
- 256 点実 FFT（CMSIS-DSP Q15）: RP2040（M0+ 125 MHz）で **0.87 ms** → 48 MHz 換算 **≈ 2.3 ms**。8 kHz で 32 ms フレームごとなら CPU 7 %。
- さらに軽い代替: **Goertzel** で 12–24 ビン（各小室の f_H ± 1 ビン、2 倍音）だけ計算 → 24 × 256 ≈ 6 k MAC/フレーム（<0.5 ms）。
- 特徴: 16–32 帯域の対数エネルギー（FDM 設計なら各小室ピーク周辺 4–6 帯域 × 4 小室 ≈ 16–24 次元）を 4–8 フレーム平均 → 12–64 入力に収まる。**MFCC は不要**（Wall らは平滑化スペクトルが MFCC より良いと報告、Edge Impulse は RP2040 で MFCC ~1 s と警告）。
- NN: 24→16→5（ReLU, int8）≈ 500 重み → 1 KB 未満。推論 <1 ms。
- RAM 見積り: ADC バッファ 1 KB＋FFT 作業 1 KB＋特徴/NN 1 KB＋スタック ≈ **4 KB** → 16 KB 内。
- 温度補正: ピーク周波数比・高さ比を特徴にする（§4.3）か、温度センサ値を NN 入力に追加（IJRR が温度を署名から推定できた＝入力に入れれば分離可能）。

### 5.3 組込みの先行例
- Drew ら ICRA 2021: Teensy 4（M7 600 MHz）＋ピエゾで 6 kHz トーン振幅検出 1 Hz 更新（本件より重い MCU だが処理は単純な振幅比較）。
- Edge Impulse: SAMD21（M0+ 48 MHz, 32 KB RAM）で 2 語 KWS（MFE 特徴）、約 30 KB Flash、1–2 語が限界。
- Compact RNN 音響イベント検出: 34.3 kB RAM・125 ms/秒（M0+ 16 KB では超過 → FFT 帯域エネルギー＋Dense が現実的）。
- 2605.10110（PerCom 2026 ws）: ピエゾ＋8,722 パラメータの DS-1D-CNN で 6 ジェスチャ、ユーザ非依存。
- **16 KB RAM・48 MHz で空気室音響位置推定を実装した公開例は見つからなかった（未確認）**。ただし上記見積りで余裕はある。

### 5.4 学習運用
- 個体較正は必須（他個体転移 35 %）。工場/初回起動時に「各小室を順に押す」較正シーケンス 25 回 × N 箇所（Zöller らの学習サンプル数は 15–25/クラス）→ PC で学習して int8 重みを書き込む、または MCU 上で最近傍テンプレート（k-NN）を保持（24 次元 × 5 箇所 × 10 テンプレート = 1.2 KB）。

---

## 6. 未解決の課題（ギャップ）

1. **TPU 壁での定量値が無い**: 先行研究はシリコーン/ラテックス/硬質樹脂。TPU（ショア 85–95A）中空体のヘルムホルツ共鳴の実周波数・Q・押圧時シフトは未確認。試作で 300–900 Hz のピークが見えるか、まず確認が必要。
2. **温度・膨張圧の交絡**: 音速 0.17 %/°C、膨張圧で署名が 100 % 分離できるほど変わる。ピーク比特徴＋圧力/温度入力で相殺できるかは未検証。
3. **個体差**: 製造ばらつきで転移 35 %。量産玩具なら個体較正フローか「較正音」補正（Wall らの提案、未実装）が必要。
4. **対称性**: 鏡像位置は内部非対称化で解く（Xu らが実証）が、恐竜の左右対称ボディで仕切りをどこまで非対称にできるかは設計次第。
5. **複数点同時接触・把持**: IROS 2022 は 2×2 ピンまで。子どもが抱える（全面接触）状態の扱いは未報告。
6. **自ら音を出す物体**（IJRR 7）: 玩具自身のスピーカ出力・ポンプ音が署名を汚す。Mikogai らのようにポンプ音を逆に励振源として使う案は未検証。
7. **M0+ 実装例の不在**: 計算見積り上は可能だが、ADC 雑音・PWM 駆動スピーカの再現性（能動方式は励振の再現性に依存）を実機で確認する必要がある。
8. **受動方式の精度上限**: 単一マイク受動は 75 %/18 mm 止まり。能動パルスを数十 ms 入れるコスト（電力・音漏れ、25 % 音量で可）との trade-off を決める必要がある。

---

## 参考 URL（主要）
- IROS 2018: https://www.static.tu.berlin/fileadmin/www/10002220/Publications/Zoeller-18-IROS_acousticsensing.pdf
- ICRA 2020: https://www.static.tu.berlin/fileadmin/www/10002220/Publications/Zoeller-20-ICRA_activeacoustic.pdf
- IJRR 2023: https://arxiv.org/pdf/2208.10299 ／ https://journals.sagepub.com/doi/10.1177/02783649231168954
- IROS 2022: https://arxiv.org/pdf/2208.10218
- Acoustruments: https://chrisharrison.net/projects/acoustruments/PrintedAcoustics.pdf
- Stane: https://mural.maynoothuniversity.ie/id/eprint/1735/1/MurWilHugQuaStr08.pdf
- Touch & Activate / TEI 2015: https://dl.acm.org/doi/10.1145/2677199.2680585
- ゲル板: https://www.mdpi.com/1424-8220/22/24/9951
- 岩瀬 2018: http://www.interaction-ipsj.org/proceedings/2018/data/pdf/INT18008.pdf
- ProtoHole: https://www.semanticscholar.org/paper/9c6cc4cb8f5cc8a084e1bdf04836218f0c1bef3c
- CMU ゲームコントローラ: https://www.cs.cmu.edu/~justinc3/docs/acoustic_game.pdf
- CMU ソフトグリッパ: https://www.cs.cmu.edu/~justinc3/docs/soft_gripper.pdf
- Mikogai RA-L 2020: https://takemura-lab.org/publication.html（doi:10.1109/LRA.2020.3002196）
- Drew ICRA 2021: https://arxiv.org/pdf/2101.11817
- Resonant pneumatic RA-L 2022: https://escholarship.org/uc/item/8ht881qh
- AcousTac: https://doi.org/10.1089/soro.2023.0082 ／ Milli: https://edg.berkeley.edu/wp-content/uploads/2025/05/ICRA25_3910_FI-1.pdf
- Shinoda ARTC 1997: https://ieeexplore.ieee.org/document/606757/
- Alonso-Martín 2017: https://doi.org/10.3390/s17051138
- AirTouch: https://dl.acm.org/doi/10.1145/3313831.3376136
- AST Skin: https://arxiv.org/pdf/2303.17355
- SonicBoom: https://arxiv.org/abs/2412.09878
- VibeCheck: https://arxiv.org/abs/2504.15535
- CMSIS-DSP ベンチ（RP2040）: https://github.com/jptrainor/cmsis-sandbox
- Edge Impulse KWS: https://docs.edgeimpulse.com/docs/tutorials/end-to-end-tutorials/audio/responding-to-your-voice
