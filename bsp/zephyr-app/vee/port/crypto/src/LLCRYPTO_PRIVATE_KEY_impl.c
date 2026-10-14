/*
 * Copyright 2021-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdint.h>
#include <stddef.h>
#include <LLCRYPTO_PRIVATE_KEY_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "LLCRYPTO_configuration.h"
#include "sni_extras.h"
#include "LLCRYPTO_key.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

int32_t LLCRYPTO_PRIVATE_KEY_IMPL_get_encoded_max_size(int32_t native_id) {
	LLCRYPTO_UNUSED_PARAM(native_id);
	VEEPORT_LOGGER_LOG_DBG("");

	return LLCRYPTO_PRIVATE_KEY_MAX_PKCS8_LEN;
}

int32_t LLCRYPTO_PRIVATE_KEY_IMPL_get_encode(int32_t native_id, uint8_t *output, int32_t outputLength) {
	VEEPORT_LOGGER_LOG_DBG("");
	const struct llcrypto_key *key = (const struct llcrypto_key *)native_id;
	int32_t length = 0;

	if ((outputLength < 0) || (outputLength > microej_sni_get_array_length(output))) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_LENGTH);
		length = SNI_IGNORED_RETURNED_VALUE;
	} else {
		length = llcrypto_key_export_pkcs8(key, output, (size_t)outputLength);
		if (LLCRYPTO_ERROR_OOM == length) {
			/* Returned, not thrown, so that the Crypto library retries after a GC. */
			VEEPORT_LOGGER_LOG_DBG("export failed: out of memory");
		} else if (length <= 0) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_EXPORT_FAILED);
			length = SNI_IGNORED_RETURNED_VALUE;
		} else {
			/* length is the size of the encoded key */
		}
	}

	return length;
}

int32_t LLCRYPTO_PRIVATE_KEY_IMPL_get_output_size(int32_t native_id) {
	VEEPORT_LOGGER_LOG_DBG("");
	const struct llcrypto_key *key = (const struct llcrypto_key *)native_id;

	int32_t output_size = llcrypto_key_get_output_size(key);
	if (output_size <= 0) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "Failed to get private key output size");
		output_size = SNI_IGNORED_RETURNED_VALUE;
	}

	return output_size;
}
