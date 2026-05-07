/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

#if CONFIG_MICROEJ_CORE
#include "microej_main.h"
#include "LLMJVM.h"
#include "sni.h"
#endif // CONFIG_MICROEJ_CORE
#include "stddef.h"
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
	microej_main(0, NULL);
#endif // CONFIG_MICROEJ_CORE

#ifdef CONFIG_ARCH_POSIX
	nsi_exit(0);
#endif
	return 0;
}
