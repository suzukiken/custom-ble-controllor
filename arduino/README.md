# Arduino firmwares

ZMK 以外の Arduino スケッチです（現状は XIAO RP2040 の仮想指のみ）。

| フォルダ | MCU | 用途 | ペアになる ZMK |
| --- | --- | --- | --- |
| [`xiao-rp2040/virtual-finger/`](xiao-rp2040/virtual-finger/) | XIAO RP2040 | D0: 5分おき status トリガ | `sleep_xiao` / `awake_xiao` |

配線: **D0・GND** を共有（3V3 / 5V / BAT は繋がない）。RP2040=USB、nRF=LiPo。

ZMK 本体はリポジトリ直下の `config/`・`boards/shields/` です。
