# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Generates a compile_commands.json file containing the exact compiler calls for all translation units of the project in machine-readable form.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(target app)

# add sources
target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/inc)
target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/port/config)

if(CONFIG_MICROEJ_BT_SSL_SECURITY_ERROR)
	message(FATAL_ERROR "Bluetooth cannot be enabled together with MICROEJ_SSL or MICROEJ_SECURITY due to Mbed TLS dependency conflicts.")
endif()

if (CONFIG_MICROEJ_CORE)
	add_subdirectory(port/core)
endif()
if (CONFIG_MICROEJ_BLUETOOTH)
	add_subdirectory(port/bluetooth)
endif()
if (CONFIG_MICROEJ_DEVICE)
	add_subdirectory(port/device)
endif()
if (CONFIG_MICROEJ_ECOM_NETWORK)
	add_subdirectory(port/ecom-network)
endif()
if (CONFIG_MICROEJ_EVENT_QUEUE)
	add_subdirectory(port/event-queue)
endif()
if (CONFIG_MICROEJ_FS)
	add_subdirectory(port/fs)
endif()
if (CONFIG_MICROEJ_KF)
	add_subdirectory(port/kf)
endif()
if (CONFIG_MICROEJ_MICROAI)
	add_subdirectory(port/microai)
endif()
if (CONFIG_MICROEJ_NET)
	add_subdirectory(port/net)
endif()
if (CONFIG_MICROEJ_SECURITY)
	add_subdirectory(port/security)
endif()
if (CONFIG_MICROEJ_SERIAL)
	add_subdirectory(port/serial)
endif()
if (CONFIG_MICROEJ_SSL)
	add_subdirectory(port/ssl)
endif()
if (CONFIG_MICROEJ_UTIL)
	add_subdirectory(port/util)
endif()

if (CONFIG_MICROEJ_CORE)
	# Compile Options
	target_compile_options(${target}
		PUBLIC
		-fno-stack-protector
		-fdollars-in-identifiers
		-fno-strict-aliasing
		-fwrapv
		$<$<COMPILE_LANGUAGE:C>:-Werror-implicit-function-declaration>
		-D_XOPEN_SOURCE=700
		-D_GNU_SOURCE
		-D__iceTea__RELEASE
	)

	if (CONFIG_VEE_LIB_LINK)
		# Link Options
		add_library(microEJRuntime STATIC IMPORTED)
		set_target_properties(microEJRuntime PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/lib/microejruntime.a)
		add_library(microEJApp STATIC IMPORTED)
		set_target_properties(microEJApp PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/lib/microejapp.o)

		target_link_libraries(${target} PRIVATE microEJRuntime microEJApp ${CMAKE_CURRENT_LIST_DIR}/lib/microejruntime.a)
	endif()
endif()
