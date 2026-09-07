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
| `powerbtn_xiao` | 電源ボタン（ZMK Soft Off） | D0 ↔ GND | 3秒長押しで System OFF / 押して起動 |
| `batt_1hz_xiao` | 電池 soak（RP2040 積み） | D0–D9=0–9 / D10=Enter | 約1秒に1打・行数で稼働時間 |

BLE 名はそれぞれ `OneKey Xiao` / `Key Xiao` / `Encoder Xiao` / `PushEnc Xiao` / `Fourway Xiao` / `KeyEnc Xiao` / `Rkjxt Xiao` / `BattTest Xiao` / `PowerBtn Xiao` / `Batt1Hz Xiao` です（ZMK の上限は15文字）。

共通設定（各 `config/*.conf`）:

- `CONFIG_ZMK_BLE=y` / `CONFIG_ZMK_USB=n`
- `CONFIG_ZMK_SLEEP=y` / `CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=60000`（60秒）。`powerbtn_xiao` だけ `CONFIG_ZMK_SLEEP=n`（明示オフまで ON のまま）
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

### batt_1hz_xiao（約1Hz 数字行ログ / iPad）

LiPo の nRF が BLE キーボードとして動き続ける時間を、**メモに増える行数**で測ります。接続先は触らない **iPad** を想定（自動ロック「しない」＋充電推奨）。

```text
XIAO nRF D0..D10  ←→  XIAO RP2040 D0..D10
XIAO nRF GND      ←→  XIAO RP2040 GND
（3V3 / 5V / BAT は繋がない）
```

| ピン | キー |
| --- | --- |
| D0–D9 | `0`–`9` |
| D10 | Enter |

- nRF: shield `batt_1hz_xiao`（スリープなし）／ BLE 名 `Batt1Hz Xiao`
- RP2040: [`tester-digits-rp2040`](tester-digits-rp2040/tester-digits-rp2040.ino) が約1秒ごとに D0→…→D10 を GND へパルス
- メモ上の1行 `0123456789` ≒ **11秒**（完成行数 × 11 ≒ 稼働秒）
- iPad が寝ると行が増えないので、試験中はスリープさせない

UF2:

- ZMK: `batt_1hz_xiao-xiao_ble__zmk-zmk.uf2`
- RP2040: Actions [`Build RP2040 digits tester`](.github/workflows/build-rp2040-digits.yml) → `xiao-rp2040-digits-tester.uf2`

## RP2040 キーテスター（Arduino）

電池試験用に、XIAO RP2040 が 30 秒周期で `D0` を **500ms** LOW にするファームです（nRF のスリープ復帰＋デバウンス用。短すぎると手動短絡は成功しても自動は失敗しやすい）。未使用の `D1`–`D10` は Hi-Z のままです。パルス時は赤 LED が点灯します。

GitHub Actions [`Build RP2040 tester`](.github/workflows/build-rp2040.yml) が UF2 を出します。

1. Actions の Artifacts から `rp2040-tester-firmware` をダウンロード
2. XIAO RP2040 で **B を押しながら R**（または B 押しながら挿す）→ `RPI-RP2` ドライブ
3. `xiao-rp2040-key-tester.uf2` をドラッグ&ドロップ

周期や対象ピンは `tester-rp2040/tester-rp2040.ino` の `INTERVAL_MS` / `TARGET_PIN` で変更できます。

## バッテリー監視（スリープなし / Mac ロガー）

ZMK ではなく **Arduino（Seeed nRF52 / Bluefruit）** の専用ファームです。スリープせず、BLE で起動からの経過時間・推定残量・電圧を約 **5分** ごとに送ります。

| 側 | 場所 |
| --- | --- |
| ファーム | [`batt_monitor_xiao/`](batt_monitor_xiao/batt_monitor_xiao.ino) |
| Mac ロガー | [`mac-batt-logger/`](mac-batt-logger/logger.py) |

送信例（1行）:

```text
uptime_s=600 percent=87 voltage_mv=3921
```

GATT（Nordic UART ではなく独自サービス）:

- Service `7f5f0001-7a4b-4c8f-9e2d-1b3c5a7e9f01`
- Characteristic `7f5f0002-…`（Read + Notify）

Mac ロガーは Notify に加え、数秒ごとの Read でも取ります。
### ファーム書き込み

1. Actions [`Build batt monitor`](.github/workflows/build-batt-monitor.yml) の Artifact `batt-monitor-firmware` から `xiao-nrf52840-batt-monitor.uf2` を取得  
   （または Arduino IDE: Board = **Seeed XIAO nRF52840** / Seeed nRF52 Boards）
2. XIAO nRF52840 を `RST` 素早く2回 → ブートローダーへ UF2 をコピー
3. BLE 名は `BattMon Xiao`。送信時のみ赤 LED が短く点灯（接続中の緑点滅はなし）

LiPo は BAT+ / GND に接続。長時間の持ち測定では USB を抜く（挿したままだと充電され電圧が歪む）。

### Mac 側（Python）

Bluetooth 権限がオンの macOS で:

```bash
cd mac-batt-logger
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python logger.py -o ~/Desktop/batt-monitor-log.tsv
```

- 接続後すぐ、その後約5分ごとに追記
- `batt-monitor-log.tsv` … 履歴（TSV）
- `batt-monitor-log-latest.tsv` … 最新1件だけ上書き
- 切断時は自動再スキャン／再接続
- 見つからないときはスキャンで見えたデバイス一覧を出す。`--address` で直接接続も可

OS の「Bluetooth」設定でデバイスをペアリングする必要はありません（ロガーが直接 GATT 接続します）。  
Terminal / iTerm に **Bluetooth 権限**が必要です（システム設定 → プライバシーとセキュリティ → Bluetooth）。

ファーム書き込み後、起動時に赤→緑→青のセルフテストがあり、送信時だけ赤 LED が点灯します。Reflash 後に GATT が古い場合は Bluetooth を一度オフ／オンしてください。

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
    shield: batt_test_xiao
  - board: xiao_ble//zmk
    shield: powerbtn_xiao
  - board: xiao_ble//zmk
    shield: batt_1hz_xiao
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
- `powerbtn_xiao-xiao_ble__zmk-zmk.uf2`
- `batt_1hz_xiao-xiao_ble__zmk-zmk.uf2`
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
│   ├── build.yml                 # ZMK
│   ├── build-rp2040.yml          # XIAO RP2040 key tester
│   ├── build-rp2040-digits.yml   # XIAO RP2040 digit-line tester
│   └── build-batt-monitor.yml    # XIAO nRF52840 battery monitor
├── .gitignore
├── build.yaml
├── README.md
├── batt_monitor_xiao/
│   └── batt_monitor_xiao.ino
├── tester-digits-rp2040/
│   └── tester-digits-rp2040.ino
├── mac-batt-logger/
│   ├── logger.py
│   └── requirements.txt
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
│   ├── batt_test_xiao.conf / .keymap
│   ├── batt_1hz_xiao.conf / .keymap
│   └── powerbtn_xiao.conf / .keymap
├── boards/shields/
│   ├── onekey_xiao/
│   ├── key_xiao/
│   ├── encoder_xiao/
│   ├── push_encoder_xiao/
│   ├── fourway_xiao/
│   ├── key_encoder_xiao/
│   ├── rkjxt_xiao/
│   ├── batt_test_xiao/
│   ├── batt_1hz_xiao/
│   └── powerbtn_xiao/
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
