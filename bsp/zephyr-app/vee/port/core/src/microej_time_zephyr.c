/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

#include "microej_time.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "microej.h"

/*
 *********************************************************************************************************
 *                                             DEFINES
 *********************************************************************************************************
 */

/*
 *********************************************************************************************************
 * 	                                      PUBLIC FUNCTIONS
 *********************************************************************************************************
 */

int64_t microej_time_get_application_time(void) {
	struct timespec ts;

	if (sys_clock_gettime(SYS_CLOCK_REALTIME, &ts) != 0) {
		// TODO error handling
	}
	int64_t milliseconds = ((int64_t)ts.tv_sec * MSEC_PER_SEC);
	milliseconds += ((int64_t)ts.tv_nsec / NSEC_PER_MSEC);
	return milliseconds;
}

void microej_time_set_application_time(int64_t time_millis) {
	struct timespec ts;

	// TimeSpec represents a time as an amount of seconds + an amount of nano seconds
	int64_t tv_sec = time_millis / MSEC_PER_SEC;
	int64_t tv_millisec = time_millis - (tv_sec * MSEC_PER_SEC);
	int32_t tv_nanosec = (int32_t)tv_millisec * NSEC_PER_MSEC;

	ts.tv_sec = (int32_t)tv_sec;
	ts.tv_nsec = tv_nanosec;

	// can only be called by ej.bon.Util.setCurrentTimeMillis, so Java system/application time (i.e. REALTIME)
	if (sys_clock_settime(SYS_CLOCK_REALTIME, &ts) != 0) {
		// TODO error handling
	}
}

#ifdef __cplusplus
}
#endif
