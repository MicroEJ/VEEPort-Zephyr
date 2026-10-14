# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.
# Build: 7E4D1F7C

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

set(target app)

project(tests_llkernel)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/board_linker_sections.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/../../vee/thirdparty/embunit/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/../common/CMakeLists.txt)

target_include_directories(${target} PRIVATE inc)

target_sources(${target} PRIVATE
	src/t_llkernel_main.c
	src/t_llkernel.c
)

target_compile_definitions(${target} PUBLIC KERNEL_RAM_IMPL_BESTFIT)
