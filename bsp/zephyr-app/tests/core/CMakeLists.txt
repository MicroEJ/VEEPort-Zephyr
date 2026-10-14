# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.
# Build: 7E4D1F7C

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

set(target app)

project(tests_core)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/board_linker_sections.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/../../vee/thirdparty/embunit/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/../../vee/port/core/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/../common/CMakeLists.txt)

set(COREMARK_DIR ${CMAKE_CURRENT_LIST_DIR}/../../vee/thirdparty/coremark)

target_include_directories(${target} PUBLIC
	${CMAKE_CURRENT_LIST_DIR}/inc
	${CMAKE_CURRENT_LIST_DIR}/coremark/inc
	${CMAKE_CURRENT_LIST_DIR}/../../vee/inc
	${COREMARK_DIR}
)
target_sources(${target}
    PRIVATE
    ${COREMARK_DIR}/core_list_join.c
    ${COREMARK_DIR}/core_main.c
    ${COREMARK_DIR}/core_matrix.c
    ${COREMARK_DIR}/core_state.c
    ${COREMARK_DIR}/core_util.c
	${CMAKE_CURRENT_LIST_DIR}/coremark/src/core_portme.c
    ${CMAKE_CURRENT_LIST_DIR}/src/async_wakeup_vm.c
    ${CMAKE_CURRENT_LIST_DIR}/src/main.c
    ${CMAKE_CURRENT_LIST_DIR}/src/microejruntime_mock.c
    ${CMAKE_CURRENT_LIST_DIR}/src/t_core_core_benchmark.c
    ${CMAKE_CURRENT_LIST_DIR}/src/t_core_main.c
    ${CMAKE_CURRENT_LIST_DIR}/src/t_core_print.c
    ${CMAKE_CURRENT_LIST_DIR}/src/t_core_ram.c
    ${CMAKE_CURRENT_LIST_DIR}/src/t_core_time_base.c
    ${CMAKE_CURRENT_LIST_DIR}/src/x_impl_core_benchmark.c
    ${CMAKE_CURRENT_LIST_DIR}/src/x_impl_ram_checks.c
    ${CMAKE_CURRENT_LIST_DIR}/src/x_impl_ram_checks_zephyr.c
    ${CMAKE_CURRENT_LIST_DIR}/src/x_impl_ram_speed.c
    ${CMAKE_CURRENT_LIST_DIR}/src/x_ram_checks.c
    ${CMAKE_CURRENT_LIST_DIR}/src/x_ram_speed.c
)

target_compile_definitions(${target} PUBLIC MICROEJ_CORE_VEE_MOCK_VALIDATION=1)
target_compile_definitions(${target} PUBLIC UTIL_printf=printf)
