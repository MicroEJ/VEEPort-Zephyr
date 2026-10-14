/*
 * C
 *
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

/**
 * @file
 * @brief Critical section of VEE Port Essentials, mapped onto a Zephyr mutex.
 * @author MicroEJ Developer Team
 */

#include <stdint.h>
#include <zephyr/kernel.h>

#include "veeport_configuration.h"
#include "veeport_logger.h"

VEEPORT_LOGGER_DEFINE_MODULE(veeport_critical_section, VEEPORT_LOGGER_LEVEL_ERR)

/**
 * @brief The mutex behind the critical section.
 *
 * @details The section of the error helpers spans a call to SNI_throwNativeException(), so it must be
 * a lock a thread can hold across that call. A kernel mutex is one. Locking the scheduler or masking
 * the interrupts is not.
 */
K_MUTEX_DEFINE(veeport_critical_section_mutex);

void VEEPORT_CRITICAL_SECTION_enter(void) {
	int32_t status = k_mutex_lock(&veeport_critical_section_mutex, K_FOREVER);
	if (0 != status) {
		VEEPORT_LOGGER_LOG_ERR("k_mutex_lock failed (%d)", status);
	}
}

void VEEPORT_CRITICAL_SECTION_exit(void) {
	int32_t status = k_mutex_unlock(&veeport_critical_section_mutex);
	if (0 != status) {
		VEEPORT_LOGGER_LOG_ERR("k_mutex_unlock failed (%d)", status);
	}
}
