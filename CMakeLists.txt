# SPDX-License-Identifier: MIT

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_ZMK_SOAK_STATUS modules/zmk-soak-status/src/soak_status.c)
