# Xiao BLE Controller

Seeed XIAO 向けのファーム／周辺ツール集です。**スタック（どの MCU・どのランタイムか）で場所が分かれています。**

## ファームの場所（まずここ）

| 種類 | 置き場 | 例 |
| --- | --- | --- |
| **ZMK（XIAO nRF52840）** | リポジトリ直下 `config/` + `boards/shields/` | `sleep_xiao`, `onekey_xiao`, … |
| **Arduino（XIAO RP2040）** | [`arduino/xiao-rp2040/`](arduino/xiao-rp2040/) | `virtual-finger`（nRF を押す仮想指） |

積み重ね（nRF + RP2040）のペア:

| ZMK shield（nRF・電池） | RP2040 finger（USB） |
| --- | --- |
| `sleep_xiao`（deep sleep **あり**） | [`virtual-finger`](arduino/xiao-rp2040/virtual-finger/)（D0） |
| `sleep_xiao_pi`（同上 + Pi/BlueZ 向け BLE） | 同じ |
| `awake_xiao`（deep sleep **なし**） | 同じ |
| `awake_xiao_pi`（awake + Pi/BlueZ 向け BLE） | 同じ |

RP2040: **D0** を約5分おきにパルス。nRF は `time: …, power=…, mode=…` を打つ。

### スリープあり／なし比較（手元の機材向け）

公平に見るには **打鍵周期を同じにして sleep だけ変える**。

| 役割 | nRF ファーム | RP2040 | ホスト例 | 見るもの |
| --- | --- | --- | --- | --- |
| A | `sleep_xiao`（deep sleep **あり**） | `virtual-finger` | iPad | 持ち時間 |
| B | `awake_xiao`（deep sleep **なし**） | `virtual-finger` | iPhone | 持ち時間 |
| C | `sleep_xiao`（deep sleep あり） | **なし**（接続だけ） | Mac | ベースライン |

- 電池3本は満充電から開始。nRF=LiPo / RP2040=USB。**GND + D0** を共有。
- ホストはどれも近く・同じ部屋に固定（距離差で再送が増えると壊れる）。
- 画面は自動ロック「しない」、試験中は寝かさない。
- Notes 等に約5分ごと `time: …, power=…, mode=sleep|awake` が出る。
- 切れ時刻か、最後の `time:` で比較。A≪B なら sleep が効いている。A≈B なら接続維持が支配的。
- C が A より大幅に長いなら「5分打鍵＋起床」のコストが見える。

詳細は [`arduino/README.md`](arduino/README.md)。ZMK は user-config の都合でルートに残しています。

---

## ZMK user config（nRF52840）

Board は `xiao_ble//zmk`。ZMK 本体は [`config/west.yml`](config/west.yml) で commit `268b1b1e82150460f00fd701bcd08583d5c75d29` に固定しています。

## Shield 一覧

| Shield | ハードウェア | ピン | キーマップ / センサ |
| --- | --- | --- | --- |
| `onekey_xiao` | `one-key` | D0 ↔ GND | `SPACE` |
| `key_xiao` | `main-board` + `key-board`（PH 2） | D0 ↔ GND | `SPACE` |
| `encoder_xiao` | `main-board` + `encoder-board`（PH 3） | D1=A, D2=B, GND=C | 回転: `C_VOL_UP` / `C_VOL_DN` |
| `push_encoder_xiao` | `main-board` + `push-encoder-board`（PH 4） | D7=A, D6=B, D5=SW, GND=C | Push: `C_MUTE` / 回転: 音量 |
| `fourway_xiao` | `main-board-8` + `4way-re-board`（8ピン, RKJXT1F42001） | 下表 | 十字・Enter・音量 |
| `key_encoder_xiao` | Xiao + keyswitch + encoder 一体 | D0=SW / D1=A, D2=B, GND | `SPACE` + 方向キー上下 |
| `rkjxt_xiao` | Xiao + RKJXT1F42001 一体 | 下表 | 十字・Enter・音量 |
| `sleep_xiao` | 仮想指 + deep sleep あり | D0 ↔ RP2040 | D0=status |
| `sleep_xiao_pi` | `sleep_xiao` + Raspberry Pi/BlueZ 向け BLE | 同上 | 2M PHY 無効など |
| `awake_xiao` | 仮想指 + deep sleep **なし** | D0 ↔ RP2040 | D0=status |
| `awake_xiao_pi` | `awake_xiao` + Pi/BlueZ 向け BLE | 同上 | 接続切り分け用 |
| `drain_xiao` | **電池消費最大化**（充電電流計測の前処理） | （キー不要） | RGB+CPU+HID spam |
| `powerbtn_xiao` | 電源ボタン（ZMK Soft Off） | D0 ↔ GND | 3秒長押しで System OFF / 押して起動 |

BLE 名はそれぞれ `OneKey Xiao` / `Key Xiao` / `Encoder Xiao` / `PushEnc Xiao` / `Fourway Xiao` / `KeyEnc Xiao` / `Rkjxt Xiao` / `Sleep Xiao` / `Sleep Pi Xiao` / `Awake Xiao` / `Awake Pi Xiao` / `Drain Xiao` / `PowerBtn Xiao` です（ZMK の上限は15文字）。

status の周期は nRF 側ではなく **RP2040**（既定 5 分）が決めます。

共通設定（各 `config/*.conf`）:

- `CONFIG_ZMK_BLE=y` / `CONFIG_ZMK_USB=n`
- `sleep_xiao`: `CONFIG_ZMK_SLEEP=y` / `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=20000`（status 打鍵後に寝る余裕）。`powerbtn_xiao` と `awake_xiao` は `CONFIG_ZMK_SLEEP=n`
- エンコーダ付きは `CONFIG_EC11=y`

## 配線

### onekey_xiao / key_xiao

```text
XIAO D0 ----[ switch ]---- XIAO GND
```

`zmk,kscan-gpio-direct`、`GPIO_ACTIVE_LOW | GPIO_PULL_UP`。

### encoder_xiao

```text
XIAO D1 ---- Encoder A
XIAO D2 ---- Encoder B
XIAO GND --- Encoder C
```

キー入力は使わず（overlay 上の D10 は未使用スタブ）、回転のみ。

### key_encoder_xiao

Xiao・キースイッチ・ロータリーエンコーダ（プッシュなし）を1枚に載せた構成です。

```text
XIAO D0  ---- keyswitch pin1
XIAO GND ---- keyswitch pin2

XIAO D1  ---- encoder pin1 (A)
XIAO D2  ---- encoder pin3 (B)
XIAO GND ---- encoder pin2 (C)
```

キーは `SPACE`、回転は方向キー Up/Down です。

### push_encoder_xiao

```text
XIAO D7 ---- Encoder A
XIAO D6 ---- Encoder B
XIAO D5 ---- SW (Push)
XIAO GND --- Encoder C
```

### fourway_xiao（RKJXT1F42001）

```text
XIAO D4 ---- A      → UP
XIAO D0 ---- B      → RIGHT
XIAO D2 ---- C      → DOWN
XIAO D3 ---- D      → LEFT
XIAO D5 ---- Push   → ENTER
XIAO D6 ---- Encoder A
XIAO D1 ---- Encoder B
XIAO GND --- GND (Com / ECom)
```

keymap の並びは A, B, C, D, Push。回転は `C_VOL_UP` / `C_VOL_DN`（`steps = 20`）。

### rkjxt_xiao

Xiao と RKJXT1F42001 を1枚に載せた構成です。

```text
XIAO D4 ---- A      → UP
XIAO D0 ---- B      → RIGHT
XIAO D2 ---- C      → DOWN
XIAO D3 ---- D      → LEFT
XIAO D5 ---- Push   → (無効 / 方向と同時導通のため)
XIAO D6 ---- EA (Encoder A)
XIAO D1 ---- EB (Encoder B)
XIAO GND --- GND
```

keymap の並びは A, B, C, D, Push。回転は音量 Up/Down（`steps = 20`）。

RKJXT1F42001 は方向入力時に Push も同時に落ちるため、`rkjxt_xiao` では Push を `&none` にしています（Enter が乗らないようにするため）。中央プッシュが必要なら別途相談してください。

### powerbtn_xiao

```text
XIAO D0 ----[ switch ]---- XIAO GND
```

`zmk,kscan-gpio-direct`。短押しではキーは出ず、3秒長押しで Soft Off（後述）。

### sleep_xiao / awake_xiao（仮想指 + status）

目的は「BLE 接続したまま、約5分に1回 status 行を打つ」ときの持ち時間の見積もりです。`sleep_xiao` は deep sleep あり、`awake_xiao` はなし。

```text
nRF D0 ---- RP2040 D0   (status trigger, ~5min)
nRF GND --- RP2040 GND
```

- D0: keymap は `&none`。`soak_status` が `time: …, power=…, mode=…` + Enter を打つ
- `sleep_xiao`: 入力後 **20秒** でスリープ（status 打鍵が終わる余裕）
- status 周期は **RP2040** 側（既定 5 分）。deep sleep 中でも D0 で起きて送信できる

#### 実験のやり方

1. LiPo を nRF の BAT に接続し、この uf2 を書く
2. ホストと `Sleep Xiao` / `Awake Xiao` をペアリングし、Notes 等を開く
3. RP2040 を積み、**GND + D0** を共有（3V3/5V/BAT は繋がない）。RP2040 は USB 電源
4. 約5分ごとに Notes に status 行が出ることを確認し、電池切れまで測る

見積もりの目安: `稼働時間 = 満充電から不能になるまでの時間`。最後の `time:` でも比較できます。

### sleep_xiao_pi（Raspberry Pi / BlueZ 向け）

`sleep_xiao` と同じ打鍵・スリープ構成で、**BLE だけ** Linux（特に Raspberry Pi の BlueZ）向けに変えています。

ログに `le-connection-abort-by-local` と `Connected: yes`/`no` の高速点滅が出る場合、多くは次のどちらかです。

1. **Pi 側がリンクを切っている**（2M PHY 交渉や SMP/ボンディング不整合で BlueZ が abort）
2. **片方だけの古いボンド**（iPhone 等とペアした鍵が残り、Pi が同じアドレスに古い鍵で接続しようとして失敗→再試行ループ）

`sleep_xiao_pi` では主に次を入れています。

- `CONFIG_ZMK_BLE_EXPERIMENTAL_CONN=y` / `CONFIG_BT_CTLR_PHY_2M=n`（2M PHY 無効）
- ボンド上書き許可（再ペアしやすくする）
- `CONFIG_BT_GATT_ENFORCE_SUBSCRIPTION=n`
- idle sleep を **10分**（virtual-finger の5分より長くする）。あわせて idle も5分に延長
- 送信出力 `CONFIG_BT_CTLR_TX_PWR_PLUS_8=y`

初回は両方きれいにしてからペアしてください。

1. nRF にいったん `settings_reset` を書き、すぐ `sleep_xiao_pi` を書き直す
2. Pi: `bluetoothctl remove <addr>`（該当デバイス）、必要なら `sudo systemctl restart bluetooth`
3. デバイスを起こした状態（D0 パルス後〜60秒以内）で次を実行

```text
agent off
agent NoInputNoOutput
default-agent
scan on
pair <addr>
trust <addr>
connect <addr>
```

**重要:** デスクトップ GUI に「Enter Code 2314134」のような表示が出ることがあります。これは「キーボードでパスコードを打て」という意味で、数字キーの無い本機では入力できません。**GUI のダイアログはキャンセル**し、上記のとおり `agent NoInputNoOutput` の bluetoothctl だけでペアしてください（Just Works）。GUI エージェントが割り込むと失敗しやすいです。

#### つながらないときの切り分け

ログの `Connected: yes`/`no` 連打 + `le-connection-abort-by-local` は、だいたい次の合成です。

1. **ペア未完了／壊れたボンド**（GUI パスコードをキャンセルしたあとに特に多い。`trust` だけでは足りない）
2. **BlueZ × ZMK HID** の相性（2M PHY・認証。ファーム側は 2M 無効などを入れ済み）
3. **deep sleep**（広告が止まる／再接続のたびに同じ失敗を繰り返す）

先に **`awake_xiao_pi`（BLE名 `Awake Pi Xiao`）** でペアを試してください。deep sleep なし・同じ BLE 対策です。

- `awake_xiao_pi` で安定する → 問題は主に sleep／再接続。そのボンドのまま `sleep_xiao_pi` へ載せ替えを検討
- `awake_xiao_pi` でも同じ → deep sleep ではなく **Pi/BlueZ 側**（ボンド掃除・agent・場合によって BT ドングル／OS）が本丸

そのとき `info <addr>` で `Paired: yes` か、できれば `sudo btmon` の切断理由（Authentication / Timeout 等）を見ると次が決まります。

## virtual-finger（Arduino RP2040）

置き場: [`arduino/xiao-rp2040/virtual-finger/`](arduino/xiao-rp2040/virtual-finger/)

| ピン | 周期（既定） | nRF 側の意味 |
| --- | --- | --- |
| `D0` | 5分 | status トリガ（`soak_status`） |

**1.5秒** LOW パルス（deep sleep 起床＋スキャン検出用）。パルス時は赤 LED。起動後約15秒で最初の status が出ます。

GitHub Actions [`Build Arduino RP2040 virtual-finger`](.github/workflows/build-arduino-rp2040-virtual-finger.yml) が UF2 を出します。

1. Actions の Artifacts から `arduino-rp2040-virtual-finger` をダウンロード
2. XIAO RP2040 で **B を押しながら R**（または B 押しながら挿す）→ `RPI-RP2` ドライブ
3. `xiao-rp2040-virtual-finger.uf2` をドラッグ&ドロップ

周期は `INTERVAL_MS` で変更できます。

## drain_xiao（電池を早く減らす・充電計測の前処理）

40mAh などで **半分付近まで落としてから充電電流を測る**ための、消費最大化ファームです。

| 項目 | 内容 |
| --- | --- |
| BLE 名 | `Drain Xiao` |
| sleep | **なし** |
| 動作 | RGB 全点灯 + CPU busy + 約40msおき HID（SPACE）+ 約30秒ごと `time/power` 行 |
| TX | `CONFIG_BT_CTLR_TX_PWR_PLUS_8` |

### 使い方

1. LiPo のみ（USB 給電だと電池が減らない／計測が崩れる）
2. `Drain Xiao` をホストにペアし、Notes 等を開く（HID spam と status 用）
3. 約30秒ごと `time: …, power=XX, mode=drain` を見る
4. **`power` がだいたい 50 前後**になったら止める（抜く／別ファームへ）
5. その状態で充電開始し、電流を測る

注意: 放置すると空近くまで減る可能性があります。50%狙いなら `power=` を見て止めてください。

## 電源ボタン（Soft Off）

D0–GND のスイッチで nRF52840 の **System OFF**（ZMK Soft Off）を入切します。短押しではキーは出ません。

| 操作 | 動作 |
| --- | --- |
| Soft Off 中に押す | ON（即起動） |
| ON 中に **3秒長押し** → 離す | Soft Off |

```text
XIAO D0 ----[ switch ]---- XIAO GND
```

- Shield: `powerbtn_xiao`（他 shield と同じ ZMK ビルド）
- アイドルスリープなし（OFF するまで ON のまま）
- LED 演出や「1秒長押しで ON 確定」はなし

## ビルド

[`build.yaml`](build.yaml) の全 shield が GitHub Actions（`Build ZMK firmware`）でビルドされます。

```yaml
include:
  - board: xiao_ble//zmk
    shield: onekey_xiao
  - board: xiao_ble//zmk
    shield: key_xiao
  - board: xiao_ble//zmk
    shield: encoder_xiao
  - board: xiao_ble//zmk
    shield: push_encoder_xiao
  - board: xiao_ble//zmk
    shield: fourway_xiao
  - board: xiao_ble//zmk
    shield: key_encoder_xiao
  - board: xiao_ble//zmk
    shield: rkjxt_xiao
  - board: xiao_ble//zmk
    shield: sleep_xiao
  - board: xiao_ble//zmk
    shield: sleep_xiao_pi
  - board: xiao_ble//zmk
    shield: awake_xiao
  - board: xiao_ble//zmk
    shield: awake_xiao_pi
  - board: xiao_ble//zmk
    shield: drain_xiao
  - board: xiao_ble//zmk
    shield: powerbtn_xiao
  - board: xiao_ble//zmk
    shield: settings_reset
```

成果物（Artifacts の `firmware`）:

- `onekey_xiao-xiao_ble__zmk-zmk.uf2`
- `key_xiao-xiao_ble__zmk-zmk.uf2`
- `encoder_xiao-xiao_ble__zmk-zmk.uf2`
- `push_encoder_xiao-xiao_ble__zmk-zmk.uf2`
- `fourway_xiao-xiao_ble__zmk-zmk.uf2`
- `key_encoder_xiao-xiao_ble__zmk-zmk.uf2`
- `rkjxt_xiao-xiao_ble__zmk-zmk.uf2`
- `sleep_xiao-xiao_ble__zmk-zmk.uf2`
- `sleep_xiao_pi-xiao_ble__zmk-zmk.uf2`
- `awake_xiao-xiao_ble__zmk-zmk.uf2`
- `awake_xiao_pi-xiao_ble__zmk-zmk.uf2`
- `drain_xiao-xiao_ble__zmk-zmk.uf2`
- `powerbtn_xiao-xiao_ble__zmk-zmk.uf2`
- `settings_reset-xiao_ble__zmk-zmk.uf2`（BLE ペアリング復旧用）

### UF2 書き込み

1. XIAO を USB-C 接続
2. `RST` を素早く2回 → ブートローダー
3. マウントされたドライブへ `.uf2` をコピー

### ペアリング

OS の Bluetooth 設定で上記 BLE 名を選択。`BT_CLR` 等は未割り当てなので、付け直すときは `settings_reset` ファームを使うか、後から制御キーを追加してください。

## 変更箇所

| 目的 | ファイル |
| --- | --- |
| キーコード・音量など | `config/<shield>.keymap` |
| ピン割り当て | `boards/shields/<shield>/<shield>.overlay` |
| BLE / sleep / EC11 | `config/<shield>.conf` |
| ビルド対象 | `build.yaml` |

回転方向が逆のときは、該当 overlay の `a-gpios` と `b-gpios` を入れ替えます。

## ファイル構成

```text
.
├── .github/workflows/
│   ├── build.yml                                      # ZMK (nRF)
│   └── build-arduino-rp2040-virtual-finger.yml        # Arduino RP2040
├── arduino/
│   ├── README.md
│   └── xiao-rp2040/
│       └── virtual-finger/               # ↔ ZMK sleep_xiao / awake_xiao
├── config/                               # ZMK conf / keymap / west.yml
├── boards/shields/                       # ZMK shields
├── src/soak_status.c                     # D0 トリガで time/power HID
├── src/power_drain.c                     # 消費最大化（drain_xiao）
├── build.yaml
├── CMakeLists.txt / Kconfig / zephyr/    # ZMK extra module
├── pcb/
└── 3d/
```

Gerber（`*.gbr`）・ドリル（`*.drl`）・`*.kicad_prl` / `fp-info-cache` / `.history/` は `.gitignore` 対象です。
