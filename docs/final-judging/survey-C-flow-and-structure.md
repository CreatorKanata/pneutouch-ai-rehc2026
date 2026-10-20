# 調査C: 1個の圧力/流量センサと内部形状設計でタッチ位置を特定する手法

作成日: 2026-10-18 / 文献調査(Web)のみ。原文を取得できなかった数値には「未確認」を付す。

---

## 1. 結論(1段落)

「センサ1個+内部形状で位置を符号化する」研究は、大きく **(a) 能動的に空気を流し、穴を指で塞いだときの定常圧上昇量を出口面積で符号化する AirTouch(CHI 2020)**、**(b) 定常流の圧力分圧器(pressure divider)で、押された皮下流路の流路抵抗変化を圧力として読む Sariola 研(Robotics Reports 2024 / Adv. Intell. Syst. 2025)**、**(c) 空気流を音に変え、空洞・管長で周波数を符号化する Blowhole(GI 2018)/Resonant pneumatic sensing(RA-L 2022)/AcousTac(2023)/SqueezaPulse(TEI 2017)**、**(d) 受動的な機械バーコードを 1 個のマイクで読む Lamello(CHI 2015)/Acoustruments(CHI 2015)** に整理できる。真に「圧力センサ1個のみ・外皮無加工・ポンプあり」で複数位置を識別した査読論文は AirTouch だけで、それも表面に 0.6〜1.5 mm の出口穴が必要である。Sariola 研の方式は外皮無加工かつ本体に電子部品ゼロだが、流路ごとに圧力計を置く(4 本/32 本)ため「単一センサ」ではない。受動的(ポンプなし)に絞った体でオリフィス径・室形状が ID を符号化する方式は、査読論文では見つからず、特許(Alcon US10987183: 1 本のチューブ上に容積の異なる複数バルーン→振幅で判別、原文該当箇所は未確認)と理論的な RC 時定数の考え方が存在するのみで、ここが未踏領域(gap)である。

---

## 2. 一覧表

| # | 文献 | 原理 | センサ/レート | 能動源 | 外皮 | 位置を符号化する幾何パラメータ(設計則) | 位置数/精度 | 主な限界 |
|---|---|---|---|---|---|---|---|---|
| 1 | AirTouch (Tejada, Ramakers, Boring, Ashbrook; CHI 2020) | 定常流+穴塞ぎ→内圧上昇。ΔP_x = (ΣA)²ΔP/(ΣA−A_x)² | Panasonic ADP5 気圧センサ 0–6 kPa、Arduino Uno 5 kHz、ノイズ ≈0.01 kPa | コンプレッサ+Festo MS4-LR レギュレータ(定流量) | **穴が必要**(φ0.6–1.5 mm) | **出口面積** A_i を 0.35–1.45 mm² で 0.1 mm² 刻み。管長(3–100 mm)は影響なし。分配室 φ30 mm 球、管 φ5 mm | 最大12箇所。95.5 % / 100 % / 97.75 %、2点同時 91.6 % | 高解像 SLA 必須(FDM は漏れ)、コンプレッサ必要、出口径の印刷限界で >12 不可、押圧の強弱は不要だが完全に塞ぐ必要 |
| 2 | Soft Pneumatic Tactile Pressure Sensor Matrix (Lampinen, Koivikko, Pihlajamäki, Sharma, Sariola; Robotics Reports 2024) | 蛇行微細流路の流路抵抗変化を圧力分圧器で読む | 各流路に圧力計(4 個)。モデル・レート未確認 | 定圧供給(60 kPa、未確認) | 無加工、本体に電子部品なし | **流路の変形**(断面縮小→抵抗増)。基準抵抗(蛇行流路)と直列 | 4 センサで接触点と力。<0.1–4 N、感度 50.5 N⁻¹、ヒステリシス 3.9 %、ドリフト 16.4 %/12 h | センサは流路ごと、ドリフト大 |
| 3 | Soft Micropneumatic Touchpad (Lampinen, Pihlajamäki, Koivikko, Sariola; Adv. Intell. Syst. 2025) | 行16+列16 の流路の抵抗を分圧器で測りスパース回帰で 16×16 圧力マップ再構成 | 流路ごとに圧力計(32 ch)。モデル未確認 | 定圧 60 kPa、大気開放 | 無加工、電子部品なし | 流路断面 200×200 µm、長さ 40 mm、ピッチ 2.65 mm、有効 40×40 mm(全体 68×68 mm) | 位置・深さ・面積・2点同時、手書き文字認識。τ=55 ms、帯域 18 Hz | 32 本の計測が必要、微細流路(モールド) |
| 4 | Blowhole (Tejada, Fujimoto, Li, Ashbrook; GI 2018) | 息で Helmholtz 共鳴器を鳴らし基本周波数で穴を識別 | ノート PC 内蔵マイク 44.1 kHz、Welch 法で f0 | **ユーザの息**(電源不要) | **穴 φ5 mm 必要** | **球空洞径 d_s(8–40 mm)と管長 L_t(2.5–10 mm)** → 500–5900 Hz | 6 球(8–28 mm, L_t=2.5 mm)で 98 %、7 球で 90 %、実用上 ≤9 | 球空洞の体積が大きい、FDM 粗さで周波数ずれ(<240 Hz) |
| 5 | Lamello (Savage, Head, Hartmann, Goldman, Mysore, Li; CHI 2015) | 可動部が長さの異なる片持ち梁(tine)を弾き、f0 で位置を判別 | コンタクトマイク 16 kHz、2048 点フレーム(128 ms) | なし(受動) | 可動部品が表面に露出 | **tine 長 L**(f0 ∝ h/L²)。400 Hz(4×50×6 mm)〜4 kHz。ABS で厚み:長さ=1:25〜1:2。de Bruijn 系列で少数 f0 から位置復元 | 4 tine スライダ(924/1103/1340/1662 Hz) 精度 93 %/再現 90 %。>2 kHz の 7 tine で 72 % | 高周波が分類不能、打撃音ノイズ |
| 6 | Acoustruments (Laput, Brockmeyer, Hudson, Harrison; CHI 2015) | スマホのスピーカ→管→マイクに 16.5–22.05 kHz 掃引を通し、管内構造で周波数応答を変える | スマホマイク 44.1 kHz、FFT >15.5 kHz +1・2階微分 → SMO-SVM | スピーカ(能動超音波) | 穴・機構が表面に露出 | **穴・管長・空洞・隙間・テクスチャ・分岐・弁**。穴間隔 ≥7.5 mm、穴は ≤5 個、肉厚 1.0 mm | 2値センサ 99.5 %(200 試行)、ノブ 0–240° 9 位置を 10 例で回帰 | スマホ密着が前提、学習が必要 |
| 7 | Resonant pneumatic tactile sensing (Li, Huh, Yahnker, Stuart; RA-L 2022) | 圧縮空気で fipple(笛)を鳴らし、管長で基本周波数を分離。接触穴の開閉で境界条件が変わる | 遠隔マイク(1 m)44.1/48 kHz、33 ms Hamming | 圧縮空気 <34 kPa、4 m ホース | 音の出口+接触穴 φ3.2 mm | **共鳴管長 L**(f=nc/2L)。90 mm/φ7.1 mm → ≈4 kHz。管長の異なる taxel は帯域が重ならない | 周波数の標準偏差 1.65 Hz、感度 113 Hz/N | 流量・気温で周波数が動く、空気流なしでは無音 |
| 8 | AcousTac (Li, Stuart; Soft Robotics 2023) | 同上をシリコーンキャップ付き共鳴管で taxel 化 | 遠隔マイク | 圧縮空気 | キャップが表面 | 管長+キャップ形状で力レンジと周波数を個別化 | 4 taxel アレイ、3 taxel グリッパ(精度未確認) | 同上 |
| 9 | SqueezaPulse (He, Laput, Brockmeyer, Froehlich; TEI 2017) | 柔らかい空洞を握る→空気パルスが**波形付き(corrugated)管**を通り固有の音になる | マイク+ML 分類 | なし(握る力) | 無加工(管・マイクは内部) | 管長・内径・波形の分布/間隔/形状 | 4 応用(ゲームパッド、ウサギ、ケース、力センサ)。精度数値は未確認 | 波形管は別途成形して挿入、マイク内蔵 |
| 10 | Tactile perception through fluid–solid interaction (Goshtasbi ら; Nat. Commun. 2026) | 粘性油を封入した流路の両端 **2 個**の圧力センサの差圧パターンを ML で位置・力に写像 | 2 個の外部圧力センサ、FCM+ANFIS | なし(封止、受動) | 無加工、電子部品なし | 流路径 1–2 mm、長 50–150 mm、油 100 cSt。2D は 4 次 Hilbert 曲線(□1.8 mm、60×60 mm) | 1D 位置 1.8 mm、力 0.4 N RMSE。2D 3.3/4.2 mm、0.7 N | 応答 2.5–10 s、油の拡散でドリフト |
| 11 | Pneumatic-tomographic tactile skin (arXiv 2503.13036, 2025) | TPE 中空シェル(100×100×5 mm、5 % gyroid)に **圧力センサ 1 個**(XGZP6847A005KPG、20 Hz)+EIT 層 | 1 個 | なし(受動) | EIT 電極が必要 | 位置は EIT で、圧力は総力のみ | 位置 4.4±1.9 mm(EIT)、力 0.59 N RMSE | 圧力単独では位置不可 |
| 12 | 特許 US10987183 / US12478447(Alcon, 2018 出願) | 1 本のチューブに径/容積の異なる複数バルーン→押下時の信号振幅でどのボタンか判別(検索要約で確認、原文該当段落は未確認) | 圧力センサ 1 個 | なし | バルーンが表面 | **バルーン容積** → 振幅 | — | ON/OFF 用途向けと記載 |
| 13 | 特許 US20140256213(個人、2013、放棄) | 単一エアブラダ+センサ1個、しきい値で強度のみ | 1 個 | なし | — | **位置判別なし** | — | — |
| 14 | 特許 US9491929(複数スクイーカー犬用玩具) | 室ごとに異なる音程のスクイーカー | なし(センシングなし) | 握る | — | スクイーカーのリード | — | 認識機能なし |
| 15 | Tawk ら SPSC (Adv. Intell. Syst. 2019) | TPU 3D プリント密閉室の圧力変化 | 室ごとに圧力センサ(個数未確認) | なし | 無加工 | 位置符号化なし(室=センサ) | — | 室数=センサ数 |
| 16 | Fluidic innervation (Truby ら; Sci. Adv. 2022) | 格子内の密閉空気流路、Boyle 則で圧力変化 | 流路ごとに差圧トランスデューサ | なし | 無加工 | 流路の配置(ML) | — | 流路数=センサ数 |
| 17 | AirLogic (Savage, Tejada, Zhong, Ramakers, Ashbrook, Kim; UIST 2022) | 空気論理ゲートを 3D モデルに埋め込み、電子部品ゼロで計算・出力 | センサなし | 空気源 | 入力ウィジェットが表面 | 13 種ウィジェット | — | 圧力測定はしない |
| 18 | ProtoHole (Katakura, Watanabe; CHI EA 2018) | 物体内部でスピーカ→マイクの掃引、穴を塞ぐと共鳴が変化、ML | 内蔵マイク | 内蔵スピーカ | 穴 | 穴の位置 | — | 電子部品内蔵 |

---

## 3. 論文ごとのノート(数値付き)

### 3.1 AirTouch(CHI 2020)— 定常流+出口面積符号化
- URL: https://dl.acm.org/doi/10.1145/3313831.3376136 / PDF: https://carlosetejada.com/documents/papers/airtouch.pdf
- 原理: 連続の式 Q_I = ΣQ_i と Bernoulli Q = C·A·√ΔP から、出口 x を塞いだときの内圧は ΔP_x = (ΣA_i)²ΔP /(ΣA_i − A_x)²(式4)。逆に ΔP_x から A_x = ΣA_i(1 − √(ΔP/ΔP_x))(式5)。著者自身が「非圧縮・定常・理想オリフィスの仮定が成り立たないので目安」と断っている。
- ハード: 空気コンプレッサ+Festo MS4-LR-1/4-D5-AS レギュレータ、6 mm PE チューブ。Panasonic PS-A(ADP5)アナログ気圧センサ(0–6 kPa)を Arduino Uno で **5 kHz** サンプリング。センサノイズ ≈ **0.01 kPa**。出口数に応じてレギュレータ出力を調整して飽和回避。
- 内部構造の設計則: どの物体でも同じ **φ30 mm 球の流量分配室**(15/20/30 mm を試験、容積は出口間の相対差に影響しない)。分配室→各タッチ点は **φ5 mm 管**(φ3 mm は詰まる)。管長 **3–100 mm**(動物モデル)および 50 cm→10 cm 刻みの実験で**管長の影響なし**。40 m 管(2 L)で 2–3 s の遅れ。出口は管末端 1 mm を絞って形成。印刷限界から出口径 0.6–1.5 mm。面積刻み 0.02/0.06/0.1 mm² を比較し、ノイズに対し **0.1 mm² 刻みが必要** → 0.35–1.45 mm² の **12 出口**。
- 2点同時: 面積和が一意になるよう 0.4/0.5/0.6/0.8 mm² を使用(0.1 mm² 分離を保証)。
- 認識: 100 サンプル窓の平均・標準偏差、標準偏差がしきい値超えでタッチ/リリース、直前 1000 サンプル平均を基準圧にして比をとる。**SVM(RBF)**、出口ごとに **学習例 1 個**(平均・標準偏差)。同じ出口構成なら物体形状が違っても **モデルを転用可**(ウサギ・アヒル・CHI-nosaur)。
- 精度: 棒グラフ 95.5 %、Stanford bunny 100 %、色相セレクタ(12 箇所)97.75 %、把持球(2 点)91.6 %。4 日間の気圧変動 0.1–0.7 kPa は基準差分より小さい。
- 限界: コンプレッサ必須(小型ポンプで代替可と示唆)、**Form 2 SLA 必須**(Lulzbot Taz 6/QiDi X-One の FDM は内部漏れ)、>12 箇所は印刷解像度で不可、スワイプ等の連続ジェスチャ未対応。

### 3.2 Sariola 研(Tampere)— 圧力分圧器方式
- 2024: Lampinen, Koivikko, Pihlajamäki, Sharma, Sariola, "Soft Pneumatic Tactile Pressure Sensor Matrix", Robotics Reports 2(1) 115–127, DOI 10.1089/rorep.2024.0034(CC-BY; 出版社/リポジトリとも 403 で本文未取得)。4 センサ、各センサは同一エラストマ製の蛇行微細流路。圧縮で流路の圧力損失が変わる。力 <0.1–4 N、最大感度 50.5 N⁻¹、ヒステリシス 3.9 %、静圧 12 h で 16.4 % ドリフト。4 センサで接触点と力、突起ポストで 3 軸力。
- 2025: Lampinen, Pihlajamäki, Koivikko, Sariola, "Soft Micropneumatic Touchpad", Adv. Intell. Syst. 7(2) 2400381, DOI 10.1002/aisy.202400381(CC-BY; 本文 403)。
  - 構造: 行 16・列 16 の **32 流路**、断面 **200×200 µm**、長さ **40 mm**、ピッチ **2.65 mm**、有効 40×40 mm/全体 68×68 mm(数値は検索要約経由、未確認)。
  - 読み出し: 各流路を **一定の空気抵抗(蛇行流路)と直列**にし、片側に **定圧 60 kPa**、他端は大気開放。中間ノードの圧力を測る=電圧分圧器の空気版。押すと近傍の行/列流路の断面が縮み抵抗増→ノード圧が変わる。
  - 復号: スパース性仮定でスパース回帰、16×16 グリッドの圧力マップを実時間再構成。**τ = 55 ms、帯域 18 Hz**。文字の手書き認識、2 点同時。
  - 「単一センサ」ではなく **32 ch 分の圧力測定**が必要。本体は完全に非電子(MRI 等向け)。
  - 設計則(文献から読める範囲): 流路抵抗 R ∝ L/(w·h³)。押圧で h が減ると R が急増(h³)。基準抵抗 R_ref ≈ 流路抵抗と同程度にすると分圧感度が最大。

### 3.3 Blowhole(GI 2018)— 息+Helmholtz
- URL: https://dl.acm.org/doi/10.20380/GI2018.18 / PDF: https://carlosetejada.com/documents/papers/blowhole.pdf
- Helmholtz: f = (c·d_t/π)·√(3 / (8(L_t + 0.75 d_t) d_s³))。管径 d_t = 5 mm 固定、管長 L_t ∈ {2.5, 3.5, 5, 7.5, 8.5, 10} mm、球径 d_s = 8–40 mm(4 mm 刻み)→ 500–5900 Hz。48 試験体、10 人、830 回。
- 認識: 44.1 kHz、0.1 s 窓 RMS、0.5 s 連続、最強周波数を Helmholtz 予測値と照合(scikit-learn)。
- 精度: L_t = 2.5 mm で球 8–28 mm の 6 個 → **98 %**、32 mm 追加で 90 %。向き 0–180° で平均偏差 <240 Hz。SLA は FDM より 100 Hz 予測に近い。
- 限界: 表面に φ5 mm 穴、空洞が大きい(球)、1 kHz 未満と長管はノイズ大、60 mm 球は共鳴せず。

### 3.4 Lamello(CHI 2015)— 受動的片持ち梁バーコード
- PDF: https://people.eecs.berkeley.edu/~bjoern/papers/savage-lamello-chi2015.pdf
- 片持ち梁 f0 = (1.875²/2π)·√(E·b h³/12 / (ρ b h L⁴)) 相当(論文式)。幅・厚み固定で **長さ L** を変える。400 Hz(4×50×6 mm)〜4000 Hz(7.25×6.0×1.2 mm)。誤差平均 69 Hz(σ 112.5 Hz)。
- コンタクトマイク 16 kHz、2048 点(128 ms)、61 % 重複、FFT エネルギー比を f0 中心のガウシアンで重み付け。
- 符号化: tine ごとに一意 f0、または少数 f0 の **de Bruijn 系列** で位置と方向。
- 精度: 4 tine スライダ P 93 %/R 90 %、手弾き Delrin 98 %/97 %、>2 kHz 7 tine 72 %。
- 限界: プリンタ XY 最小 1.194 mm、Z 0.254 mm、ストライカ雑音、高調波の重なり。

### 3.5 Acoustruments(CHI 2015)— 能動超音波+管内プリミティブ
- PDF: https://s3-us-west-1.amazonaws.com/disneyresearch/wp-content/uploads/20150416201017/Acoustrument-Passive-Acoustically-Driven-Interactive-Controls-for-Hand-Held-Devices-Paper.pdf
- 100 ms 線形掃引 16.50–22.05 kHz、44.1 kHz、スピーカ–マイク間 26.2 mm。FFT >15.5 kHz と 1・2 階微分を SMO-SVM(libSVM iOS 移植)。
- プリミティブ: 穴(間隔 ≥7.5 mm、>5 個で劣化)、管長、空洞、隙間(半波長フィルタ)、テクスチャ、分岐、弁、肉厚 1.0 mm(Objet260 Connex)。
- 精度: 2 値センサ 99.5 %(SD 0.07 %)、ノブ 0–240° 9 位置、目覚まし時計 3 状態 50 例で 1 時間試験 4 誤り。

### 3.6 Resonant pneumatic tactile sensing(RA-L 2022)/ AcousTac(2023)
- PDF: https://escholarship.org/content/qt8ht881qh/qt8ht881qh.pdf、arXiv 2307.09730
- 圧縮空気(<34 kPa, 5 psi、4 m ホース)で fipple を鳴らす。共鳴管 90 mm×φ7.1 mm → 基本 ≈3–4 kHz、f = n c/(2L)。**管長の異なる taxel は周波数帯が重ならない**ので 1 m 離れたマイク 1 個で同時監視。接触穴 φ3.2 mm を塞ぐと境界条件が変わり f_r がシフト(h<2 mm で最大変化)。33 ms Hamming、σ 1.65 Hz、感度 113 Hz/N。
- 限界: 気温・流量で f_r が動く(使用前再校正)、**空気流なしでは無音**、ポンプ騒音。

### 3.7 SqueezaPulse(TEI 2017)
- https://makeabilitylab.cs.washington.edu/project/squeezapulse/ 、DOI 10.1145/3024969.3024976
- 柔らかい空洞を握る→空気パルスが波形付き管を通過→固有音→マイク+ML。管長・内径・波形の分布・間隔・形状が署名を決める。4 応用。精度数値は要原文(未確認)。Blowhole 論文によれば波形管は成形して挿入、マイク内蔵。

### 3.8 受動・流体封入型(比較対象)
- Nat. Commun. 2026 Goshtasbi ら: センサ 2 個、油封入、位置 1.8 mm/力 0.4 N(1D)、応答 2.5–10 s。
- arXiv 2503.13036: 圧力センサ 1 個(XGZP6847A005KPG、20 Hz)は総力のみ、位置は EIT。**圧力 1 個では位置不可** と示す対照例。
- Truby 2022 / Tawk 2019 / Slyper & Hodgins 2012(RO-MAN): 室(流路)ごとにセンサ。

### 3.9 特許・玩具
- US10987183B2 / US12478447B2(Alcon「Ophthalmic surgical systems」): 「1 本のフレキシブル管に径/容積の異なる複数バルーンを設け、押下時の **振幅** でどのボタンか判別、ON/OFF 用途向け」との記載が検索要約で確認された(Google Patents 全文フェッチでは当該段落を特定できず、**未確認**)。
- US20140256213A1: 単一ブラダ+センサ 1 個、しきい値のみ、位置なし、放棄。
- US9491929(Multiple squeaker pet toy): 室ごとに異なる音程のスクイーカー、センシングなし。
- 市販の「どこを握ったか分かる」単一圧力センサ玩具は見つからず(未確認)。

---

## 4. 外皮無加工かつ本体内に電子部品が不要なもの

| 方式 | 外皮 | 本体内電子部品 | 単一センサ | ポンプ |
|---|---|---|---|---|
| Sariola 2024/2025(分圧器) | **無加工** | **なし**(チューブで外部へ) | ×(流路ごと) | 定圧源 60 kPa |
| Nat. Commun. 2026(油封入) | 無加工 | なし | ×(2 個) | 不要 |
| SqueezaPulse | 無加工 | マイク内蔵(外出し可と推測、未確認) | ○(マイク 1) | 不要 |
| Alcon 特許 | バルーン=表面 | なし | ○ | 不要 |
| Tawk / Truby / Slyper | 無加工 | なし | × | 不要 |
| AirTouch | **穴 φ0.6–1.5 mm** | なし | **○** | 必要 |
| Blowhole | 穴 φ5 mm | なし | ○(マイク) | 息 |
| Resonant pneumatic / AcousTac | 音の出口・接触穴 | なし | ○(マイク) | 必要 |
| Lamello / Acoustruments | 可動部露出 | なし/スマホ | ○(マイク) | 不要/スピーカ |
| ProtoHole / EIT 複合 | 穴/電極 | あり | — | — |

→ 「外皮無加工・内部電子ゼロ・ポンプ・圧力センサ 1 個」の組合せは **既存文献に存在しない**。最も近いのは AirTouch(穴あり)と Sariola 分圧器(センサ複数)で、両者の合成が本プロジェクトの新規性になる。

---

## 5. 20 cm TPU 恐竜・7 ゾーンのポンプ式圧力分圧器に必要なもの(式付き)

### 5.1 構成
ポンプ →(基準抵抗 R_ref または定流量)→ マニホールド(圧力センサ 1 個)→ 7 本の皮下流路(各ゾーンに「つぶれ部」+「ID 用固定絞り R_i」を直列)→ 大気開放。+ 固定ブリード抵抗 1 本。
ゾーン i を押すとつぶれ部が閉じ、コンダクタンス G_i = 1/R_i が消える → マニホールド圧が ΔP_i だけ上がる。

### 5.2 式
- 定流量近似(ポンプ=流量源、あるいは R_ref ≫ R_par): P_0 = Q·R_par、 1/R_par = Σ G_i + G_bleed。
  ゾーン i 閉塞時: ΔP_i = Q·G_i / (G_tot·(G_tot − G_i)) = P_0 · g_i/(1 − g_i)、 g_i = G_i/G_tot。
  逆に、望む ΔP_i から g_i = ΔP_i/(P_0 + ΔP_i)。制約 Σ g_i ≤ 1(残りはブリード)。
- 定圧源+基準抵抗: P_0 = P_s·R_par/(R_ref + R_par)。閉塞時 P_i = P_s·R_par,i/(R_ref + R_par,i)。R_ref ≈ R_par で感度最大。ΔP は定流量近似より小さい(流量が減るため)。
- 層流抵抗(Hagen–Poiseuille): 円管 R = 128 µ L/(π d⁴)、矩形 R ≈ 12 µ L/(w h³(1 − 0.63 h/w))(h<w)。µ_air = 1.81×10⁻⁵ Pa·s。
- 時定数: τ = R_par·C、C = V/P_atm(等温)。
- 妥当性: Re = ρ v d/µ < 2000、動圧 ½ρv² ≪ ΔP ステップ(そうでないとオリフィス則 ΔP ∝ Q² に移り、設計則が AirTouch 型の √ 則に変わる)。

### 5.3 数値例(100 Pa ステップ)
- 条件: Q = 1 L/min(1.67×10⁻⁵ m³/s)、P_0 = 3 kPa、ΔP_i = 100·i Pa(i=1..7)。
  g_i = 0.032, 0.063, 0.091, 0.118, 0.143, 0.167, 0.189(Σ = 0.80、残り 0.20 をブリード)。G_tot = 5.6×10⁻⁹ m³/(s·Pa)、R_par = 1.8×10⁸ Pa·s/m³。
- ID 用絞り R_i = 5.6×10⁹ … 9.5×10⁸ Pa·s/m³。円管換算の長さ L:

| ゾーン | ΔP [Pa] | R_i [Pa·s/m³] | d=0.3 mm | d=0.4 mm | d=0.5 mm | d=0.7 mm | q_i [L/min] | v@0.4 mm [m/s] | Re | ½ρv² [Pa] |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 100 | 5.6e9 | 61 mm | 194 mm | 473 mm | 1.8 m | 0.032 | 4.3 | 113 | 11 |
| 2 | 200 | 2.9e9 | 32 | 100 | 244 | 0.94 m | 0.062 | 8.3 | 220 | 41 |
| 3 | 300 | 2.0e9 | 22 | 69 | 168 | 0.65 m | 0.091 | 12.1 | 320 | 87 |
| 4 | 400 | 1.5e9 | 17 | 53 | 130 | 0.50 m | 0.118 | 15.6 | 414 | 146 |
| 5 | 500 | 1.3e9 | 14 | 44 | 107 | 0.41 m | 0.143 | 18.9 | 502 | 215 |
| 6 | 600 | 1.1e9 | 12 | 37 | 92 | 0.35 m | 0.167 | 22.1 | 586 | 293 |
| 7 | 700 | 9.5e8 | 10 | 33 | 81 | 0.31 m | 0.189 | 25.1 | 665 | 378 |

- 含意: FDM-TPU で安定して印刷できる φ≥0.7 mm では 0.3–1.8 m の流路が必要で 20 cm 恐竜には収まらない。**φ0.3–0.4 mm の挿入式絞り**(PTFE 毛細管、27G/30G 注射針 ID 0.21/0.16 mm など)を各ゾーン流路に直列に入れるのが現実的。ただし高ゾーンでは動圧が 100 Pa ステップに匹敵し非線形 → **Q と P_0 を半分(0.5 L/min、1.5 kPa、ステップ 50 Pa)にすると同じ R_i・同じ L で速度が半分、動圧が 1/4** になる。その場合センサノイズ ≤5 Pa が必要。
- 時定数: マニホールド+流路容積 V ≈ 20 mL → C = 2.0×10⁻¹⁰ m³/Pa、τ ≈ 36 ms(帯域 ≈4 Hz 以上)。Sariola 2025 の 55 ms と同程度。
- つぶれ部: 皮下 1–2 mm に幅 3 mm × 高さ 0.8 mm × 長さ 20–30 mm の扁平流路。内圧 1.5–3 kPa に対し指の接触圧 10–30 kPa なので確実に閉じる。つぶれ部単独の抵抗は ID 絞りより ≥10 倍小さく設計し、閉塞の有無が 2 値になるようにする(部分圧縮による「ゾーン 7 を軽く押した」と「ゾーン 3 を強く押した」の混同=振幅符号化の本質的限界。AirTouch は剛体出口を完全に塞ぐことで回避)。
- ポンプ: 1 L/min を 3 kPa 背圧で出せる小型ダイヤフラムポンプ(血圧計用 6 V 級、無負荷 1.5–2 L/min、最大 30–40 kPa 級、約 1 W)。定流量化はポンプ定電圧駆動+R_ref ≥ 3·R_par(= 供給 ≥12 kPa)で近似。脈動は τ=36 ms で平滑化されるが、必要なら 50 mL のバッファ容積を追加。
- センサ: ゲージ/差圧 0–5 kPa、ノイズ ≤5 Pa、≥100 Hz。例: Honeywell ABP 0–1 psi(6.9 kPa、14 bit → 0.4 Pa LSB、精度 ±0.25 %FS = 17 Pa、相対変化なら十分)、NXP MPXV5004DP(0–3.92 kPa アナログ)、絶対圧なら BMP390(RMS ノイズ ~0.02–0.1 Pa、気圧ドリフトは AirTouch 同様に直前 1 s の基準差分で除去)。AirTouch の ADP5(ノイズ 10 Pa)でも 100 Pa ステップなら SN 10。
- 2 点同時: AirTouch 同様、ΔP_{i+j} が一意になるよう g_i を選ぶ(等差でなく 1,2,4,8… の重み付けなら 7 ゾーンで 2 点同時まで一意だが Σg が 1 を超えるので P_0 を上げる必要あり)。

### 5.4 代替: 受動(ポンプなし)RC 署名方式(未検証・本調査の提案)
ゾーン i を「室容積 V_i + オリフィス抵抗 R_i」とし、共通マニホールドのセンサ 1 個で押下パルスを読む。パルスの時定数 τ_i = R_i·V_i/P_atm、ピーク振幅 ≈ (押込み体積 ΔV/V_total)·P_atm に室の順応性が乗る。R_i と V_i を 2 軸で変えれば(振幅, 立上り時間)平面で 7 点を分離できる。Alcon 特許の「容積→振幅」の拡張に相当するが、押す速さ依存が強く、査読文献での実証は見つからなかった(gap)。

---

## 6. 未解決の gap

1. **外皮無加工+圧力センサ 1 個+ポンプ**で複数位置を識別した査読論文は存在しない(AirTouch は穴、Sariola は多チャネル)。本プロジェクトの主張点。
2. 受動的に握る体で **オリフィス径/室形状が ID を符号化**し単一圧力センサで読む方式は、特許記述(Alcon、原文未確認)のみで定量評価がない。押す速さ・強さと ID の分離が未解決。
3. Sariola 両論文の **圧力センサ型番・サンプリング・流量・基準抵抗の寸法**は本文 403 のため未確認(2025 の 200 µm 流路/60 kPa/τ 55 ms は二次情報)。
4. SqueezaPulse の分類精度・識別可能な管の本数は未確認。
5. TPU-FDM での φ0.3–0.5 mm 流路の再現性(AirTouch は FDM 漏れで SLA に逃げた)。挿入式絞りの実装と気密が要検証。
6. 振幅符号化の本質的限界(部分押圧の混同)に対し、時間波形特徴や 2 軸(振幅×時定数)符号化で回避できるかの実験データがない。
7. ポンプ騒音・消費電力(≈1 W)が玩具用途で許容されるかの評価がない。

---

### 参照 URL
- AirTouch: https://dl.acm.org/doi/10.1145/3313831.3376136 , https://carlosetejada.com/research/airtouch/
- Sariola 2024: https://doi.org/10.1089/rorep.2024.0034 , https://researchportal.tuni.fi/en/publications/soft-pneumatic-tactile-pressure-sensor-matrix/
- Sariola 2025: https://doi.org/10.1002/aisy.202400381 , https://www.sciencedaily.com/releases/2024/10/241024131917.htm
- Blowhole: https://dl.acm.org/doi/10.20380/GI2018.18 , https://carlosetejada.com/research/blowhole/
- Lamello: https://dl.acm.org/doi/10.1145/2702123.2702207 , https://people.eecs.berkeley.edu/~bjoern/papers/savage-lamello-chi2015.pdf
- Acoustruments: https://dl.acm.org/doi/10.1145/2702123.2702414 (SIGGRAPH ET: 10.1145/2782782.2792490)
- Resonant pneumatic: https://escholarship.org/content/qt8ht881qh/qt8ht881qh.pdf ; AcousTac: https://arxiv.org/abs/2307.09730
- SqueezaPulse: https://makeabilitylab.cs.washington.edu/project/squeezapulse/
- Nat. Commun. 2026: https://pmc.ncbi.nlm.nih.gov/articles/PMC13334043/
- Pneumatic-tomographic: https://arxiv.org/abs/2503.13036
- Tawk 2019: https://advanced.onlinelibrary.wiley.com/doi/abs/10.1002/aisy.201900002 ; Truby 2022: https://www.science.org/doi/10.1126/sciadv.abq4385
- AirLogic: https://valkyriesavage.com/airlogic.html ; Vázquez CHI 2015: https://publications.ri.cmu.edu/storage/publications/pub_files/2015/4/3D-Printing-Pneumatic-Device-Controls-with-Variable-Activation-Force-Capabilities-Paper.pdf
- 特許: https://patents.google.com/patent/US10987183B2/en , https://patents.google.com/patent/US20140256213 , https://patents.justia.com/patent/9491929
