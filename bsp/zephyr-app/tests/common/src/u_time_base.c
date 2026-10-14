/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <zephyr/sys/clock.h>

#include "../inc/u_time_base.h"
#include "../inc/bsp_util.h"
#include "config.h"
/* Private constant declarations */

/* private variable definitions */

/* public function definitions */

void UTIL_TIME_BASE_initialize(void) {
	// Nothing to do
}

int64_t UTIL_TIME_BASE_getTime(void) {
	struct timespec ts;

	if (sys_clock_gettime(SYS_CLOCK_REALTIME, &ts) != 0) {
		// TODO error handling
	}
	int64_t microseconds = ((int64_t)ts.tv_sec * USEC_PER_SEC);
	microseconds += ((int64_t)ts.tv_nsec / NSEC_PER_USEC);
	return microseconds;
}

/** private function definitions */
