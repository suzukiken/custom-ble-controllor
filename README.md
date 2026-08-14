# Xiao ZMK Config

Seeed Studio XIAO nRF52840 向けの ZMK user config です。`pcb/` の基板組み合わせごとに shield を分け、GitHub Actions で UF2 をビルドします。

Board は `xiao_ble//zmk`。ZMK 本体は [`config/west.yml`](config/west.yml) で commit `268b1b1e82150460f00fd701bcd08583d5c75d29` に固定しています。

## Shield 一覧

| Shield | ハードウェア | ピン | キーマップ / センサ |
| --- | --- | --- | --- |
| `onekey_xiao` | `one-key` | D0 ↔ GND | ZMK Studio unlock（Studio で `SPACE` 等に変更） |
| `key_xiao` | `main-board` + `key-board`（PH 2） | D0 ↔ GND | `SPACE` |
| `encoder_xiao` | `main-board` + `encoder-board`（PH 3） | D1=A, D2=B, GND=C | 回転: `C_VOL_UP` / `C_VOL_DN` |
| `push_encoder_xiao` | `main-board` + `push-encoder-board`（PH 4） | D7=A, D6=B, D5=SW, GND=C | Push: `C_MUTE` / 回転: 音量 |
| `fourway_xiao` | `main-board-8` + `4way-re-board`（8ピン, RKJXT1F42001） | 下表 | 十字・Enter・音量 |

BLE 名はそれぞれ `OneKey Xiao` / `Key Xiao` / `Encoder Xiao` / `PushEnc Xiao` / `Fourway Xiao` です（ZMK の上限は15文字）。

共通設定（各 `config/*.conf`）:

- `CONFIG_ZMK_BLE=y`
- `CONFIG_ZMK_SLEEP=y` / `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=60000`（60秒）
- エンコーダ付きは `CONFIG_EC11=y`
- `onekey_xiao` のみ ZMK Studio 用に `CONFIG_ZMK_USB=y`（他は `CONFIG_ZMK_USB=n`）

## ZMK Studio（`onekey_xiao`）

`onekey_xiao` は [ZMK Studio](https://zmk.dev/docs/features/studio) でキー割り当てを変更できます（再フラッシュ不要）。

1. Actions でビルドした `onekey_xiao-...uf2` を書き込む  
   （`build.yaml` で `studio-rpc-usb-uart` + `CONFIG_ZMK_STUDIO=y`）
2. USB で PC に接続し、https://zmk.studio/ を開く
3. キーを押して unlock（初期割り当ては `&studio_unlock`）
4. Studio 上で `SPACE` など好きなキーに変更して Save

1キー構成のため、hold-tap で Space と unlock を同居させるのは ZMK の制約上むずかしく、初期は unlock 専用にしています。Studio で一度 Save すると、以降の変更も Studio 上で行えます（`.keymap` より端末の保存が優先。戻すときは Restore Stock Settings）。

### BLE にペアできないとき

ファーム更新後は古いボンディングで失敗しやすいです。次の順で試してください。

1. PC/スマホ側で `OneKey Xiao` を削除（Forget）
2. **USB ケーブルを抜く**（Studio 用 USB 接続中はペアしにくい）
3. XIAO の `RST` を押すか、キーを一度押して起こす
4. すぐ Bluetooth 設定で `OneKey Xiao` を追加

それでもダメなら:

1. Artifacts の `settings_reset` 用 uf2 を焼く（保存設定を消去）
2. 続けて通常の `onekey_xiao` uf2 を焼く
3. もう一度上記 1–4

いまの `onekey_xiao.conf` には一時的に `CONFIG_ZMK_BLE_CLEAR_BONDS_ON_START=y` を入れています（起動のたびにボンディング消去）。ペアできたらこの行を消して再ビルドしてください。

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

## ビルド

[`build.yaml`](build.yaml) の全 shield が GitHub Actions（`Build ZMK firmware`）でビルドされます。

```yaml
include:
  - board: xiao_ble//zmk
    shield: onekey_xiao
    snippet: studio-rpc-usb-uart
    cmake-args: -DCONFIG_ZMK_STUDIO=y
  - board: xiao_ble//zmk
    shield: key_xiao
  - board: xiao_ble//zmk
    shield: encoder_xiao
  - board: xiao_ble//zmk
    shield: push_encoder_xiao
  - board: xiao_ble//zmk
    shield: fourway_xiao
  - board: xiao_ble//zmk
    shield: settings_reset
```

成果物（Artifacts の `firmware`）:

- `onekey_xiao-xiao_ble__zmk-zmk.uf2`
- `key_xiao-xiao_ble__zmk-zmk.uf2`
- `encoder_xiao-xiao_ble__zmk-zmk.uf2`
- `push_encoder_xiao-xiao_ble__zmk-zmk.uf2`
- `fourway_xiao-xiao_ble__zmk-zmk.uf2`
- `settings_reset-xiao_ble__zmk-zmk.uf2`（ペアリング復旧用）

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
├── .github/workflows/build.yml
├── .gitignore
├── build.yaml
├── README.md
├── config/
│   ├── west.yml
│   ├── onekey_xiao.conf / .keymap
│   ├── key_xiao.conf / .keymap
│   ├── encoder_xiao.conf / .keymap
│   ├── push_encoder_xiao.conf / .keymap
│   └── fourway_xiao.conf / .keymap
├── boards/shields/
│   ├── onekey_xiao/
│   ├── key_xiao/
│   ├── encoder_xiao/
│   ├── push_encoder_xiao/
│   └── fourway_xiao/
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
