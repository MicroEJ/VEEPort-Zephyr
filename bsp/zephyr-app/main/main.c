/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

#if CONFIG_MICROEJ_CORE
#include "veeport_main.h"
#include "LLMJVM.h"
#include "sni.h"
#endif // CONFIG_MICROEJ_CORE
#include <stddef.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);
#ifdef CONFIG_ARCH_POSIX
#include <nsi_main.h>
#endif
#ifdef CONFIG_MICROEJ_KF
#include "LLKERNEL_RAM.h"

#ifdef KERNEL_RAM_IMPL_BESTFIT

/**
 * @brief Kernel applications buffer size.
 *
 */
#ifdef CONFIG_MICROEJ_KF_KERNEL_WORKING_BUFFER_SIZE
#define KERNEL_WORKING_BUFFER_SIZE CONFIG_MICROEJ_KF_KERNEL_WORKING_BUFFER_SIZE
#else
#define KERNEL_WORKING_BUFFER_SIZE 0xF0000
#endif // CONFIG_MICROEJ_KF_KERNEL_WORKING_BUFFER_SIZE

/**
 * @brief Declare the section where will be allocated the Applications.
 *
 */
uint8_t kernel_working_buffer[KERNEL_WORKING_BUFFER_SIZE];
#endif // KERNEL_RAM_IMPL_BESTFIT
#endif // CONFIG_MICROEJ_KF

int main(void) {
#ifdef CONFIG_MICROEJ_KF
#ifdef KERNEL_RAM_IMPL_BESTFIT
	/* Initialize KERNEL allocator. */
	LLKERNEL_RAM_BESTFIT_initialize((int32_t)&kernel_working_buffer[0],
	                                (int32_t)&kernel_working_buffer[KERNEL_WORKING_BUFFER_SIZE]);
#endif // KERNEL_RAM_IMPL_BESTFIT
#endif // CONFIG_MICROEJ_KF

#if CONFIG_MICROEJ_CORE
	int result = veeport_main(0, NULL, NULL);
	if (0 != result) {
		LOG_ERR("veeport_main error: %d", result);
	}
#endif // CONFIG_MICROEJ_CORE

#ifdef CONFIG_ARCH_POSIX
	nsi_exit(0);
#endif
	return 0;
}
