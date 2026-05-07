/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

#include <stddef.h>
#include "microej_main.h"
#include "t_core_main.h"

int main(void) {
	T_CORE_main();
	microej_main(0, NULL);
	return 0;
}
