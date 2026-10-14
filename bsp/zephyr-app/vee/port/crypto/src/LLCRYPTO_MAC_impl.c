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
#include <LLCRYPTO_MAC_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_utils.h"
#include "sni_extras.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

/*
 * LLAPI Context (used for native_id)
 */
typedef struct {
	psa_mac_operation_t op_handle;
	psa_key_id_t key_id;
} LLCRYPTO_MAC_ctx;

/*
 * LLAPI related functions & struct
 */
typedef struct {
	const char *name;
	psa_algorithm_t algorithm;
	LLCRYPTO_MAC_algorithm_desc description;
} LLCRYPTO_MAC_algorithm;

#if defined(PSA_WANT_ALG_HMAC)
/*
 * Generic PSA Crypto function.
 * Returns LLCRYPTO_SUCCESS, or the error code to throw on failure: LLCRYPTO_ERROR_OOM when the
 * failure is an allocation (so the Java caller can run a GC and retry once), LLCRYPTO_ERROR
 * otherwise.
 */
static int32_t llcrypto_mac_init(LLCRYPTO_MAC_ctx **native_id, psa_algorithm_t algorithm, uint8_t *key,
                                 int32_t key_length) {
	int32_t return_code = LLCRYPTO_ERROR;
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	VEEPORT_LOGGER_LOG_DBG("");

	/* Defense-in-depth: reject a non-positive or oversized key length before it is cast to size_t for
	 * psa_set_key_bits / psa_import_key, where a negative value would become a huge size and an oversized
	 * one would make psa_import_key read past the end of the key array. */
	if ((key_length <= 0) || (key_length > microej_sni_get_array_length(key))) {
		return LLCRYPTO_ERROR;
	}

	LLCRYPTO_MAC_ctx *ctx = malloc(sizeof(LLCRYPTO_MAC_ctx));
	if (NULL == ctx) {
		/* Allocation failure: return the OOM error code so the caller's throw lets Java run a GC and
		 * retry once. */
		return_code = LLCRYPTO_ERROR_OOM;
		/* cppcheck-suppress misra-c2012-15.2 ; cppcheck 2.21 false positive: goto jumps forward to
		 * 'cleanup' */
		goto cleanup;
	}
	/* Must be set before any goto cleanup: makes the cleanup-path psa_destroy_key a safe no-op if import fails. */
	ctx->key_id = PSA_KEY_ID_NULL;
	/* Initialize the operation to a known state before calling psa_mac_sign_setup */
	ctx->op_handle = (psa_mac_operation_t)PSA_MAC_OPERATION_INIT;

	/* Configure key attributes */
	psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
	psa_set_key_bits(&attributes, (size_t)key_length * LLCRYPTO_BITS_PER_BYTE);
	psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(algorithm));
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);

	/* Import the key and save it */
	psa_status_t status = psa_import_key(&attributes, key, key_length, &ctx->key_id);
	if (PSA_SUCCESS != status) {
		return_code = llcrypto_sni_error_code(status);
		goto cleanup;
	}

	/* Create an operation handle and save it */
	status = psa_mac_sign_setup(&ctx->op_handle, ctx->key_id, PSA_ALG_HMAC(algorithm));
	if (PSA_SUCCESS != status) {
		return_code = llcrypto_sni_error_code(status);
		goto cleanup;
	}

	*native_id = ctx;
	return_code = LLCRYPTO_SUCCESS;

cleanup:
	psa_reset_key_attributes(&attributes);
	if (LLCRYPTO_SUCCESS != return_code) {
		if (NULL != ctx) {
			psa_status_t destroy_status = psa_destroy_key(ctx->key_id);
			if (PSA_SUCCESS != destroy_status) {
				VEEPORT_LOGGER_LOG_WRN("psa_destroy_key failed: %d", destroy_status);
			}
			free(ctx);
		}
	}
	return return_code;
}

static int32_t llcrypto_mac_update(LLCRYPTO_MAC_ctx *native_id, const uint8_t *buffer, int32_t buffer_length) {
	VEEPORT_LOGGER_LOG_DBG("");

	return (int32_t)psa_mac_update(&native_id->op_handle, buffer, (size_t)buffer_length);
}

static int32_t llcrypto_mac_dofinal(LLCRYPTO_MAC_ctx *native_id, uint8_t *out, int32_t out_length) {
	VEEPORT_LOGGER_LOG_DBG("");

	/* mac_length is required by psa_mac_sign_finish but not reported back: the caller
	 * (LLCRYPTO_MAC_IMPL_do_final) is void and does not return the produced length to Managed Code. */
	size_t mac_length = 0;
	psa_status_t status = psa_mac_sign_finish(&native_id->op_handle, out, (size_t)out_length, &mac_length);
	return (int32_t)status;
}

static int32_t llcrypto_mac_reset(LLCRYPTO_MAC_ctx *native_id, psa_algorithm_t algorithm) {
	int32_t return_code = LLCRYPTO_SUCCESS;
	VEEPORT_LOGGER_LOG_DBG("");

	return_code = (int32_t)psa_mac_abort(&native_id->op_handle);
	if (LLCRYPTO_SUCCESS == return_code) {
		return_code = (int32_t)psa_mac_sign_setup(&native_id->op_handle, native_id->key_id, algorithm);
		if (LLCRYPTO_SUCCESS != return_code) {
			VEEPORT_LOGGER_LOG_DBG("psa_mac_sign_setup failed: %d", return_code);
		}
	} else {
		VEEPORT_LOGGER_LOG_DBG("psa_mac_abort failed: %d", return_code);
	}
	return return_code;
}

static void llcrypto_mac_close(void *native_id) {
	VEEPORT_LOGGER_LOG_DBG("");

	LLCRYPTO_MAC_ctx *ctx = (LLCRYPTO_MAC_ctx *)native_id;

	if (NULL != ctx) {
		psa_status_t abort_status = psa_mac_abort(&ctx->op_handle);

		/*
		 * psa_mac_abort can fail with PSA_ERROR_COMMUNICATION_FAILURE,
		 * PSA_ERROR_HARDWARE_FAILURE or PSA_ERROR_CORRUPTION_DETECTED.
		 * Errors cannot be propagated since this is a void close callback.
		 */
		if (PSA_SUCCESS != abort_status) {
			VEEPORT_LOGGER_LOG_DBG("psa_mac_abort failed: %d", abort_status);
		}

		psa_status_t destroy_status = psa_destroy_key(ctx->key_id);

		/*
		 * psa_destroy_key can fail with PSA_ERROR_COMMUNICATION_FAILURE,
		 * PSA_ERROR_HARDWARE_FAILURE or PSA_ERROR_CORRUPTION_DETECTED.
		 * Errors cannot be propagated since this is a void close callback.
		 * On failure, the PSA volatile key slot is leaked with no way to retry.
		 */
		if (PSA_SUCCESS != destroy_status) {
			VEEPORT_LOGGER_LOG_DBG("psa_destroy_key failed: %d", destroy_status);
		}

		free(ctx);
	}
}

#else /* !PSA_WANT_ALG_HMAC: MAC unavailable. Footprint stubs so the always-compiled LLAPI layer links.
       * Unreachable at runtime: get_algorithm_description issues no HMAC handle when HMAC is disabled. */
static int32_t llcrypto_mac_init(LLCRYPTO_MAC_ctx **native_id, psa_algorithm_t algorithm, uint8_t *key,
                                 int32_t key_length) {
	LLCRYPTO_UNUSED_PARAM(native_id);
	LLCRYPTO_UNUSED_PARAM(algorithm);
	LLCRYPTO_UNUSED_PARAM(key);
	LLCRYPTO_UNUSED_PARAM(key_length);
	return LLCRYPTO_ERROR;
}

static int32_t llcrypto_mac_update(LLCRYPTO_MAC_ctx *native_id, const uint8_t *buffer, int32_t buffer_length) {
	LLCRYPTO_UNUSED_PARAM(native_id);
	LLCRYPTO_UNUSED_PARAM(buffer);
	LLCRYPTO_UNUSED_PARAM(buffer_length);
	return LLCRYPTO_ERROR;
}

static int32_t llcrypto_mac_dofinal(LLCRYPTO_MAC_ctx *native_id, uint8_t *out, int32_t out_length) {
	LLCRYPTO_UNUSED_PARAM(native_id);
	LLCRYPTO_UNUSED_PARAM(out);
	LLCRYPTO_UNUSED_PARAM(out_length);
	return LLCRYPTO_ERROR;
}

static int32_t llcrypto_mac_reset(LLCRYPTO_MAC_ctx *native_id, psa_algorithm_t algorithm) {
	LLCRYPTO_UNUSED_PARAM(native_id);
	LLCRYPTO_UNUSED_PARAM(algorithm);
	return LLCRYPTO_ERROR;
}

static void llcrypto_mac_close(void *native_id) {
	LLCRYPTO_UNUSED_PARAM(native_id);
}

#endif // PSA_WANT_ALG_HMAC

int32_t LLCRYPTO_MAC_IMPL_get_algorithm_description(uint8_t *algorithm_name,
                                                    LLCRYPTO_MAC_algorithm_desc *algorithm_desc) {
	static const LLCRYPTO_MAC_algorithm available_mac_algorithms[] = {
#if defined(PSA_WANT_ALG_HMAC)
#if defined(PSA_WANT_ALG_MD5)
		{
			.name = "HmacMD5",
			.algorithm = PSA_ALG_HMAC(PSA_ALG_MD5),
			.description = {
				.mac_length = PSA_HASH_LENGTH(PSA_ALG_MD5)
			}
		},
#endif // PSA_WANT_ALG_MD5
#if defined(PSA_WANT_ALG_SHA_1)
		{
			.name = "HmacSHA1",
			.algorithm = PSA_ALG_HMAC(PSA_ALG_SHA_1),
			.description = {
				.mac_length = PSA_HASH_LENGTH(PSA_ALG_SHA_1)
			}
		},
#endif // PSA_WANT_ALG_SHA_1
#if defined(PSA_WANT_ALG_SHA_256)
		{
			.name = "HmacSHA256",
			.algorithm = PSA_ALG_HMAC(PSA_ALG_SHA_256),
			.description = {
				.mac_length = PSA_HASH_LENGTH(PSA_ALG_SHA_256)
			}
		}
#endif // PSA_WANT_ALG_SHA_256
#endif // PSA_WANT_ALG_HMAC
	};
	int32_t return_code = LLCRYPTO_ERROR;
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	/* Reject an algorithm_desc array too small to hold the description before the memcpy writes into it. */
	if (microej_sni_get_array_length(algorithm_desc) < (int32_t)sizeof(LLCRYPTO_MAC_algorithm_desc)) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_BUFFER_TOO_SMALL);
		return_code = SNI_IGNORED_RETURNED_VALUE;
	} else {
		int32_t nb_algorithms = sizeof(available_mac_algorithms) / sizeof(LLCRYPTO_MAC_algorithm);
		const LLCRYPTO_MAC_algorithm *algorithm = &available_mac_algorithms[0];

		while (--nb_algorithms >= 0) {
			if (strcmp((char *)algorithm_name, (algorithm->name)) == 0) {
				(void)memcpy(algorithm_desc, &(algorithm->description), sizeof(LLCRYPTO_MAC_algorithm_desc));
				break;
			}
			algorithm++;
		}

		if (0 <= nb_algorithms) {
			return_code = (intptr_t)algorithm;
		}
	}
	LLCRYPTO_PROFILE_END();
	return return_code;
}

int32_t LLCRYPTO_MAC_IMPL_init(int32_t algorithm_id, uint8_t *key, int32_t key_length) {
	int32_t return_code = LLCRYPTO_SUCCESS;
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	LLCRYPTO_MAC_ctx *native_id = NULL;
	const LLCRYPTO_MAC_algorithm *algorithm = (const LLCRYPTO_MAC_algorithm *)algorithm_id;

	return_code = llcrypto_mac_init(&native_id, algorithm->algorithm, key, key_length);

	if (LLCRYPTO_SUCCESS != return_code) {
		VEEPORT_LOGGER_LOG_DBG("init failed: %d", return_code);
		/* llcrypto_mac_init returned the error code to throw (LLCRYPTO_ERROR_OOM on allocation
		 * failure). */
		llcrypto_throw_error(return_code, LLCRYPTO_MSG_INIT_FAILED);
	} else {
		/* register SNI native resource */
		if (SNI_OK != SNI_registerResource(native_id, llcrypto_mac_close, NULL)) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_SNI_REGISTER_FAILED);
			llcrypto_mac_close(native_id);
			return_code = LLCRYPTO_ERROR;
		} else {
			return_code = (intptr_t)native_id;
		}
	}
	LLCRYPTO_PROFILE_END();
	return return_code;
}

void LLCRYPTO_MAC_IMPL_update(int32_t algorithm_id, int32_t native_id, uint8_t *buffer, int32_t buffer_offset,
                              int32_t buffer_length) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	LLCRYPTO_UNUSED_PARAM(algorithm_id);
	LLCRYPTO_MAC_ctx *ctx = (LLCRYPTO_MAC_ctx *)native_id;
	int32_t buffer_array_length = microej_sni_get_array_length(buffer);
	if (!llcrypto_bounds_ok(buffer_array_length, buffer_offset, buffer_length)) {
		VEEPORT_LOGGER_LOG_DBG("invalid buffer_offset %d / buffer_length %d (array %d)",
		                       buffer_offset, buffer_length, buffer_array_length);
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_OFFSET_LENGTH);
	} else {
		int32_t return_code = llcrypto_mac_update(ctx, &buffer[buffer_offset], buffer_length);
		if (LLCRYPTO_SUCCESS != return_code) {
			VEEPORT_LOGGER_LOG_DBG("update failed: %d", return_code);
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_UPDATE_FAILED);
		}
	}
	LLCRYPTO_PROFILE_END();
}

void LLCRYPTO_MAC_IMPL_do_final(int32_t algorithm_id, int32_t native_id, uint8_t *out, int32_t out_offset,
                                int32_t out_length) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	LLCRYPTO_UNUSED_PARAM(algorithm_id);
	LLCRYPTO_MAC_ctx *ctx = (LLCRYPTO_MAC_ctx *)native_id;
	int32_t out_array_length = microej_sni_get_array_length(out);
	if (!llcrypto_bounds_ok(out_array_length, out_offset, out_length)) {
		VEEPORT_LOGGER_LOG_DBG("invalid out_offset %d / out_length %d (array %d)",
		                       out_offset, out_length, out_array_length);
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_OFFSET_LENGTH);
	} else {
		int32_t return_code = llcrypto_mac_dofinal(ctx, &out[out_offset], out_length);
		if (LLCRYPTO_SUCCESS != return_code) {
			VEEPORT_LOGGER_LOG_DBG("do_final failed: %d", return_code);
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "do_final failed");
		}
	}
	LLCRYPTO_PROFILE_END();
}

void LLCRYPTO_MAC_IMPL_reset(int32_t algorithm_id, int32_t native_id) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	const LLCRYPTO_MAC_algorithm *algorithm = (const LLCRYPTO_MAC_algorithm *)algorithm_id;
	LLCRYPTO_MAC_ctx *ctx = (LLCRYPTO_MAC_ctx *)native_id;
	int32_t return_code = llcrypto_mac_reset(ctx, algorithm->algorithm);

	if (LLCRYPTO_SUCCESS != return_code) {
		VEEPORT_LOGGER_LOG_DBG("reset failed: %d", return_code);
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "reset failed");
	}
	LLCRYPTO_PROFILE_END();
}

void LLCRYPTO_MAC_IMPL_close(int32_t algorithm_id, int32_t native_id) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	LLCRYPTO_UNUSED_PARAM(algorithm_id);
	LLCRYPTO_MAC_ctx *ctx = (LLCRYPTO_MAC_ctx *)native_id;

	llcrypto_unregister_and_close(ctx, llcrypto_mac_close);
	LLCRYPTO_PROFILE_END();
}

int32_t LLCRYPTO_MAC_IMPL_get_close_id(int32_t algorithm_id) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_UNUSED_PARAM(algorithm_id);
	return (intptr_t)llcrypto_mac_close;
}
