# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.

include_guard()
message("microej/config component is included.")

zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR}/inc)
target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/veeport_critical_section.c
)
