# OneKey Xiao ZMK Config

Seeed Studio XIAO nRF52840 を使った、1キーだけのBLE HIDキーボード用ZMK user config repositoryです。

現在のZMK公式ドキュメントに合わせ、MCU boardは `xiao_ble//zmk`、自作ハードウェアは shield `onekey_xiao` として定義しています。XIAOのD0はshield overlay内で `&xiao_d 0` として参照します。

## 配線

キースイッチを XIAO nRF52840 の `D0` と `GND` の間に接続します。

```text
XIAO D0 ----[ key switch ]---- XIAO GND
```

GPIOは内部プルアップを使います。キーを押すとD0がGNDへ落ちるため、ZMK側では `GPIO_ACTIVE_LOW | GPIO_PULL_UP` として定義しています。

## 動作

キーを押すとBLE HIDキーボードとして `SPACE` を送信します。

通常の入力はBLEのみを使うため、`config/onekey_xiao.conf` で `CONFIG_ZMK_USB=n` にしています。UF2書き込み用のブートローダーは別なので、USB-CでのUF2書き込みは可能です。

## ビルド方法

GitHubにこのリポジトリをpushすると、GitHub Actionsの `Build ZMK firmware` が実行されます。

```sh
git add .
git commit -m "Add one-key XIAO ZMK config"
git push
```

ビルド対象は [build.yaml](build.yaml) で指定しています。

```yaml
include:
  - board: xiao_ble//zmk
    shield: onekey_xiao
```

Actionsが成功したら、対象runのArtifactsから `firmware` をダウンロードして展開します。生成されるUF2は `onekey_xiao-xiao_ble__zmk-zmk.uf2` のような名前になります。

## XIAOへのUF2書き込み方法

1. XIAO nRF52840をUSB-CでPCに接続します。
2. XIAOの `RST` を素早く2回押してブートローダーモードに入れます。
3. `XIAO-SENSE` または同等のUSBマスストレージがマウントされます。
4. GitHub Actionsで取得した `.uf2` ファイルを、そのドライブへドラッグ&ドロップします。
5. 書き込み後、自動で再起動します。

## BLEペアリング方法

初回書き込み後、XIAOはBLEキーボードとして広告します。PC、iPad、AndroidなどのBluetooth設定を開き、`OneKey Xiao` を選択してペアリングしてください。

この最小構成ではキーが1つだけなので、`BT_CLR` や `BT_SEL` はまだ割り当てていません。別の機器とペアリングし直す場合は、今後キーを増やしてBluetooth制御キーを追加するか、ZMKの `settings_reset` firmwareを一時的にビルドしてフラッシュし、保存済みペアリング情報を消去してください。

## 今後キーを増やす方法

キーを増やす場合は、主に次の2ファイルを変更します。

1. [boards/shields/onekey_xiao/onekey_xiao.overlay](boards/shields/onekey_xiao/onekey_xiao.overlay)
   - `input-gpios` に使うXIAOピンを追加します。
   - `columns` をキー数に合わせて増やします。
   - `map` に `RC(0,0) RC(0,1) ...` のように位置を追加します。

2. [config/onekey_xiao.keymap](config/onekey_xiao.keymap)
   - `bindings` にキー数と同じ数の動作を追加します。
   - 例: `&kp SPACE &kp ENTER &kp C_PLAY_PAUSE`

メディアキーを追加する場合もkeymap側でZMKのキーコードやbehaviorを追加します。バッテリー動作や省電力機能は、まず [config/onekey_xiao.conf](config/onekey_xiao.conf) に設定を追加し、必要ならshield overlayにバッテリー測定用のdevicetree設定を追加する方針にすると拡張しやすいです。

## ファイル構成

```text
.
├── .github/workflows/build.yml
├── build.yaml
├── config/
│   ├── onekey_xiao.conf
│   ├── onekey_xiao.keymap
│   └── west.yml
├── boards/shields/onekey_xiao/
│   ├── Kconfig.defconfig
│   ├── Kconfig.shield
│   ├── onekey_xiao.overlay
│   └── onekey_xiao.zmk.yml
└── zephyr/module.yml
```

