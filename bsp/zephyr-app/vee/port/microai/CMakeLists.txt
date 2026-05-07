# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.

# CMake
#

include_guard()
message("microej/microai component is included.")

target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/inc)
target_sources(${target}
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/LLML_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLML_microai_heap.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLML_tensorflow_lite.cpp
)
