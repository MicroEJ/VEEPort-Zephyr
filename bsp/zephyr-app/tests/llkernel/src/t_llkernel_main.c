/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

#include <stdio.h>
#include <stdarg.h>
#include "LLKERNEL_RAM.h"
#include "t_llkernel.h"
#include "embUnit.h"
#include "u_print.h"

#define LLKERNEL_VERSION "v1.2.0"

void T_LLKERNEL_main(void) {
	UTIL_print_string("\nT_LLKERNEL " LLKERNEL_VERSION "\n");
	TestRunner_start();
	TestRunner_runTest(T_LLKERNEL_tests());
	TestRunner_end();
	return;
}

#ifdef KERNEL_RAM_IMPL_BESTFIT

/**
 * @brief Kernel applications buffer size.
 *
 */
#define KERNEL_WORKING_BUFFER_SIZE 0xF0000

/**
 * @brief Declare the section where will be allocated the Applications.
 *
 */
uint8_t kernel_working_buffer[KERNEL_WORKING_BUFFER_SIZE];
#endif

int main(void) {
#ifdef KERNEL_RAM_IMPL_BESTFIT
	/* Initialize KERNEL allocator. */
	LLKERNEL_RAM_BESTFIT_initialize((int32_t)&kernel_working_buffer[0],
	                                (int32_t)&kernel_working_buffer[KERNEL_WORKING_BUFFER_SIZE]);
#endif // KERNEL_RAM_IMPL_BESTFIT

	/* Start the LLkernel tests */
	T_LLKERNEL_main();
	return 0;
}
