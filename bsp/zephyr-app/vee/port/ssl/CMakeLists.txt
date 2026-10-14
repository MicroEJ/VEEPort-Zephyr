# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.
#
# Build: 7E4D1F7C

include_guard()
message("microej/ssl component is included.")

target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/inc)
target_sources(${target} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/LLNET_SSL_CONTEXT_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLNET_SSL_ERRORS.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLNET_SSL_SOCKET_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLNET_SSL_utils_mbedtls.c
    ${CMAKE_CURRENT_LIST_DIR}/src/LLNET_SSL_verifyCallback.c
)
