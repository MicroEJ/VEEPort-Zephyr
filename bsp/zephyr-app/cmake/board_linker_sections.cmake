# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.
# Build: 7E4D1F7C

# Appends the including application's board-specific linker sections, if the board provides any,
# from <application>/boards/<board_target>.ld.
#
# Include this from an application CMakeLists.txt after find_package(Zephyr). It reads
# NORMALIZED_BOARD_TARGET, set by Zephyr's `boards` CMake module (the board target with slashes
# replaced by underscores), so the file name matches the boards/<board_target>.conf and .overlay
# naming. include() does not open a new scope, so CMAKE_CURRENT_SOURCE_DIR below is the including
# application's directory.

set(board_linker_script ${CMAKE_CURRENT_SOURCE_DIR}/boards/${NORMALIZED_BOARD_TARGET}.ld)
if(EXISTS ${board_linker_script})
	zephyr_linker_sources(SECTIONS ${board_linker_script})
endif()
unset(board_linker_script)
