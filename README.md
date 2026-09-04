# Xiao ZMK Config

Seeed Studio XIAO nRF52840 向けの ZMK user config です。`pcb/` の基板組み合わせごとに shield を分け、GitHub Actions で UF2 をビルドします。

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
| `batt_test_xiao` | 電池寿命実験用（D0 キーのみ） | D0 ↔ GND | 30秒おきページめくり想定 |

BLE 名はそれぞれ `OneKey Xiao` / `Key Xiao` / `Encoder Xiao` / `PushEnc Xiao` / `Fourway Xiao` / `KeyEnc Xiao` / `Rkjxt Xiao` / `BattTest Xiao` です（ZMK の上限は15文字）。

共通設定（各 `config/*.conf`）:

- `CONFIG_ZMK_BLE=y` / `CONFIG_ZMK_USB=n`
- `CONFIG_ZMK_SLEEP=y` / `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=60000`（60秒）
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

### batt_test_xiao（電池持ち実験）

目的は「BLE 接続したまま、約30秒に1回ページめくり相当のキーが出る」ときの持ち時間の見積もりです。

```text
XIAO D0 ----[ switch or external timer ]---- XIAO GND
```

- キー: `RIGHT`（Kindle で効かなければ `config/batt_test_xiao.keymap` を `SPACE` に変更）
- 入力後 **5秒** でスリープ（`CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=5000`）
- 30秒周期なら、大半の時間はスリープになる想定

#### 実験のやり方

1. LiPo を XIAO の BAT に接続し、この uf2 を書く
2. iPhone と `BattTest Xiao` をペアリングし、Kindle を開く
3. **30秒に1回** D0 を GND へ落とす（手押しでも可）
4. 電池切れ／電源断まで時間を測る

完全自動にしたい場合は、積み重ねた **XIAO RP2040**（[`tester-rp2040`](tester-rp2040/tester-rp2040.ino)）で30秒ごとに D0 を GND へ落とします。RP2040 は USB 電源、nRF52840 は電池、**GND と GPIO だけ共有**（3V3/5V/BAT は繋がない）。

見積もりの目安: `稼働時間 = 満充電から不能になるまでの時間`。  
手動と自動で周期がずれても、`回数 × 30秒` から換算できます。

## RP2040 キーテスター（Arduino）

電池試験用に、XIAO RP2040 が 30 秒周期で `D0` を **500ms** LOW にするファームです（nRF のスリープ復帰＋デバウンス用。短すぎると手動短絡は成功しても自動は失敗しやすい）。未使用の `D1`–`D10` は Hi-Z のままです。パルス時は赤 LED が点灯します。

GitHub Actions [`Build RP2040 tester`](.github/workflows/build-rp2040.yml) が UF2 を出します。

1. Actions の Artifacts から `rp2040-tester-firmware` をダウンロード
2. XIAO RP2040 で **B を押しながら R**（または B 押しながら挿す）→ `RPI-RP2` ドライブ
3. `xiao-rp2040-key-tester.uf2` をドラッグ&ドロップ

周期や対象ピンは `tester-rp2040/tester-rp2040.ino` の `INTERVAL_MS` / `TARGET_PIN` で変更できます。

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
    shield: batt_test_xiao
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
- `batt_test_xiao-xiao_ble__zmk-zmk.uf2`
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
│   ├── build.yml          # ZMK
│   └── build-rp2040.yml   # XIAO RP2040 key tester
├── .gitignore
├── build.yaml
├── README.md
├── tester-rp2040/
│   └── tester-rp2040.ino
├── config/
│   ├── west.yml
│   ├── onekey_xiao.conf / .keymap
│   ├── key_xiao.conf / .keymap
│   ├── encoder_xiao.conf / .keymap
│   ├── push_encoder_xiao.conf / .keymap
│   ├── key_encoder_xiao.conf / .keymap
│   ├── rkjxt_xiao.conf / .keymap
│   └── batt_test_xiao.conf / .keymap
├── boards/shields/
│   ├── onekey_xiao/
│   ├── key_xiao/
│   ├── encoder_xiao/
│   ├── push_encoder_xiao/
│   ├── fourway_xiao/
│   ├── key_encoder_xiao/
│   ├── rkjxt_xiao/
│   └── batt_test_xiao/
├── pcb/
│   ├── one-key.kicad_pcb
│   ├── main-board.kicad_pcb
│   ├── main-board-8.kicad_pcb
│   ├── key-board.kicad_pcb
│   ├── encoder-board.kicad_pcb
│   ├── push-encoder-board.kicad_pcb
│   ├── 4way-re-board.kicad_pcb
│   └── README.md
└── zephyr/module.yml
```

Gerber（`*.gbr`）・ドリル（`*.drl`）・`*.kicad_prl` / `fp-info-cache` / `.history/` は `.gitignore` 対象です。
