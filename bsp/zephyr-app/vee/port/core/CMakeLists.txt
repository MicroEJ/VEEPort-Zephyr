# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.
# Build: 7E4D1F7C


include_guard()
message("microej/core component is included.")

target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/inc)
target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/LLMJVM_ZephyrOS.c
)

target_sources_ifdef(CONFIG_MICROEJ_CORE_TIME_USE_MICROEJ_TIME ${target} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/microej_time.c)
target_sources_ifdef(CONFIG_MICROEJ_CORE_TIME_USE_NATIVE_ZEPHYR ${target} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/microej_time_zephyr.c)

# Source for core testsuite validation
target_sources(${target} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/core_validation.c)
