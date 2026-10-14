/*
 * Copyright 2023-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdint.h>
#include <LLCRYPTO_SECRET_KEY_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_key.h"
#include "sni_extras.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

int32_t LLCRYPTO_SECRET_KEY_IMPL_get_encoded_max_size(int32_t native_id) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();

	struct llcrypto_key *key = (struct llcrypto_key *)native_id;
	int32_t max_size = llcrypto_key_get_encoded_max_size(key);

	if (0 == max_size) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "Cannot get encoded max size");
	}

	LLCRYPTO_PROFILE_END();
	VEEPORT_LOGGER_LOG_DBG("Return size = %d", max_size);
	return max_size;
}

int32_t LLCRYPTO_SECRET_KEY_IMPL_get_encoded(int32_t native_id, uint8_t *output, int32_t output_length) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();

	struct llcrypto_key *key = (struct llcrypto_key *)native_id;
	int32_t key_length = LLCRYPTO_ERROR;

	/* Defense-in-depth: reject a negative or oversized output_length before llcrypto_key_get_encoded
	 * casts it to size_t, where a negative would become a huge capacity for psa_export_key. */
	if ((output_length < 0) || (output_length > microej_sni_get_array_length(output))) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_LENGTH);
		key_length = SNI_IGNORED_RETURNED_VALUE;
	} else {
		key_length = llcrypto_key_get_encoded(key, output, output_length);
		if (0 == key_length) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_EXPORT_FAILED);
		}
	}

	LLCRYPTO_PROFILE_END();
	VEEPORT_LOGGER_LOG_DBG("Return size = %d", key_length);
	return key_length;
}

int32_t LLCRYPTO_SECRET_KEY_IMPL_create(uint8_t *raw_key, int32_t raw_key_length, uint8_t *algorithm) {
	VEEPORT_LOGGER_LOG_DBG("");

	/* Defense-in-depth: reject a non-positive or oversized length here, while it is still signed, before it
	 * is passed as size_t to llcrypto_key_from_secret_key (malloc/memcpy), where a negative would become huge
	 * and an oversized one would read past the end of the raw_key array. */
	if ((raw_key_length <= 0) || (raw_key_length > microej_sni_get_array_length(raw_key))) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_LENGTH);
		return (intptr_t)NULL;
	}

	struct llcrypto_key *key = llcrypto_key_from_secret_key(raw_key, raw_key_length, algorithm);
	if (NULL == key) {
		/* llcrypto_key_from_secret_key only fails on allocation (malloc or SNI resource table):
		 * report OOM. */
		VEEPORT_ERRORS_throw_runtime_exception(LLCRYPTO_ERROR_OOM);
	}

	return (intptr_t)key;
}

int32_t LLCRYPTO_SECRET_KEY_IMPL_nativeGetCloseId(void) {
	VEEPORT_LOGGER_LOG_DBG("");

	return (intptr_t)llcrypto_key_free;
}
