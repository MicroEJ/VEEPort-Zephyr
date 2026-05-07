/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

/**
 * @file
 * @brief MicroEJ Security low level API implementation for MbedTLS Library.
 * @author @CCO_AUTHOR@
 * @version @CCO_VERSION@
 * @date @CCO_DATE@
 */

#include <LLSEC_impl.h>
#include <LLSEC_mbedtls.h>
#include "mbedtls/platform.h"

MICROEJ_ASYNC_WORKER_worker_declare(llsec_worker, LLSEC_WORKER_JOB_COUNT, union LLSEC_worker_params,
                                    LLSEC_WAITING_LIST_SIZE);
OSAL_task_stack_declare(sec_worker_stack, LLSEC_WORKER_STACK_SIZE);

void LLSEC_IMPL_initialize(void) {
	MICROEJ_ASYNC_WORKER_status_t status = MICROEJ_ASYNC_WORKER_initialize(&llsec_worker, (uint8_t *)"MicroEJ LLSEC",
	                                                                       sec_worker_stack, LLSEC_WORKER_PRIORITY);
	if (status == MICROEJ_ASYNC_WORKER_INVALID_ARGS) {
		int32_t sni_rc = SNI_throwNativeException(status, "Invalid argument for SEC async worker");
		LLSEC_ASSERT(sni_rc == SNI_OK);
	} else if (status == MICROEJ_ASYNC_WORKER_ERROR) {
		int32_t sni_rc = SNI_throwNativeException(status, "Error while initializing SEC async worker");
		LLSEC_ASSERT(sni_rc == SNI_OK);
	} else {
		// Default case: MICROEJ_ASYNC_WORKER_OK
	}
}

void llsec_mbedtls_gen_random_str(char *str, size_t buflen) {
	srand((unsigned int)time(NULL));

	int idx;
	for (idx = 0; idx < (buflen - 1); idx++) {
		int flag = rand() % 3;
		switch (flag) {
		case 0:
			str[idx] = 'A' + (char)(rand() % 26);
			break;
		case 1:
			str[idx] = 'a' + (char)(rand() % 26);
			break;
		case 2:
			str[idx] = '0' + (char)(rand() % 10);
			break;
		default:
			str[idx] = 'x';
			break;
		}
	}
	str[buflen - 1] = '\0';
}
