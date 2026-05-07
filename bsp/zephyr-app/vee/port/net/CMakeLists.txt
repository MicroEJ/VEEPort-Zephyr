# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.


include_guard()
message("microej/net component is included.")

target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/inc)

target_sources(${target} PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/src/LLNET_CHANNEL_bsd.c
	${CMAKE_CURRENT_LIST_DIR}/src/LLNET_Common.c
	${CMAKE_CURRENT_LIST_DIR}/src/LLNET_DATAGRAMSOCKETCHANNEL_bsd.c
	${CMAKE_CURRENT_LIST_DIR}/src/LLNET_MULTICASTSOCKETCHANNEL_bsd.c
	${CMAKE_CURRENT_LIST_DIR}/src/LLNET_NETWORKADDRESS_bsd.c
	${CMAKE_CURRENT_LIST_DIR}/src/LLNET_SOCKETCHANNEL_bsd.c
	${CMAKE_CURRENT_LIST_DIR}/src/LLNET_STREAMSOCKETCHANNEL_bsd.c
	${CMAKE_CURRENT_LIST_DIR}/src/async_select.c
	${CMAKE_CURRENT_LIST_DIR}/src/async_select_osal.c
	${CMAKE_CURRENT_LIST_DIR}/src/LLNET_DNS_zephyr.c
	${CMAKE_CURRENT_LIST_DIR}/src/LLNET_NETWORKINTERFACE_zephyr.c
	${CMAKE_CURRENT_LIST_DIR}/src/LLNET_init_zephyr.c
)

