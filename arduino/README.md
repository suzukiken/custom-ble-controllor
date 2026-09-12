# Arduino firmwares

ZMK 以外の Arduino スケッチです（現状は XIAO RP2040 の仮想指のみ）。

| フォルダ | MCU | 用途 | ペアになる ZMK |
| --- | --- | --- | --- |
| [`xiao-rp2040/virtual-finger-30sec/`](xiao-rp2040/virtual-finger-30sec/) | XIAO RP2040 | 30秒おきに D0 を GND パルス | `sleep_xiao` / `awake_xiao` |

積み重ね利用時は **GND + GPIO だけ**共有（3V3 / 5V / BAT は繋がない）。RP2040=USB、nRF=LiPo。

ZMK 本体はリポジトリ直下の `config/`・`boards/shields/` です。
