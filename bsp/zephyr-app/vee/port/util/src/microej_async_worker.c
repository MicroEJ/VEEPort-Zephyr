/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

/**
 * @file
 * @brief Asynchronous Worker implementation
 * @author MicroEJ Developer Team
 * @version 0.6.0
 */

#include "microej_async_worker.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Concurrency model
 * -----------------
 * The free jobs list and the waiting threads list are updated with the worker mutex held, and both are
 * updated in the same critical section. Taking a free job and joining the waiting list are one
 * indivisible decision: a job freed between the two would notify nobody, and the calling thread would
 * never be resumed.
 *
 * The mutex is always taken with OSAL_INFINITE_TIME on a handle that cannot be NULL, so the take
 * cannot fail and there is no status to act on.
 *
 * MICROEJ_ASYNC_WORKER_free_job() resumes a Java thread outside the critical section, because
 * SNI_resumeJavaThread() may reschedule. Beware that the waiting threads list therefore has two
 * consumers: the virtual machine task through a callback, and the worker task itself when a resume
 * fails because the Java thread that ordered the job was killed.
 */

// Entry point of the async worker task.
static void * MICROEJ_ASYNC_WORKER_loop(void *args);

// Generic method for MICROEJ_ASYNC_WORKER_async_exec and MICROEJ_ASYNC_WORKER_async_exec_no_wait
static MICROEJ_ASYNC_WORKER_status_t MICROEJ_ASYNC_WORKER_async_exec_intern(MICROEJ_ASYNC_WORKER_handle_t *async_worker,
                                                                            MICROEJ_ASYNC_WORKER_job_t *job,
                                                                            MICROEJ_ASYNC_WORKER_action_t action,
                                                                            SNI_callback on_done_callback, bool wait);

/**
 * @brief Removes the first job from the free jobs list.
 *
 * @warning The caller must hold the worker mutex.
 *
 * @param[in] async_worker the worker owning the list.
 *
 * @return the job, or NULL when no job is free.
 */
static inline MICROEJ_ASYNC_WORKER_job_t * MICROEJ_ASYNC_WORKER_pop_free_job(
	MICROEJ_ASYNC_WORKER_handle_t *async_worker) {
	MICROEJ_ASYNC_WORKER_job_t *job = async_worker->free_jobs;

	if (job != NULL) {
		async_worker->free_jobs = job->_intern.next_free_job;
		job->_intern.next_free_job = NULL;
	}

	return job;
}

/**
 * @brief Puts a job back at the head of the free jobs list.
 *
 * @warning The caller must hold the worker mutex.
 *
 * @param[in] async_worker the worker owning the list.
 * @param[in] job the job to release.
 */
static inline void MICROEJ_ASYNC_WORKER_push_free_job(MICROEJ_ASYNC_WORKER_handle_t *async_worker,
                                                      MICROEJ_ASYNC_WORKER_job_t *job) {
	job->_intern.next_free_job = async_worker->free_jobs;
	async_worker->free_jobs = job;
}

/**
 * @brief Appends a Java thread to the waiting threads list.
 *
 * @warning The caller must hold the worker mutex.
 *
 * @param[in] async_worker the worker owning the list.
 * @param[in] thread_id id of the Java thread to append.
 *
 * @return true when the thread was appended, false when the list is full.
 */
static inline bool MICROEJ_ASYNC_WORKER_push_waiting_thread(MICROEJ_ASYNC_WORKER_handle_t *async_worker,
                                                            int32_t thread_id) {
	uint_fast16_t tail = async_worker->waiting_threads_tail;
	uint_fast16_t new_tail = tail + 1u;
	bool appended = false;

	if (new_tail >= async_worker->waiting_threads_length) {
		new_tail = 0u;
	}

	// One slot is always left free, which is how a full list is told apart from an empty one.
	if (new_tail != async_worker->waiting_threads_head) {
		async_worker->waiting_threads[tail] = thread_id;
		async_worker->waiting_threads_tail = new_tail;
		appended = true;
	}

	return appended;
}

/**
 * @brief Removes the oldest Java thread from the waiting threads list.
 *
 * @warning The caller must hold the worker mutex.
 *
 * @param[in] async_worker the worker owning the list.
 *
 * @return the id of the Java thread, or <code>SNI_ERROR</code> when no thread is waiting.
 */
static inline int32_t MICROEJ_ASYNC_WORKER_pop_waiting_thread(MICROEJ_ASYNC_WORKER_handle_t *async_worker) {
	uint_fast16_t head = async_worker->waiting_threads_head;
	int32_t thread_id = SNI_ERROR;

	if (head != async_worker->waiting_threads_tail) {
		uint_fast16_t new_head = head + 1u;

		thread_id = async_worker->waiting_threads[head];

		if (new_head >= async_worker->waiting_threads_length) {
			new_head = 0u;
		}

		async_worker->waiting_threads_head = new_head;
	}

	return thread_id;
}

MICROEJ_ASYNC_WORKER_status_t MICROEJ_ASYNC_WORKER_initialize(MICROEJ_ASYNC_WORKER_handle_t *async_worker,
                                                              uint8_t *name,
                                                              OSAL_task_stack_t stack, int32_t priority) {
	// Check configuration
	int32_t job_count = async_worker->jobs_queue->size;
	if (job_count <= 0
	    || async_worker->waiting_threads_length <= 1 // compare with 1 because '+1' is added when declaring the array
	    ) {
		return MICROEJ_ASYNC_WORKER_INVALID_ARGS;
	}

	// Init jobs
	MICROEJ_ASYNC_WORKER_job_t *jobs = async_worker->free_jobs;
	uint8_t *params = (uint8_t *)async_worker->params;
	size_t params_sizeof = async_worker->params_sizeof;
	for (int32_t i = 0; i < job_count - 1; i++) {
		jobs[i]._intern.next_free_job = &jobs[i + 1];
		jobs[i].params = params;
		params += params_sizeof;
	}
	jobs[job_count - 1]._intern.next_free_job = NULL;
	jobs[job_count - 1].params = params;

	// Create queue
	OSAL_status_t res = OSAL_queue_create(name, &async_worker->jobs, *async_worker->jobs_queue);
	if (res != OSAL_OK) {
		return MICROEJ_ASYNC_WORKER_ERROR;
	}

	// Create mutex
	res = OSAL_mutex_create(name, &async_worker->mutex);
	if (res != OSAL_OK) {
		return MICROEJ_ASYNC_WORKER_ERROR;
	}

	// Create task
	res = OSAL_task_create((OSAL_task_entry_point_t)MICROEJ_ASYNC_WORKER_loop, name, stack, priority, async_worker,
	                       &async_worker->task);
	if (res != OSAL_OK) {
		return MICROEJ_ASYNC_WORKER_ERROR;
	}

	return MICROEJ_ASYNC_WORKER_OK;
}

MICROEJ_ASYNC_WORKER_job_t * MICROEJ_ASYNC_WORKER_allocate_job(MICROEJ_ASYNC_WORKER_handle_t *async_worker,
                                                               SNI_callback sni_retry_callback) {
	MICROEJ_ASYNC_WORKER_job_t *job = NULL;
	bool queued = false;

	OSAL_mutex_take(&async_worker->mutex, OSAL_INFINITE_TIME);
	{
		job = MICROEJ_ASYNC_WORKER_pop_free_job(async_worker);
		if (job == NULL) {
			int32_t thread_id = SNI_getCurrentJavaThreadID();
			// Taking a free job and joining the waiting list are one indivisible decision: a job freed
			// in between would notify nobody, and this thread would never be resumed.
			queued = MICROEJ_ASYNC_WORKER_push_waiting_thread(async_worker, thread_id);
		}
	}
	OSAL_mutex_give(&async_worker->mutex);

	if (job == NULL) {
		if (queued == true) {
			// Wait for a job to be freed. MICROEJ_ASYNC_WORKER_free_job() then resumes this thread and
			// sni_retry_callback runs this function again.
			SNI_suspendCurrentJavaThreadWithCallback(0, (SNI_callback)sni_retry_callback, NULL);
		} else {
			SNI_throwNativeIOException(-1, "MICROEJ_ASYNC_WORKER: thread cannot be suspended, waiting list is full.");
		}
	}

	return job;
}

MICROEJ_ASYNC_WORKER_status_t MICROEJ_ASYNC_WORKER_free_job(MICROEJ_ASYNC_WORKER_handle_t *async_worker,
                                                            MICROEJ_ASYNC_WORKER_job_t *job) {
	int32_t thread_id;

	OSAL_mutex_take(&async_worker->mutex, OSAL_INFINITE_TIME);
	{
		MICROEJ_ASYNC_WORKER_push_free_job(async_worker, job);
		thread_id = MICROEJ_ASYNC_WORKER_pop_waiting_thread(async_worker);
	}
	OSAL_mutex_give(&async_worker->mutex);

	// A thread is resumed outside the critical section: SNI_resumeJavaThread() may reschedule. An
	// invalid id means the waiting thread was killed, so unqueue the next one.
	while ((SNI_ERROR != thread_id) && (SNI_OK != SNI_resumeJavaThread(thread_id))) {
		OSAL_mutex_take(&async_worker->mutex, OSAL_INFINITE_TIME);
		{
			thread_id = MICROEJ_ASYNC_WORKER_pop_waiting_thread(async_worker);
		}
		OSAL_mutex_give(&async_worker->mutex);
	}

	return MICROEJ_ASYNC_WORKER_OK;
}

MICROEJ_ASYNC_WORKER_status_t MICROEJ_ASYNC_WORKER_async_exec(MICROEJ_ASYNC_WORKER_handle_t *async_worker,
                                                              MICROEJ_ASYNC_WORKER_job_t *job,
                                                              MICROEJ_ASYNC_WORKER_action_t action,
                                                              SNI_callback on_done_callback) {
	return MICROEJ_ASYNC_WORKER_async_exec_intern(async_worker, job, action, on_done_callback, true);
}

MICROEJ_ASYNC_WORKER_status_t MICROEJ_ASYNC_WORKER_async_exec_no_wait(MICROEJ_ASYNC_WORKER_handle_t *async_worker,
                                                                      MICROEJ_ASYNC_WORKER_job_t *job,
                                                                      MICROEJ_ASYNC_WORKER_action_t action) {
	return MICROEJ_ASYNC_WORKER_async_exec_intern(async_worker, job, action, NULL, false);
}

MICROEJ_ASYNC_WORKER_status_t MICROEJ_ASYNC_WORKER_async_exec_intern(MICROEJ_ASYNC_WORKER_handle_t *async_worker,
                                                                     MICROEJ_ASYNC_WORKER_job_t *job,
                                                                     MICROEJ_ASYNC_WORKER_action_t action,
                                                                     SNI_callback on_done_callback, bool wait) {
	job->_intern.action = action;

	if (wait == true) {
		job->_intern.thread_id = SNI_getCurrentJavaThreadID();
	} else {
		job->_intern.thread_id = SNI_ERROR;
	}

	OSAL_status_t res = OSAL_queue_post(&async_worker->jobs, job);
	if (res == OSAL_OK) {
		if (wait == true) {
			SNI_suspendCurrentJavaThreadWithCallback(0, (SNI_callback)on_done_callback, job);
		}
		return MICROEJ_ASYNC_WORKER_OK;
	} else {
		SNI_throwNativeIOException(-1, "MICROEJ_ASYNC_WORKER: Internal error.");
		return MICROEJ_ASYNC_WORKER_ERROR;
	}
}

MICROEJ_ASYNC_WORKER_job_t * MICROEJ_ASYNC_WORKER_get_job_done(void) {
	MICROEJ_ASYNC_WORKER_job_t *job = NULL;
	SNI_getCallbackArgs((void **)&job, NULL);
	return job;
}

static void * MICROEJ_ASYNC_WORKER_loop(void *args) {
	MICROEJ_ASYNC_WORKER_handle_t *async_worker = (MICROEJ_ASYNC_WORKER_handle_t *)args;

	while (1) {
		MICROEJ_ASYNC_WORKER_job_t *job;
		OSAL_status_t res = OSAL_queue_fetch(&async_worker->jobs, (void **)&job, OSAL_INFINITE_TIME);

		if (res == OSAL_OK) {
			// New job to execute
			job->_intern.action(job);
			if (job->_intern.thread_id != SNI_ERROR) {
				if (SNI_OK != SNI_resumeJavaThread(job->_intern.thread_id)) {
					MICROEJ_ASYNC_WORKER_free_job(async_worker, job);
				}
			} else {
				MICROEJ_ASYNC_WORKER_free_job(async_worker, job);
			}
		}
	}
	return NULL;
}

#ifdef __cplusplus
}
#endif
