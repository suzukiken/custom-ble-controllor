# Xiao BLE Controller

Seeed XIAO 向けのファーム／周辺ツール集です。**スタック（どの MCU・どのランタイムか）で場所が分かれています。**

## ファームの場所（まずここ）

| 種類 | 置き場 | 例 |
| --- | --- | --- |
| **ZMK（XIAO nRF52840）** | リポジトリ直下 `config/` + `boards/shields/` | `sleep_xiao`, `onekey_xiao`, … |
| **Arduino（XIAO RP2040）** | [`arduino/xiao-rp2040/`](arduino/xiao-rp2040/) | `virtual-finger-30sec`（nRF を押す仮想指） |

積み重ね（nRF + RP2040）のペア:

| ZMK shield（nRF・電池） | RP2040 finger（USB） |
| --- | --- |
| `sleep_xiao`（deep sleep **あり**） | [`virtual-finger-30sec`](arduino/xiao-rp2040/virtual-finger-30sec/) |
| `awake_xiao`（deep sleep **なし**） | 同じ [`virtual-finger-30sec`](arduino/xiao-rp2040/virtual-finger-30sec/) |

### スリープあり／なし比較（手元の機材向け）

公平に見るには **打鍵周期を同じにして sleep だけ変える**。

| 役割 | nRF ファーム | RP2040 | ホスト例 | 見るもの |
| --- | --- | --- | --- | --- |
| A | `sleep_xiao`（deep sleep **あり**） | `virtual-finger-30sec` | iPad | 持ち時間 |
| B | `awake_xiao`（deep sleep **なし**） | `virtual-finger-30sec` | iPhone | 持ち時間 |
| C | `sleep_xiao`（deep sleep **あり**） | **なし**（接続だけ） | Mac | ベースライン |

- 電池3本は満充電から開始。nRF=LiPo / RP2040=USB。GND+D0 のみ共有。
- ホストはどれも近く・同じ部屋に固定（距離差で再送が増えると壊れる）。
- 画面は自動ロック「しない」、試験中は寝かさない。
- 切れ時刻か、最後に動いていた時間で比較。A≪B なら sleep が効いている。A≈B なら接続維持が支配的。
- C が A より大幅に長いなら「30秒打鍵＋起床」のコストが見える。

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
| `sleep_xiao` | 仮想指 30秒 + deep sleep あり | D0 ↔ GND | `virtual-finger-30sec` + 10分おき status |
| `sleep_xiao_1min` | `sleep_xiao` の動作確認用 | D0 ↔ GND | status のみ **1分**おき |
| `awake_xiao` | 仮想指 30秒 + deep sleep **なし** | D0 ↔ GND | 同じ + 10分おき status |
| `awake_xiao_1min` | `awake_xiao` の動作確認用 | D0 ↔ GND | status のみ **1分**おき |
| `powerbtn_xiao` | 電源ボタン（ZMK Soft Off） | D0 ↔ GND | 3秒長押しで System OFF / 押して起動 |

BLE 名はそれぞれ `OneKey Xiao` / `Key Xiao` / `Encoder Xiao` / `PushEnc Xiao` / `Fourway Xiao` / `KeyEnc Xiao` / `Rkjxt Xiao` / `Sleep Xiao` / `Sleep 1min` / `Awake Xiao` / `Awake 1min` / `PowerBtn Xiao` です（ZMK の上限は15文字）。

共通設定（各 `config/*.conf`）:

- `CONFIG_ZMK_BLE=y` / `CONFIG_ZMK_USB=n`
- `CONFIG_ZMK_SLEEP=y` / `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=60000`（60秒）。`powerbtn_xiao` と `awake_xiao` は `CONFIG_ZMK_SLEEP=n`
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

### sleep_xiao（deep sleep あり・仮想指 30秒）

目的は「BLE 接続したまま、約30秒に1回ページめくり相当のキーが出る」ときの持ち時間の見積もりです。

```text
XIAO D0 ----[ switch or external timer ]---- XIAO GND
```

- キー: `RIGHT`（Kindle で効かなければ `config/sleep_xiao.keymap` を `SPACE` に変更）
- 入力後 **5秒** でスリープ（`CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=5000`）
- 30秒周期なら、大半の時間はスリープになる想定
- **約10分ごと**に HID で `time: 06915, power=54, mode=sleep`（または `mode=awake` / `mode=awake1min`）+ Enter

#### 実験のやり方

1. LiPo を XIAO の BAT に接続し、この uf2 を書く
2. iPhone と `Sleep Xiao` をペアリングし、Kindle を開く
3. **30秒に1回** D0 を GND へ落とす（手押しでも可）
4. 電池切れ／電源断まで時間を測る

完全自動にしたい場合は、積み重ねた **XIAO RP2040**（[`virtual-finger-30sec`](arduino/xiao-rp2040/virtual-finger-30sec/virtual-finger-30sec.ino)）で30秒ごとに D0 を GND へ落とします。RP2040 は USB 電源、nRF52840 は電池、**GND と GPIO だけ共有**（3V3/5V/BAT は繋がない）。

見積もりの目安: `稼働時間 = 満充電から不能になるまでの時間`。  
手動と自動で周期がずれても、`回数 × 30秒` から換算できます。

## virtual-finger-30sec（Arduino RP2040）

置き場: [`arduino/xiao-rp2040/virtual-finger-30sec/`](arduino/xiao-rp2040/virtual-finger-30sec/)

XIAO RP2040 が 30 秒周期で `D0` を **80ms** LOW にします（ホストのキーリピートを避ける短パルス）。未使用の `D1`–`D10` は Hi-Z。パルス時は赤 LED が点灯します。`sleep_xiao` で起きない場合は `PULSE_MS` を少し延ばしてください。

GitHub Actions [`Build Arduino RP2040 virtual-finger-30sec`](.github/workflows/build-arduino-rp2040-virtual-finger-30sec.yml) が UF2 を出します。

1. Actions の Artifacts から `arduino-rp2040-virtual-finger-30sec` をダウンロード
2. XIAO RP2040 で **B を押しながら R**（または B 押しながら挿す）→ `RPI-RP2` ドライブ
3. `xiao-rp2040-virtual-finger-30sec.uf2` をドラッグ&ドロップ

周期や対象ピンは `virtual-finger-30sec.ino` の `INTERVAL_MS` / `TARGET_PIN` で変更できます。

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
    shield: sleep_xiao_1min
  - board: xiao_ble//zmk
    shield: awake_xiao
  - board: xiao_ble//zmk
    shield: awake_xiao_1min
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
- `sleep_xiao_1min-xiao_ble__zmk-zmk.uf2`
- `awake_xiao-xiao_ble__zmk-zmk.uf2`
- `awake_xiao_1min-xiao_ble__zmk-zmk.uf2`
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
│   └── build-arduino-rp2040-virtual-finger-30sec.yml  # Arduino RP2040
├── arduino/
│   ├── README.md
│   └── xiao-rp2040/
│       └── virtual-finger-30sec/         # ↔ ZMK sleep_xiao / awake_xiao
├── config/                               # ZMK conf / keymap / west.yml
├── boards/shields/                       # ZMK shields
├── src/soak_status.c                     # 10分おき time/power HID
├── build.yaml
├── CMakeLists.txt / Kconfig / zephyr/    # ZMK extra module
├── pcb/
└── 3d/
```

Gerber（`*.gbr`）・ドリル（`*.drl`）・`*.kicad_prl` / `fp-info-cache` / `.history/` は `.gitignore` 対象です。
