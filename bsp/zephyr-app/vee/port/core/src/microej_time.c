/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

#include <zephyr/kernel.h>

#include "microej_time.h"
/*
 *********************************************************************************************************
 *                                             DEFINES
 *********************************************************************************************************
 */

/*
 *********************************************************************************************************
 * 	                                      INTERNAL FUNCTIONS
 *********************************************************************************************************
 */

/** Offset in ms from system time to application time */
static int64_t microej_app_time_offset_ms = 0;

/*
 *********************************************************************************************************
 * 	                                      PUBLIC FUNCTIONS
 *********************************************************************************************************
 */

void microej_time_init(void) {
}

int64_t microej_time_get_application_time(void) {
	int64_t time_ms = k_uptime_get();
	time_ms += microej_app_time_offset_ms;
	return time_ms;
}

void microej_time_set_application_time(int64_t t) {
	microej_app_time_offset_ms = t - k_uptime_get();
}
