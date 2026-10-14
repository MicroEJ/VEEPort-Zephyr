/*
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdint.h>
#include <psa/crypto.h>
#include <LLCRYPTO_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "LLCRYPTO_configuration.h"
#if defined(CONFIG_HW_UNIQUE_KEY)
#include <hw_unique_key.h>
#endif

VEEPORT_LOGGER_DEFINE_MODULE(LLCRYPTO_LOG_MODULE)

void LLCRYPTO_IMPL_initialize(void) {
#if defined(CONFIG_HW_UNIQUE_KEY)
	if (!hw_unique_key_are_any_written()) {
		if (HW_UNIQUE_KEY_SUCCESS != hw_unique_key_write_random()) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "Hardware unique key init failed");
		}
	}
#endif

	/*
	 * psa_crypto_init() is idempotent (PSA spec: once a call succeeds, subsequent
	 * calls are guaranteed to succeed). Call it unconditionally so PSA is initialized
	 * regardless of whether Zephyr's CONFIG_PSA_CRYPTO_SYS_INIT SYS_INIT already ran
	 * it at POST_KERNEL.
	 */
	if (PSA_SUCCESS != psa_crypto_init()) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "PSA init failed");
	}
}
