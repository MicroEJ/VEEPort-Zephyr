# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.

cmake_minimum_required(VERSION 3.13.1)

# load project specific overrides
# Replace / with _ in the BOARD variable
string(REPLACE "/" "_" BOARD_SAFE "${BOARD}${BOARD_QUALIFIERS}")
set(BOARD_CONFIG ${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD_SAFE}.cmake)
if(EXISTS ${BOARD_CONFIG})
	message(STATUS "Including board-specific config: ${BOARD_CONFIG}")
	include(${BOARD_CONFIG})
else()
	message(STATUS "No board-specific config for ${BOARD}${BOARD_QUALIFIERS}")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

set(target app)

project(${target} LANGUAGES C)

# Load project specific linker script
set(BOARD_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_SAFE}.ld)
if(EXISTS ${BOARD_LINKER_SCRIPT})
	message(STATUS "Using board-specific linker script: ${BOARD_LINKER_SCRIPT}")
	zephyr_linker_sources(SECTIONS ${BOARD_LINKER_SCRIPT})
else()
	message(STATUS "No board-specific linker script for ${BOARD}")
endif()

target_sources(${target} PRIVATE main.c)
