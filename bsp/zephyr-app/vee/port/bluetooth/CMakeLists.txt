# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.


include_guard()
message("microej/bluetooth component is included.")

target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/inc)
target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/bt_event.c
    ${CMAKE_CURRENT_LIST_DIR}/src/bt_helper.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLBLUETOOTH_impl.c
)

