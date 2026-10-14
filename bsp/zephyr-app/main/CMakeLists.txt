# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.
# Build: 7E4D1F7C

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

set(target app)

project(${target} LANGUAGES C)

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/board_linker_sections.cmake)

target_sources(${target} PRIVATE main.c)
