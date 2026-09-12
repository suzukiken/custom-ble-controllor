# SPDX-License-Identifier: MIT

zephyr_library()
# EXTRA_MODULES libraries do not inherit ZMK app includes by default.
zephyr_library_include_directories(${APPLICATION_SOURCE_DIR}/include)
zephyr_library_sources_ifdef(CONFIG_ZMK_SOAK_STATUS src/soak_status.c)
