/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

#ifndef OSAL_PORTMACRO_H
#define OSAL_PORTMACRO_H

/**
 * @file
 * @brief OS Abstraction Layer Zephyr OS port macro
 * @author MicroEJ Developer Team
 * @version 1.0.1
 * @date 24 June 2021
 */

#include <stdint.h>
#include "zephyr/kernel.h"
#include "zephyr/kernel/thread_stack.h"

/** @brief Custom OS type definitions */
#define OSAL_CUSTOM_TYPEDEF

/** @brief task function entry point */
typedef k_thread_entry_t OSAL_task_entry_point_t;

/** @brief OS task handle */
typedef struct k_thread OSAL_task_handle_t;

/** @brief OS queue handle */
typedef struct k_msgq OSAL_queue_handle_t;

/** @brief OS counter semaphore handle */
typedef struct k_sem OSAL_counter_semaphore_handle_t;

/** @brief OS binary semaphore handle */
typedef struct k_sem OSAL_binary_semaphore_handle_t;

/** @brief OS mutex handle */
typedef struct k_mutex OSAL_mutex_handle_t;

/** @brief OS task stack */
typedef struct {
	k_thread_stack_t *stack;
	uint32_t size;
} OSAL_task_stack_t;

/** @brief OS queue */
typedef struct _OSAL_queue_t {
	int32_t size;
	void *addr;
} OSAL_queue_t;

/*
 * @brief Declare a task stack.
 *
 * @param[in] _name name of the variable that defines the stack.
 * @param[in] _size size of the stack in bytes. _size must be compile time constant value.
 */
#define OSAL_task_stack_declare(_name, _size)       K_THREAD_STACK_DEFINE(mem_ ## _name, _size); \
		OSAL_task_stack_t _name = { .stack = mem_ ## _name, .size = _size };
/*
 * @brief Declare a queue.
 *
 * @param[in] _name name of the variable that defines the queue address.
 * @param[in] _size number of items that can be stored in the queue. _size must be compile time constant value.
 */
#define OSAL_queue_declare(_name, _size) OSAL_queue_t _name = { .size = _size, .addr = NULL }

#endif // OSAL_PORTMACRO_H
