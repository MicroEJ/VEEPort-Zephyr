#!/bin/bash
#
# BASH
#
# Copyright 2025-2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.
#
# Build: 7E4D1F7C

# 'set_project_env.sh' implementation for MicroEJ Linux builds.
# 'set_project_env' is responsible for
# - setting project variables for 'build.sh' and 'run.sh'.

# BOARD is provided by the Gradle build (see gradle/boards.gradle.kts), which writes the resolved
# Zephyr board name to the generated '.board.env' beside this script (a single bare value, so both this
# script and set_project_env.bat read it natively). Reading it here lets the west build use the board
# selected via -Pveeport.board. The 'export BOARD="${BOARD:-...}"' below is only the standalone fallback
# (manual ./build.sh with no Gradle run); keep its default consistent with the Gradle default.
_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "${_SCRIPT_DIR}/.board.env" ]; then
	IFS= read -r BOARD < "${_SCRIPT_DIR}/.board.env" || true
fi

export BOARD="${BOARD:-mimxrt1170_evk/mimxrt1176/cm7}"

# Set additional arguments to the west build command
# export WEST_BUILD_ARGS=""

# Set additional arguments to the west flash command
# export WEST_FLASH_ARGS="--runner jlink --reset"
