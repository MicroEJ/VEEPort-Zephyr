# Copyright 2025 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.

# Define flash runner
# set(BOARD_FLASH_RUNNER linkserver)

# Define debug runner
# set(BOARD_DEBUG_RUNNER linkserver)

# Define your devicetree overlays
# see https://docs.zephyrproject.org/latest/build/dts/howtos.html#set-devicetree-overlays
# DTC_OVERLAY_FILE will override any other overlays in the boards/ folder.
# set(DTC_OVERLAY_FILE "")
# EXTRA_DTC_OVERLAY_FILE will append to the overlays detected by zephyr in the boards/ folder (see zephyr doc for the overlay priority).
set(EXTRA_DTC_OVERLAY_FILE "")

# Define your configuration overlay
#set(OVERLAY_CONFIG pjr_xxxx.conf)