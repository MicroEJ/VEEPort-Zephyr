# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.
# Build: 7E4D1F7C

# Board-level CMake for this board extension. Zephyr includes it after the upstream
# mimxrt1170_evk board.cmake, so only settings that are still read at that point work
# here -- essentially the default runners (upstream already configures linkserver,
# jlink and pyocd, and defaults to linkserver):
#
#   set(BOARD_FLASH_RUNNER jlink)
#   set(BOARD_DEBUG_RUNNER jlink)
#
# Anything consumed earlier is ignored: board_runner_args(), SHIELD,
# EXTRA_DTC_OVERLAY_FILE, EXTRA_CONF_FILE.
