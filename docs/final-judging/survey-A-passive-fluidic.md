# 文献調査 A: 受動流体ボディ + 圧力センサ1個で「どこを押されたか」を判別する物理機構

作成日: 2026-10-18
対象: ポンプなし（ユーザが押す/握るだけ）の柔軟ボディに圧力センサが1個だけある構成で、接触位置の情報が圧力信号のどこに載るのかを文献で確認する。
注記: 「未確認」と付けた項目は原著本文を直接読めず、要旨・二次情報・検索要約からの推定であることを示す。

---

## 1. 結論（1段落）

「受動（ポンプなし）・空気・圧力センサ1個」で接触位置を判別したと明示する査読論文は、今回の調査範囲（英語・日本語、約30件の検索、十数本の原著確認）では見つからなかった。最も近い成果は Goshtasbi ら（Nature Communications 2026）で、位置情報は**(d) 流体の粘性**と**(b) 流路の流体抵抗（長い細い流路）**の組み合わせによって生まれる。つまり、押した点から流路両端までの流路長が左右で異なると、粘性流体が両端へ押し出されるときの流体抵抗が非対称になり、**左右2個のセンサの差圧 ΔP = PL − PR と、各端のピーク値・立ち上がり時間**に位置が符号化される。同論文は空気・水・シリコーンオイル(100/200/1000 cSt)を比較し、**空気では圧力変動が最小で立ち上がり/立ち下がり時間が位置によらずほぼ同じ**（=位置情報が失われる）ことを明示しており、100 cSt オイルを採用している。空気で位置を取った例は、(i) 圧力波の到達時間差を複数センサで測る方式（Okunaka ら ROBIO 2018; Thuruthel ら Sensors 2021 は 200 kHz サンプリング）、(ii) 空気室内のマイクで音響スペクトルを見る方式（Wall ら IJRR 2023: 受動音で6箇所 47%、能動音で93%）に限られ、いずれも「センサ複数」または「kHz 帯の音響」を要する。**(a) チャンバ形状**と**(c) 壁の力学**で位置を取る方式（Piacenza 2018, De Clercq 2022）は、1個のインデントが複数センサに異なる重みで漏れる「クロストーク」を学習するもので、センサ 5〜15 個が前提である。Kawase ら（Sensors and Materials 2021）の空気圧リザバーは「細長い管（内径 2 mm, センサ間隔 1.4 m）が圧力伝播を遅延・非線形化し、管上の複数点の圧力に時間的多様性が生まれる」ことを示しており、(b) 流路 RC の原理的裏付けになるが、センサは4個で、接触位置推定は扱っていない。したがって本件の設計（空気・TPU・1個・40 SPS）で位置判別を主張するなら、既存文献の延長ではなく「複数チャンバを異なるオリフィス/流路で1センサに集約し、時定数の違いで区別する」という独自仮説として立て、自前データで示す必要がある。

---

## 2. 論文一覧

| # | 論文 | 媒体 / 年 | 流体 | センサ数・種類・レート | 位置を載せる信号特徴 | 位置数 / 精度 | 受動か | 判別機構の分類 |
|---|---|---|---|---|---|---|---|---|
| 1 | Goshtasbi, Berghuis, Parvaresh, Murali Babu, Style, Rafsanjani, *Tactile perception through fluid–solid interaction* | Nat. Commun. 17:5874, 2026 | シリコーンオイル 100 cSt（空気・水・200/1000 cSt も比較） | 差圧センサ 2 個（Honeywell SSCDRRV100MDAA5, ±100 mbar）+ NI USB-6009; レート未確認 | ΔP = PL − PR、左右ピーク値、左右の立ち上がり/立ち下がり時間 | 1D: 41 点, 位置誤差 1.8 mm（R²=99.18%）; 2D: 31×31 格子, RMSE X 3.3 mm / Y 4.2 mm | 受動（押すだけ） | (d) 粘性 + (b) 流路長の非対称 |
| 2 | Kawase, Miyazaki, Kanno, Tadano, Nakajima, Kawashima, *Pneumatic Reservoir Computing for Sensing Soft Body* | Sensors and Materials 33(8):2803–2824, 2021 | 空気（約 100 kPaG 封入, 閉系・流量なし） | SMC PSE530 ×4（管上に等間隔）; NI USB-6211, 31.25 kHz → 50 Hz に再サンプル | 管上複数点の圧力時系列（遅延・非線形の多様性）をリッジ回帰 | 接触位置は対象外。大腿角 RMSE 5.5°（D=2 mm, L=1.4 m） | 受動（歩行でPARMが変形） | (b) 流路 RC（管径・管長） |
| 3 | Okunaka, Ikemoto, Hosoda, *A New Concept of Pneumatic Tactile Sensor using Pressure Wave Propagation in a Soft Chamber* | IEEE ROBIO 2018, pp.1809–1813 | 空気 | 単一チャンバ内に圧力センサ複数; レート未確認 | 圧力波の到達時間差 | 数値未確認 | 受動 | (e) 到達時間差（波動） |
| 4 | Thuruthel, Bosman, Hughes, Iida, *Soft Self-Healing Fluidic Tactile Sensors with Damage Detection and Localization Abilities* | Sensors 21, 2021 | 空気 | 力: 1 個; 1D 位置: 2 個; 2D: 2 個以上; 200 kHz, 16 bit | 到達時間差 t2−t1 = 2x/v（多点測位） | 1D 誤差 1.13±1.41 cm; 2D 2.87±2.26 cm（経路長の約15%） | 受動（損傷イベント） | (e) 到達時間差 |
| 5 | Wall, Zöller, Brock, *Passive and Active Acoustic Sensing for Soft Pneumatic Actuators* | IJRR 2023 | 空気（PneuFlex 空気室） | MEMS マイク Adafruit SPW2430 ×1 + スピーカ ×1; 48 kHz/32 bit | 周波数スペクトル（共鳴ピークのシフト） | 6 箇所: 受動 47% / 能動 93%; 30 点回帰: 受動 RMSE 18.0 mm / 能動 3.7 mm | 受動音と能動音の両方 | (e) 音響（空気室の共鳴） |
| 6 | Mikogai, Kamakura, Takemura ほか, *Contact Point Estimation along Air Tube Based on Acoustic Sensing of Pneumatic System Noise* | IEEE RA-L 5(3):4618–4625, 2020 | 空気（コンプレッサ稼働中） | マイク; CNN | コンプレッサ騒音のスペクトル変化 | 16 m のエアチューブ上の接触点推定（数値未確認） | 能動（コンプレッサ騒音を音源） | (e) 音響 |
| 7 | Piacenza, Sherman, Ciocarlie, *Data-driven Super-resolution on a Tactile Dome* | IEEE RA-L 3(3):1434–1441, 2018 | 空気（Takktile 気圧計の通気孔） | Takktile 気圧センサ ×5 を Vytaflex 20 に埋設 | 5 センサへのクロストーク比（カーネルリッジ回帰） | 半球 1300 mm²（φ54 mm 球の一部）, 中央値誤差 1.1 mm（実機 1.4 mm） | 受動 | (a)(c) 形状・壁力学のクロストーク |
| 8 | De Clercq, Sianov, Crevecoeur, *A Soft Barometric Tactile Sensor to Simultaneously Localize Contact and Estimate Normal Force…* | IEEE RA-L 2022 | 空気（MEMS 気圧計） | MS5840 ×15（3×5）, 20 Hz | 15 点圧力分布への 2D ガウシアン最小二乗フィット | 中央値誤差 0.51 mm; 力誤差 10%（0–25 N） | 受動 | (c) 壁力学（Shore 15A, 7 mm） |
| 9 | Lampinen, Koivikko, Pihlajamäki, Sharma, Sariola, *Soft Micropneumatic Touchpad* | Adv. Intell. Syst. 2025 | 空気（供給圧 60 kPa を常時流す） | 32 チャンネル（行16+列16）, 各チャンネルに圧力分圧器 | 押された行・列チャンネルの流体抵抗増加 | 16×16 格子の再構成（数値未確認） | 能動（供給圧必須） | (b) 流路抵抗（断面つぶれ） |
| 10 | Lampinen ほか, *Soft Pneumatic Tactile Pressure Sensor Matrix* | Robotics Reports 2(1):115–127, 2024 | 空気（流す） | 4 センサ（蛇行マイクロ流路） | 流路の圧力降下 | 接触点と力の推定（3×3 格子 NN は未確認） | 能動 | (b) 流路抵抗 |
| 11 | Nakajima, Itoh, Minato, Takahashi, Nishimura, *Emoballoon*（風船型インタフェース） | インタラクション 2013 / Pervasive 2014 | 空気（風船） | 気圧センサ SCP-1000 ×1 + ダイナミックマイク AT810F ×1（44.1 kHz） | 内気圧変化 + 音響スペクトル（150 ms 窓, SVM） | 7 種の動作 83.5%（被験者内）。**接触位置は非対象**（将来課題として言及） | 受動 | 位置なし |
| 12 | Chen, Kubik, Himmel, Hoffmann, Lee, *Pneumatic-Tomographic Tactile Skin* | 2026（arXiv 2503.13036） | 空気 | 空気圧層 1 スカラ + EIT 電極 | 空気圧層は「力のスカラ値のみ」、位置は EIT | 単一接触 力 RMSE 0.59 N | 受動 | 位置は EIT 側（否定的証拠） |
| 13 | Tawk ほか, *Soft Pneumatic Sensing Chambers (SPSC)* | Adv. Intell. Syst. 2019 | 空気 | 3D プリント TPU チャンバ 1 個につき圧力センサ 1 個 | 圧力値（圧縮・曲げ・ねじり・変位） | チャンバ単位の検出のみ（チャンバ内の位置なし） | 受動 | 位置なし（1 チャンバ = 1 値） |
| 14 | Zhao ほか, *Transparent Pneumatic Tactile Sensors for Soft Biomedical Robotics* | Sensors 23, 2023 | 空気 | Amphenol ELVH-M500G ×1, 10 Hz; チューブ内径 0.8 mm | 圧力値（硬さ） | 位置なし | 受動 | 位置なし |
| 15 | Ovee, Arnob, Louf, *Plant-Inspired Elastic–Hydraulic Tactile Sensing Enables Quantitative Stiffness Estimation in Soft Robots* | Soft Robotics 2026 | 液体（種類未確認） | 圧力センサ 1 個（液体充填流路） | 圧力 + 変形量 → 対象のヤング率 | 位置なし | 受動（準静的） | 位置なし |
| 16 | Li, Huh, Yahnker, Stuart, *Resonant Pneumatic Tactile Sensing for Soft Grippers* | IEEE RA-L 7(4), 2022 | 空気（流す; <34 kPa） | 遠隔マイク 1 個（44.1/48 kHz） | 共鳴周波数 fr（管長 90 mm, φ7.1 mm; 指は φ6 mm×100 mm）と横穴の閉塞 | 穴 1 箇所の接触/近接（<10 mm）; 位置の連続推定なし | 能動（空気流必須） | (e) 音響共鳴 |

---

## 3. 論文ごとのメモ（数値）

### 3.1 Goshtasbi ら 2026, Nat. Commun. — 最重要
- URL: https://www.nature.com/articles/s41467-026-72497-3 / PMC: https://pmc.ncbi.nlm.nih.gov/articles/PMC13334043/ / arXiv:2610.03773 / コード: https://github.com/Arman-Goshtasbi/Fluid-Tactile-Sensor
- 構造: Ecoflex 00-30 のブロック（1D: 幅20 mm×厚5 mm、長さ l = 50/100/150 mm）に直線流路（φ1, 1.5, 2 mm）。2D: 60×60×5 mm に 4 次ヒルベルト曲線（断面 1.8 mm 角、φ2 mm 相当）。流路は両端で外部の差圧センサ 2 個に接続。ポンプなし。
- 流体: 空気、水(1 cSt)、シリコーンオイル 100/200/1000 cSt を比較。主実験は 100 cSt。
- 押し方: インデンタ速度 100 mm/s、保持 2 s。
- 特徴量: 1D は ΔP = PL − PR と左右最大圧。2D は両端最大圧に加え「立ち上がり時間 tL_r と立ち下がり時間 tR_r」（例: trR = 3.05 s vs trL = 2.2 s）。
- 学習: Fuzzy C-means + ANFIS。80/20 分割。
- 精度: 1D 位置 1.8 mm（R² 99.18%, RMSE 0.026 正規化）、力 0.4 N RMSE（1–10 N）。2D 位置 RMSE X 3.3 mm / Y 4.2 mm（平均誤差 5%/7%）、力 RMSE 0.66 N。
- **空気に関する明示的記述**: 「空気は圧縮性のため圧力変動が最小」「空気と水は立ち上がり/立ち下がり時間が位置によらず非常に似ている」「空気は圧力減衰が最速」。オイルは粘性が高いほど逆流が抑えられ ΔP が大きい。200 cSt で立ち上がり+立ち下がり約 2.5 s、1000 cSt で約 10 s → 応答時間とのトレードオフで 100 cSt を選択。
- 表面加工: 不要（エラストマ表面を直接押す）。水中（10/30/50 mm）でも差 <5 Pa、磁気干渉なし。
- 限界: シリコーンオイルがシリコーンゴムへ拡散して圧力が徐々に低下／流路が長いほど圧力散逸が増す／応答が秒オーダー／2D では隣接流路間で圧力–力特性が不均一。
- サンプリングレート: 未確認（NI USB-6009 は最大 48 kS/s 集約）。

### 3.2 Kawase ら 2021, Sensors and Materials — 空気圧リザバー
- URL: https://doi.org/10.18494/SAM.2021.3345 （PDF: https://sensors.myu-group.co.jp/sm_pdf/SM2657.pdf）
- 原理: 2 つのチャンバを 1 本の管で結び、管上の K 点の圧力を線形回帰で読む。管が太いと圧力が瞬時に均一化して情報が消え、適切な径・長さで遅延と非線形性が生まれる。
- シミュレーション: 内径 D = 1〜2.5（0.25 刻み）, 3, 3.5, 4 mm; センサ間隔 L = 0.6/1.0/1.4/1.8 m（管全長 3/5/7/9 m に相当）; Δt = 5×10⁻⁷ s, Δx = 0.2 m; 入力 200→205 kPa のステップ。NARMA 模倣の RMSE は D ≈ 2 mm で最小、メモリ容量は D ≈ 2 mm かつ L ≥ 1.0 m で最大（上限約 3）。D = 1 mm は伝播が遅すぎて定常値に漸近、D = 4 mm はセンサ間の差がなくなる。
- 実機: Bridgestone PARM（長さ 0.3 m, 内径 9.5 mm）×2 をチャンバとして Pisco UBT ポリウレタン管で接続。SMC PSE530 ×4 を管上に配置。D=2 mm/L=1.4 m, D=4 mm/L=1.4 m, D=2 mm/L=0.6 m の 3 条件。NI USB-6211 で 31.25 kHz 取得→50 Hz に再サンプル。封入圧約 100 kPaG。**流量はなし（閉系）**。
- 結果: 大腿角 RMSE 5.5°（D=2 mm, L=1.4 m, 3–4 km/h）。D=4 mm は圧力信号の実効次元が下がり誤差増。
- 限界: 管内空気は低周波を位相遅れほぼなしで通すため、周波数特性からの評価が今後の課題。接触位置推定は扱っていない（大学サイトの要約で「touch localization」とあるのは誤り）。

### 3.3 Okunaka, Ikemoto, Hosoda 2018, ROBIO
- URL: https://doi.org/10.1109/ROBIO.2018.8665065 （著者ページ: https://kohhosoda.github.io/papers.html）
- 単一ソフトチャンバ内に複数の圧力センサを置き、接触で生じる圧力波の到達時間差から位置を求める概念実証。空間分解能がチャンバ寸法に縛られないと主張。
- チャンバ寸法・サンプリング・精度: 未確認（IEEE 本文に到達できず）。続報として Ohara ら「空気圧の到達時間差に基づく平面触覚センサ」（ROBOMECH 2019）。

### 3.4 Thuruthel ら 2021, Sensors（自己修復流体触覚センサ）
- URL: https://pmc.ncbi.nlm.nih.gov/articles/PMC8706411/
- 空気充填チャンバ（超分子自己修復ポリマー）。力は 1 センサ、1D 位置は 2 センサ、2D は 2 個以上。**200 kHz, 16 bit** で損傷時の高周波成分と到達時間差 t2−t1 = 2x/v を取る。
- 精度: 1D 1.13±1.41 cm、検出率 93%（31/33）; 2D 2.87±2.26 cm。
- 限界: より高感度なセンサが必要、反射を無視、1 センサでは方向分離に仮定が要る、材料のひずみ緩和でベースラインがドリフト。

### 3.5 Wall, Zöller, Brock 2023, IJRR（受動/能動音響）
- URL: https://doi.org/10.1177/02783649231168954 / PDF: https://www.static.tu.berlin/fileadmin/www/10002220/Publications/Wall_2023_IJRR.pdf / arXiv:2208.10299
- PneuFlex（シリコーン、指全長の空気室）の空気室両端に MEMS マイク（Adafruit SPW2430, 100 Hz–10 kHz）とスピーカを埋設。48 kHz/32 bit、振幅スペクトルを特徴量、kNN/SVC。
- 6 接触位置×25 回 = 150 サンプル。受動（環境音のみ）47%、動的（叩いた音）52%、能動（1 s スイープ）93%。30 点の連続回帰: 受動 RMSE 18.0 mm、能動 3.7 mm。
- 受動でも「軽い接触で音が変わる」が、精度は能動の約半分。
- 関連: Wall & Brock IROS 2022「Virtual 2D Tactile Array」は能動音で 29×4 ピン（点字）を X RMS 1.67 mm で読む。

### 3.6 Mikogai ら 2020, RA-L（エアチューブ上の接触点）
- URL: https://ieeexplore.ieee.org/document/9117000/
- コンプレッサ騒音がエアチューブを伝わるのをマイクで取り、CNN で 16 m チューブ上の接触点を推定。追加音源不要だがコンプレッサ稼働が前提。精度数値は未確認。

### 3.7 Piacenza, Sherman, Ciocarlie 2018, RA-L（Tactile Dome）
- URL: https://arxiv.org/abs/1802.09435
- Vytaflex 20 の半球（φ54 mm 球の切片、面積 1300 mm²）に Takktile 気圧センサ 5 個を埋設。各センサは通気孔で圧力を取る。「クロストークが大きい配置ほど位置精度が上がる」ことを FEA と実機で検証。
- 16×16 格子、深さ 0–3 mm（0.5 mm 刻み）、カーネルリッジ回帰（ラプラシアン核）。中央値誤差: シミュレーション 0.9–2.6 mm、実機 1.4 mm（最良ケース 1.1 mm）。
- 本件との関係: 位置情報は「壁の力学（弾性体内の応力分布）」が複数センサに異なる比率で分配されることに由来。センサ 1 個では比率が取れない。

### 3.8 De Clercq, Sianov, Crevecoeur 2022, RA-L（Barometric tactile sensor）
- URL: https://lacuna.tiptreesystems.com/work/a-soft-barometric-tactile-sensor-to-simultaneously-localize-contact-and-estimate/wrk_2d651b9af6f55e2089692c28207c4303
- MS5840 ×15（3×5）を Shore 15A シリコーン 7 mm で封止、20 Hz。15 値に 2D ガウシアンを最小二乗フィット。中央値誤差 0.51 mm、力誤差 10%（0–25 N）/15%（25–50 N）。約 80 EUR。多点接触は不可。

### 3.9 Lampinen ら 2024/2025（マイクロ空気圧タッチパッド）
- URL: https://doi.org/10.1002/aisy.202400381 / https://doi.org/10.1089/rorep.2024.0034
- 32 チャンネル（200 µm 角断面、長さ 40 mm、間隔 2.65 mm、有効 40×40 mm）。各チャンネルを定抵抗と直列にして供給圧 60 kPa で分圧（電気の分圧回路の類推）。押された行・列の流体抵抗が上がる。
- 2024 版（4 センサ）: 0.1–4 N、感度 50.5 N⁻¹、ヒステリシス 3.9%、12 h ドリフト 16.4%。検索要約によれば「チューブ 5 cm で時定数 27±6 ms、帯域 37 Hz、長いチューブで 100±30 ms」（出典箇所は未確認）。
- 本件との関係: 流路抵抗で位置を取る (b) の典型だが、**定常流が必須**（受動では成立しない）。

### 3.10 Emoballoon（中島ら, インタラクション 2013）
- URL: http://www.interaction-ipsj.org/archives/paper2013/data/Interaction2013/oral/data/pdf/13INT013.pdf
- 風船（φ25 cm に膨らませた最大 φ40 cm のゴム風船）内に気圧センサ SCP-1000（秋月）とダイナミックマイク AT810F。マイク 44.1 kHz、1024 サンプルごとに FFT。150 ms 窓・重複 33.3% で特徴量、SVM。
- 7 動作（抱擁・殴打・押下・撫でる・平手・つつく・何もしない）を被験者内 83.5% で識別。**接触位置は非対象**で、将来課題として「タッチ位置を検出可能なフレキシブルセンサ」との組み合わせを挙げている。
- 本件との関係: 受動・空気・圧力センサ 1 個で「動作の種類」は識別できるが、「位置」は圧力だけでは取っていない。

### 3.11 否定的証拠
- Chen ら 2026（Pneumatic-Tomographic Skin, arXiv 2503.13036）: 空気圧層は「安定なスカラの力」としてのみ使い、位置は EIT に委ねる。単一チャンバ空気圧に位置情報を期待していない設計。
- Tawk ら 2019（SPSC, TPU 3D プリント）: 1 チャンバ = 1 圧力センサ = 1 値。チャンバ内の位置は扱わない。
- Zhao ら 2023（透明空気圧触覚）: φ10 mm ドーム、壁 1.5 mm PDMS、チューブ内径 0.8 mm、10 Hz。力/硬さのみ。
- Ovee ら 2026（植物着想・液圧）: 液体流路 + 圧力センサ 1 個で対象の弾性率を推定。位置なし。

---

## 4. 受動・空気充填 TPU ボディを 40 SPS で読む場合への含意（kHz との比較）

1. **空気では「粘性による流路 RC 符号化」がほぼ働かない。** Goshtasbi らは同一形状で空気・水・オイルを比べ、空気は ΔP が最小で立ち上がり時間が位置によらないと報告している。本件の「空気 + 1 センサ」は、同論文の最も情報量の少ない条件に、さらにセンサを 1 個減らした構成である。位置情報を期待するなら、流路の細径化・長尺化で流体抵抗を意図的に作る（Kawase: D ≈ 2 mm で管長 m オーダー）か、チャンバごとに異なるオリフィス径を付けて時定数差を作る、という設計上の仕掛けが必要になる。

2. **到達時間差・音響は 40 SPS では原理的に不可能。** 空気中の音速 ~343 m/s では、10 cm の経路差が ~0.3 ms。Thuruthel らは 200 kHz、Wall らは 48 kHz、Kawase らですら 31.25 kHz で取得している。40 SPS（25 ms 周期）では (e) 系の機構は使えず、Emoballoon のように「マイクを別途追加」する以外に音響的手がかりは取れない。

3. **40 SPS で見えるのは、準静的な圧力振幅と数百 ms〜秒オーダーの時定数だけ。** 空気圧系の時定数は Lampinen らの報告（チューブ 5 cm で 27 ms、長いチューブで ~100 ms、未確認）から、通常のチューブ径では 25 ms 周期に対して速すぎて 1〜4 サンプルしか立ち上がりに掛からない。位置を時定数差で出すなら、オリフィスを絞って時定数を 200 ms〜1 s 程度に伸ばし、各チャンバで時定数が 2 倍以上違うように設計する必要がある（Goshtasbi らの 2D 版で使った rise/fall time 特徴量がこの考え方に相当。ただし彼らは粘性オイルで 2〜3 s の時定数を得ている）。

4. **壁の力学 (c) とチャンバ形状 (a) は、1 センサでは「振幅」にしか現れない。** Piacenza/De Clercq の方式は複数センサ間の比率を使うので、1 センサでは「押した強さ」と「押した場所の壁の硬さ」が同じ振幅軸に縮退する。TPU の壁厚を場所ごとに変えれば振幅–力曲線の形（非線形性、飽和点）が場所で変わる可能性はあるが、ユーザの押し力が未知の受動入力では分離できない。これを分離するには、Goshtasbi らのように「ピーク値」と「時間特徴」の 2 軸以上を持つ必要がある。

5. **実務的な設計指針（文献から導けるもの）**
   - 1 つの大きな空気室ではなく、複数の小チャンバを各々異なる流路（長さ/内径）で 1 本のマニホールドに集め、センサをマニホールド側に置く。Kawase の結果では D = 1 mm だと遅すぎて定常値に漸近、D = 4 mm だと差が消えるので、2 mm 前後で長さを変えるのが出発点。
   - 判別特徴は「ピーク圧」ではなく「立ち上がりの時定数」「ピーク後の減衰時定数」「離したときの戻り時間」。40 SPS でも時定数 ≥ 250 ms なら 10 サンプル以上で推定できる。
   - ユーザの押し速度のばらつきは Goshtasbi らでも 100 mm/s に固定していた点に注意。受動入力では速度が未知なので、時定数は「押し込み後の緩和」や「離した後の戻り」から取るほうが押し方の影響を受けにくい。
   - 空気は温度で圧力が変わる（Kawase の PARM も温度補償なし、Tawk の SPSC 系は BMP280 で環境圧補償）。長時間運用では基準圧のドリフト補正が要る。

6. **主張の強さの線引き。** 「空気・1 センサ・40 SPS で接触位置を判別した」という既報は見つからないので、審査資料では「文献では粘性流体 + 2 センサ（Goshtasbi 2026）または空気 + 複数センサ/kHz 音響が前提で、空気・1 センサ・低レートで位置を取るのは本作品の独自の試みである」と位置づけるのが正確。

---

## 5. 未解決の点（Open gaps）

- **空気・1 センサ・受動で位置を取った査読論文は未発見。** 近い HCI 事例（Emoballoon, Inflatable Mouse, Compressables）はいずれも「動作の種類」や「握り強さ」であり、位置は対象外。
- Goshtasbi らの空気充填条件での位置精度は定量的に示されていない（「情報が少ない」という定性記述のみ）。空気で何 mm まで落ちるかは不明。
- Okunaka ら 2018 の到達時間差方式は、チャンバ寸法・サンプリング周波数・精度の数値を確認できなかった（IEEE 本文未取得）。
- Lampinen らの時定数（27 ms / 100 ms）の出典箇所と条件は未確認。
- Kawase らの空気圧リザバーを「接触位置推定」に転用した続報は見つからなかった（同研究室の後続は PAM のリザバー計算や歩容推定）。
- 「複数チャンバ + 異なるオリフィス径 + 単一センサ」で時定数差を使って押されたチャンバを識別する、という構成の学術報告は見つからなかった（特許検索では空気ブリードで時定数を調整する障害者向け信号装置 US6771190 のみ）。この構成は本作品の新規性の根拠になりうるが、同時に先行検証がないことも意味する。
- TPU（熱可塑性ポリウレタン）製の受動空気室の圧力応答に関する時定数データは、SPSC（Tawk 2019; Duretto 2026 は TPU 60A で剛性 5.0→8.0 N/mm）以外に見つからず、TPU 壁厚と応答の関係は自前計測が必要。

---

## 参照 URL 一覧

- Goshtasbi et al. 2026: https://www.nature.com/articles/s41467-026-72497-3 ; https://pmc.ncbi.nlm.nih.gov/articles/PMC13334043/ ; https://arxiv.org/abs/2610.03773 ; https://github.com/Arman-Goshtasbi/Fluid-Tactile-Sensor
- Kawase et al. 2021: https://doi.org/10.18494/SAM.2021.3345 ; https://sensors.myu-group.co.jp/article.php?ss=3345
- Okunaka et al. 2018: https://doi.org/10.1109/ROBIO.2018.8665065 ; https://kohhosoda.github.io/papers.html
- Thuruthel et al. 2021: https://pmc.ncbi.nlm.nih.gov/articles/PMC8706411/
- Wall et al. 2023: https://doi.org/10.1177/02783649231168954 ; https://arxiv.org/abs/2208.10299 ; https://www.static.tu.berlin/fileadmin/www/10002220/Publications/Wall_2023_IJRR.pdf
- Wall & Brock 2022: https://arxiv.org/abs/2208.10218
- Mikogai et al. 2020: https://ieeexplore.ieee.org/document/9117000/
- Piacenza et al. 2018: https://arxiv.org/abs/1802.09435
- De Clercq et al. 2022: https://lacuna.tiptreesystems.com/work/a-soft-barometric-tactile-sensor-to-simultaneously-localize-contact-and-estimate/wrk_2d651b9af6f55e2089692c28207c4303
- Lampinen et al. 2025: https://doi.org/10.1002/aisy.202400381 ; 2024: https://doi.org/10.1089/rorep.2024.0034 ; https://researchportal.tuni.fi/en/publications/soft-pneumatic-tactile-pressure-sensor-matrix/
- Emoballoon 2013: http://www.interaction-ipsj.org/archives/paper2013/data/Interaction2013/oral/data/pdf/13INT013.pdf
- Chen et al. 2026: https://arxiv.org/abs/2503.13036
- Tawk et al. 2019: https://doi.org/10.1002/aisy.201900002 ; Duretto et al. 2026: https://www.sciencedirect.com/science/article/pii/S0924424726003183
- Zhao et al. 2023: https://pmc.ncbi.nlm.nih.gov/articles/PMC10302895/
- Ovee et al. 2026: https://doi.org/10.1177/21695172261453210 ; https://zenodo.org/records/20777220
- Li et al. 2022: https://escholarship.org/uc/item/8ht881qh
- Jenkinson et al. 2023 (ESPRESS.0): https://pmc.ncbi.nlm.nih.gov/articles/PMC9860791/
- Joshi & Paik 2023 (sensorless estimation): https://pmc.ncbi.nlm.nih.gov/articles/PMC10074228/
- 田中ら 2010（バルーン膨張触覚）: https://www.jstage.jst.go.jp/article/jrsj/28/7/28_7_811/_pdf
