/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

#include <stdio.h>
#include <stddef.h>
#include "veeport_main.h"
#include "t_core_main.h"

int main(void) {
	T_CORE_main();
	int result = veeport_main(0, NULL, NULL);
	if (0 != result) {
		printf("veeport_main returned an error: %d\n", result);
	}
	return 0;
}
