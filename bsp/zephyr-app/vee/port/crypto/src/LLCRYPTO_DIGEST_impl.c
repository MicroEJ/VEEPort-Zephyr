/*
 * Copyright 2021-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <psa/crypto.h>
#include <LLCRYPTO_DIGEST_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_utils.h"
#include "sni_extras.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

/*
 * LLAPI related functions & struct
 */
typedef struct {
	const char *name;
	psa_algorithm_t algorithm;
	LLCRYPTO_DIGEST_algorithm_desc description;
} LLCRYPTO_DIGEST_algorithm;

/*
 * Generic PSA Crypto function
 */
/* Returns LLCRYPTO_SUCCESS, or the error code to throw on failure: LLCRYPTO_ERROR_OOM when the
 * failure is an allocation (so the Java caller can run a GC and retry once), LLCRYPTO_ERROR
 * otherwise. */
static int32_t llcrypto_digest_init(psa_hash_operation_t **native_id, psa_algorithm_t algorithm) {
	int32_t return_code = LLCRYPTO_ERROR_OOM;

	psa_hash_operation_t *op_handle = malloc(sizeof(psa_hash_operation_t));
	if (NULL != op_handle) {
		/* Initialize the operation to a known state before calling psa_hash_setup */
		*op_handle = (psa_hash_operation_t)PSA_HASH_OPERATION_INIT;

		/* Create an operation handle and save it */
		psa_status_t status = psa_hash_setup(op_handle, algorithm);
		if (PSA_SUCCESS == status) {
			*native_id = op_handle;
			return_code = LLCRYPTO_SUCCESS;
		} else {
			VEEPORT_LOGGER_LOG_DBG("psa_hash_setup failed: %d", status);
			free(op_handle);
			return_code = llcrypto_sni_error_code(status);
		}
	}

	return return_code;
}

static void llcrypto_digest_close(void *native_id) {
	psa_hash_operation_t *op_handle = (psa_hash_operation_t *)native_id;
	psa_status_t status = psa_hash_abort(op_handle);

	/*
	 * psa_hash_abort can fail with PSA_ERROR_COMMUNICATION_FAILURE,
	 * PSA_ERROR_HARDWARE_FAILURE or PSA_ERROR_CORRUPTION_DETECTED.
	 * Errors cannot be propagated since this is a void close callback.
	 */
	if (PSA_SUCCESS != status) {
		VEEPORT_LOGGER_LOG_DBG("psa_hash_abort failed: %d", status);
	}

	free(op_handle);
}

/*
 * LLAPI Implementation
 */
int32_t LLCRYPTO_DIGEST_IMPL_get_algorithm_description(uint8_t *algorithm_name,
                                                       LLCRYPTO_DIGEST_algorithm_desc *algorithm_desc) {
	static const LLCRYPTO_DIGEST_algorithm available_digest_algorithms[] = {
#if defined(PSA_WANT_ALG_MD5)
		{
			.name = "MD5",
			.algorithm = PSA_ALG_MD5,
			.description = {
				.digest_length = PSA_HASH_LENGTH(PSA_ALG_MD5)
			}
		},
#endif // PSA_WANT_ALG_MD5
#if defined(PSA_WANT_ALG_SHA_1)
		{
			.name = "SHA-1",
			.algorithm = PSA_ALG_SHA_1,
			.description = {
				.digest_length = PSA_HASH_LENGTH(PSA_ALG_SHA_1)
			}
		},
#endif // PSA_WANT_ALG_SHA_1
#if defined(PSA_WANT_ALG_SHA_224)
		{
			.name = "SHA-224",
			.algorithm = PSA_ALG_SHA_224,
			.description = {
				.digest_length = PSA_HASH_LENGTH(PSA_ALG_SHA_224)
			}
		},
#endif // PSA_WANT_ALG_SHA_224
#if defined(PSA_WANT_ALG_SHA_256)
		{
			.name = "SHA-256",
			.algorithm = PSA_ALG_SHA_256,
			.description = {
				.digest_length = PSA_HASH_LENGTH(PSA_ALG_SHA_256)
			}
		},
#endif // PSA_WANT_ALG_SHA_256
#if defined(PSA_WANT_ALG_SHA_384)
		{
			.name = "SHA-384",
			.algorithm = PSA_ALG_SHA_384,
			.description = {
				.digest_length = PSA_HASH_LENGTH(PSA_ALG_SHA_384)
			}
		},
#endif // PSA_WANT_ALG_SHA_384
#if defined(PSA_WANT_ALG_SHA_512)
		{
			.name = "SHA-512",
			.algorithm = PSA_ALG_SHA_512,
			.description = {
				.digest_length = PSA_HASH_LENGTH(PSA_ALG_SHA_512)
			}
		}
#endif // PSA_WANT_ALG_SHA_512
	};
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	int32_t return_code = LLCRYPTO_ERROR;
	/* Reject an algorithm_desc array too small to hold the description before the memcpy writes into it. */
	if (microej_sni_get_array_length(algorithm_desc) < (int32_t)sizeof(LLCRYPTO_DIGEST_algorithm_desc)) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_BUFFER_TOO_SMALL);
		return_code = SNI_IGNORED_RETURNED_VALUE;
	} else {
		int32_t nb_algorithms = sizeof(available_digest_algorithms) / sizeof(LLCRYPTO_DIGEST_algorithm);
		const LLCRYPTO_DIGEST_algorithm *algorithm = &available_digest_algorithms[0];

		while (--nb_algorithms >= 0) {
			if (strcmp((char *)algorithm_name, (algorithm->name)) == 0) {
				(void)memcpy(algorithm_desc, &(algorithm->description), sizeof(LLCRYPTO_DIGEST_algorithm_desc));
				break;
			}
			algorithm++;
		}

		if (0 <= nb_algorithms) {
			return_code = (intptr_t)algorithm;
		}
	}
	LLCRYPTO_PROFILE_END();
	VEEPORT_LOGGER_LOG_DBG("return_code %d", return_code);
	return return_code;
}

int32_t LLCRYPTO_DIGEST_IMPL_init(int32_t algorithm_id) {
	int32_t return_code = LLCRYPTO_SUCCESS;
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	psa_hash_operation_t *native_id = NULL;
	const LLCRYPTO_DIGEST_algorithm *algorithm = (const LLCRYPTO_DIGEST_algorithm *)algorithm_id;

	return_code = llcrypto_digest_init(&native_id, algorithm->algorithm);

	if (LLCRYPTO_SUCCESS != return_code) {
		VEEPORT_LOGGER_LOG_DBG("init failed: %d", return_code);
		/* llcrypto_digest_init returned the error code to throw (LLCRYPTO_ERROR_OOM on allocation
		 * failure). */
		llcrypto_throw_error(return_code, LLCRYPTO_MSG_INIT_FAILED);
	} else {
		/* register SNI native resource */
		if (SNI_OK != SNI_registerResource(native_id, llcrypto_digest_close, NULL)) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_SNI_REGISTER_FAILED);
			llcrypto_digest_close(native_id);
			return_code = LLCRYPTO_ERROR;
		} else {
			return_code = (intptr_t)native_id;
		}
	}
	LLCRYPTO_PROFILE_END();
	return return_code;
}

void LLCRYPTO_DIGEST_IMPL_close(int32_t algorithm_id, int32_t native_id) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	LLCRYPTO_UNUSED_PARAM(algorithm_id);
	psa_hash_operation_t *op_handle = (psa_hash_operation_t *)native_id;

	llcrypto_unregister_and_close(op_handle, llcrypto_digest_close);
	LLCRYPTO_PROFILE_END();
}

void LLCRYPTO_DIGEST_IMPL_update(int32_t algorithm_id, int32_t native_id, uint8_t *buffer, int32_t buffer_offset,
                                 int32_t buffer_length) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	LLCRYPTO_UNUSED_PARAM(algorithm_id);
	psa_hash_operation_t *op_handle = (psa_hash_operation_t *)native_id;
	int32_t buffer_array_length = microej_sni_get_array_length(buffer);
	if (!llcrypto_bounds_ok(buffer_array_length, buffer_offset, buffer_length)) {
		VEEPORT_LOGGER_LOG_DBG("invalid buffer_offset %d / buffer_length %d (array %d)",
		                       buffer_offset, buffer_length, buffer_array_length);
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_OFFSET_LENGTH);
	} else {
		psa_status_t status = psa_hash_update(op_handle, &buffer[buffer_offset], (size_t)buffer_length);
		if (PSA_SUCCESS != status) {
			VEEPORT_LOGGER_LOG_DBG("update failed: %d", (int)status);
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_UPDATE_FAILED);
		}
	}
	LLCRYPTO_PROFILE_END();
}

void LLCRYPTO_DIGEST_IMPL_digest(int32_t algorithm_id, int32_t native_id, uint8_t *out, int32_t out_offset,
                                 int32_t out_length) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	LLCRYPTO_UNUSED_PARAM(algorithm_id);
	psa_hash_operation_t *op_handle = (psa_hash_operation_t *)native_id;
	int32_t out_array_length = microej_sni_get_array_length(out);
	if (!llcrypto_bounds_ok(out_array_length, out_offset, out_length)) {
		VEEPORT_LOGGER_LOG_DBG("invalid out_offset %d / out_length %d (array %d)",
		                       out_offset, out_length, out_array_length);
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_OFFSET_LENGTH);
	} else {
		size_t hash_length = 0;
		psa_status_t status = psa_hash_finish(op_handle, &out[out_offset], (size_t)out_length, &hash_length);
		if (PSA_SUCCESS != status) {
			VEEPORT_LOGGER_LOG_DBG("digest failed: %d", (int)status);
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "digest failed");
		}
	}
	LLCRYPTO_PROFILE_END();
}

int32_t LLCRYPTO_DIGEST_IMPL_get_close_id(int32_t algorithm_id) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_UNUSED_PARAM(algorithm_id);
	return (intptr_t)llcrypto_digest_close;
}
