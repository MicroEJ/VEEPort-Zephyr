/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

/**
 * @file
 * @brief OS Abstraction Layer Zephyr OS implementation
 * @author MicroEJ Developer Team
 * @version 1.0.1
 * @date 24 June 2021
 */

#include <stdint.h>
#include "osal.h"

/**
 * @brief Convert time from milliseconds to zephyr ticks.
 *
 * @param[in] time in milliseconds
 *
 * @return time in zephyr ticks
 */
static k_timeout_t OSAL_zephyr_ms_to_tick(uint32_t milliseconds) {
	k_timeout_t tick;
	if (milliseconds == OSAL_INFINITE_TIME) {
		tick = K_FOREVER;
	} else {
		if (milliseconds == 0) {
			tick = K_NO_WAIT;
		} else {
			tick = K_MSEC(milliseconds);
		}
	}

	return tick;
}

/**
 * @brief Create an OS task and start it.
 *
 * @param[in] entry_point function called at task startup
 * @param[in] name the task name
 * @param[in] stack task stack declared using OSAL_task_stack_declare() macro
 * @param[in] priority task priority
 * @param[in] parameters task entry parameters. NULL if no entry parameters
 * @param[in,out] handle pointer on a task handle
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_task_create(OSAL_task_entry_point_t entry_point, uint8_t *name, OSAL_task_stack_t stack,
                               int32_t priority, void *parameters, OSAL_task_handle_t *handle) {
	if (handle == NULL) {
		return OSAL_WRONG_ARGS;
	}

	k_tid_t thread = k_thread_create(handle, stack.stack,
	                                 stack.size,
	                                 entry_point,
	                                 parameters, NULL, NULL,
	                                 priority, 0, K_NO_WAIT);
	if (thread == NULL) {
		return OSAL_ERROR;
	}

	k_thread_system_pool_assign(thread);

 #if CONFIG_THREAD_NAME
	if (k_thread_name_set(thread, (const char *)name) != 0) {
		return OSAL_ERROR;
	}
 #endif

	return OSAL_OK;
}

/**
 * @brief Delete an OS task and start it.
 *
 * @param[in] handle pointer on the task handle
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_task_delete(OSAL_task_handle_t *handle) {
	if (handle == NULL) {
		return OSAL_WRONG_ARGS;
	}

	k_thread_abort(handle);

	return OSAL_OK;
}

/**
 * @brief Get the handle of the current OS task.
 *
 * @param[in,out] handle pointer on a task handle
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_task_get_current(OSAL_task_handle_t *handle) {
	*handle = *(OSAL_task_handle_t *)k_current_get();

	return OSAL_OK;
}

/**
 * @brief Create an OS queue with a predefined queue size.
 *
 * @param[in] name queue name
 * @param[in,out] handle pointer on a queue handle
 * @param[in] queue structure containing address and size declared using OSAL_queue_declare() macro
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_queue_create(uint8_t *name, OSAL_queue_handle_t *handle, OSAL_queue_t queue) {
	(void)name;
	OSAL_status_t status = OSAL_ERROR;

	if (handle == NULL) {
		status = OSAL_WRONG_ARGS;
	} else if (0 == k_msgq_alloc_init(handle, sizeof(uint32_t), queue.size)) {
		status = OSAL_OK;
	} else {
		status = OSAL_ERROR;
	}

	return status;
}

/**
 * @brief Delete an OS queue.
 *
 * @param[in] handle pointer on the queue handle
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_queue_delete(OSAL_queue_handle_t *handle) {
	if (handle == NULL) {
		return OSAL_WRONG_ARGS;
	}
	k_msgq_purge(handle);
	k_msgq_cleanup(handle);
	return OSAL_OK;
}

/**
 * @brief Post a message in an OS queue.
 *
 * @param[in] handle pointer on the queue handle
 * @param[in] msg message to post in the message queue
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_queue_post(OSAL_queue_handle_t *handle, void *msg) {
	OSAL_status_t status = OSAL_ERROR;
	if (handle == NULL) {
		status = OSAL_WRONG_ARGS;
	} else if (0 != k_msgq_put(handle, &msg, K_NO_WAIT)) {
		status = OSAL_ERROR;
	} else {
		status = OSAL_OK;
	}
	return status;
}

/**
 * @brief Fetch a message from an OS queue. Blocks until a message arrived or a timeout occurred.
 *
 * @param[in] handle pointer on the queue handle
 * @param[in,out] msg message fetched from the OS queue
 * @param[in] timeout maximum time to wait for message arrival, OSAL_INFINITE_TIME for infinite timeout
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_queue_fetch(OSAL_queue_handle_t *handle, void **msg, uint32_t timeout) {
	OSAL_status_t status = OSAL_ERROR;
	if (handle == NULL) {
		status = OSAL_WRONG_ARGS;
	} else {
		k_timeout_t tick = OSAL_zephyr_ms_to_tick(timeout);

		if (0 != k_msgq_get(handle, msg, tick)) {
			status = OSAL_ERROR;
		} else {
			status = OSAL_OK;
		}
	}
	return status;
}

/**
 * @brief Create an OS counter semaphore with a semaphore count initial value.
 *
 * @param[in] name counter semaphore name
 * @param[in] initial_count counter semaphore initial count value
 * @param[in] max_count counter semaphore maximum count value
 * @param[in,out] handle pointer on a counter semaphore handle
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_counter_semaphore_create(uint8_t *name, uint32_t initial_count, uint32_t max_count,
                                            OSAL_counter_semaphore_handle_t *handle) {
	if (handle == NULL) {
		return OSAL_WRONG_ARGS;
	}

	k_sem_init(handle, initial_count, max_count);

	return OSAL_OK;
}

/**
 * @brief Delete an OS counter semaphore.
 *
 * @param[in] handle pointer on the counter semaphore handle
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_counter_semaphore_delete(OSAL_counter_semaphore_handle_t *handle) {
	if (handle == NULL) {
		return OSAL_WRONG_ARGS;
	}

	return OSAL_OK;
}

/**
 * @brief Take operation on OS counter semaphore. Block the current task until counter semaphore
 * become available or timeout occurred. Decrease the counter semaphore count value by 1 and
 * block the current task if count value equals to 0.
 *
 * @param[in] handle pointer on the counter semaphore handle
 * @param[in] timeout maximum time to wait until the counter semaphore become available, OSAL_INFINITE_TIME for infinite
 * timeout
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_counter_semaphore_take(OSAL_counter_semaphore_handle_t *handle, uint32_t timeout) {
	if (handle == NULL) {
		return OSAL_WRONG_ARGS;
	}

	k_timeout_t tick = OSAL_zephyr_ms_to_tick(timeout);

	if (k_sem_take(handle, tick) != 0) {
		return OSAL_ERROR;
	}

	return OSAL_OK;
}

/**
 * @brief Give operation on OS counter semaphore. Increase the counter semaphore count value by 1 and unblock the
 * current task if count value equals to 0.
 *
 * @param[in] handle pointer on the counter semaphore handle
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_counter_semaphore_give(OSAL_counter_semaphore_handle_t *handle) {
	if (handle == NULL) {
		return OSAL_WRONG_ARGS;
	}

	k_sem_give(handle);

	return OSAL_OK;
}

/**
 * @brief Create an OS binary semaphore with a semaphore count initial value (0 or 1).
 *
 * @param[in] name counter semaphore name
 * @param[in] initial_count counter semaphore initial count value
 * @param[in,out] handle pointer on a binary semaphore handle
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_binary_semaphore_create(uint8_t *name, uint32_t initial_count,
                                           OSAL_binary_semaphore_handle_t *handle) {
	if (handle == NULL) {
		return OSAL_WRONG_ARGS;
	}

	k_sem_init(handle, initial_count, 1);

	return OSAL_OK;
}

/**
 * @brief Delete an OS binary semaphore.
 *
 * @param[in] handle pointer on the binary semaphore handle
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_binary_semaphore_delete(OSAL_binary_semaphore_handle_t *handle) {
	if (handle == NULL) {
		return OSAL_WRONG_ARGS;
	}

	return OSAL_OK;
}

/**
 * @brief Take operation on OS binary semaphore. Block the current task until binary semaphore
 * become available or timeout occurred. Decrease the binary semaphore count value by 1 and
 * block the current task if count value equals to 0.
 *
 * @param[in] handle pointer on the binary semaphore handle
 * @param[in] timeout maximum time to wait until the binary semaphore become available, OSAL_INFINITE_TIME for infinite
 * timeout
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_binary_semaphore_take(OSAL_binary_semaphore_handle_t *handle, uint32_t timeout) {
	if (handle == NULL) {
		return OSAL_WRONG_ARGS;
	}

	k_timeout_t tick = OSAL_zephyr_ms_to_tick(timeout);

	if (k_sem_take(handle, tick) != 0) {
		return OSAL_ERROR;
	}

	return OSAL_OK;
}

/**
 * @brief Give operation on OS binary semaphore. Increase the binary semaphore count value by 1 and unblock the current
 * task if count value.
 * equals to 0.
 *
 * @param[in] handle pointer on the binary semaphore handle
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_binary_semaphore_give(OSAL_binary_semaphore_handle_t *handle) {
	if (handle == NULL) {
		return OSAL_WRONG_ARGS;
	}

	k_sem_give(handle);

	return OSAL_OK;
}

/**
 * @brief Create an OS mutex.
 *
 * @param[in] name mutex name
 * @param[in,out] handle pointer on a mutex handle
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_mutex_create(uint8_t *name, OSAL_mutex_handle_t *handle) {
	if (handle == NULL) {
		return OSAL_WRONG_ARGS;
	}

	if (k_mutex_init(handle) != 0) {
		return OSAL_NOMEM;
	}

	return OSAL_OK;
}

/**
 * @brief Delete an OS mutex.
 *
 * @param[in] handle pointer on the mutex handle
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_mutex_delete(OSAL_mutex_handle_t *handle) {
	if (handle == NULL) {
		return OSAL_WRONG_ARGS;
	}

	return OSAL_OK;
}

/**
 * @brief Take operation on OS mutex.
 *
 * @param[in] handle pointer on the mutex handle
 * @param[in] timeout maximum time to wait until the mutex become available, OSAL_INFINITE_TIME for infinite timeout
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_mutex_take(OSAL_mutex_handle_t *handle, uint32_t timeout) {
	if (handle == NULL) {
		return OSAL_WRONG_ARGS;
	}

	k_timeout_t tick = OSAL_zephyr_ms_to_tick(timeout);

	if (k_mutex_lock(handle, tick) != 0) {
		return OSAL_ERROR;
	}

	return OSAL_OK;
}

/**
 * @brief Give operation on OS mutex.
 *
 * @param[in] handle pointer on the mutex handle
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_mutex_give(OSAL_mutex_handle_t *handle) {
	if (handle == NULL) {
		return OSAL_WRONG_ARGS;
	}

	if (k_mutex_unlock(handle) != 0) {
		return OSAL_ERROR;
	}

	return OSAL_OK;
}

/**
 * @brief Disable the OS scheduler context switching. Prevent the OS from
 * scheduling the current thread calling #OSAL_disable_context_switching while
 * the OS scheduling is already disable has an undefined behavior. This method
 * may be called from an interrupt.
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_disable_context_switching(void) {
	k_sched_lock();

	return OSAL_NOT_SUPPORTED;
}

/**
 * @brief Reenable the OS scheduling that was disabled by #OSAL_disable_context_switching.
 * This method may be called from an interrupt.
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_enable_context_switching(void) {
	k_sched_unlock();

	return OSAL_NOT_SUPPORTED;
}

/**
 * @brief Asleep the current task during specified number of milliseconds.
 *
 * @param[in] milliseconds number of milliseconds
 *
 * @return operation status (@see OSAL_status_t)
 */
OSAL_status_t OSAL_sleep(uint32_t milliseconds) {
	k_timeout_t tick = OSAL_zephyr_ms_to_tick(milliseconds);

	k_sleep(tick);

	return OSAL_OK;
}
