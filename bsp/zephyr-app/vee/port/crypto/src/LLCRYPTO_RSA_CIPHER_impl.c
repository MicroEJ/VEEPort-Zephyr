/*
 * Copyright 2023-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <LLCRYPTO_RSA_CIPHER_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_utils.h"
#include "sni_extras.h"
#include "LLCRYPTO_key.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

typedef struct {
	psa_key_id_t key_id;
	llcrypto_key_close_with_psa key_close;
	psa_algorithm_t algo;
} LLCRYPTO_RSA_CIPHER_ctx;

typedef struct {
	const char *name; // the name of the transformation
	LLCRYPTO_RSA_CIPHER_transformation_desc description;
} LLCRYPTO_RSA_CIPHER_transformation;

/* Map the Java padding type and OAEP hash to the PSA asymmetric-encryption algorithm.
 * Returns PSA_ALG_NONE for an unsupported combination. */
static psa_algorithm_t llcrypto_rsa_cipher_resolve_algo(int32_t padding_type, int32_t oaep_hash_algorithm) {
	psa_algorithm_t algo = PSA_ALG_NONE;

#if defined(PSA_WANT_ALG_RSA_PKCS1V15_CRYPT)
	if (PAD_PKCS1_TYPE == (uint32_t)padding_type) {
		algo = PSA_ALG_RSA_PKCS1V15_CRYPT;
	}
#endif //PSA_WANT_ALG_RSA_PKCS1V15_CRYPT
#if defined(PSA_WANT_ALG_RSA_OAEP)
	if (PAD_OAEP_MGF1_TYPE == (uint32_t)padding_type) {
		if (OAEP_HASH_SHA_1_ALGORITHM == (uint32_t)oaep_hash_algorithm) {
			algo = PSA_ALG_RSA_OAEP(PSA_ALG_SHA_1);
		} else if (OAEP_HASH_SHA_256_ALGORITHM == (uint32_t)oaep_hash_algorithm) {
			algo = PSA_ALG_RSA_OAEP(PSA_ALG_SHA_256);
		} else {
			/* Unsupported OAEP hash: leave algo as PSA_ALG_NONE. */
		}
	}
#endif //PSA_WANT_ALG_RSA_OAEP

	return algo;
}

static void llcrypto_rsa_cipher_close(void *ctx) {
	VEEPORT_LOGGER_LOG_DBG("native_id %p", ctx);
	LLCRYPTO_RSA_CIPHER_ctx *cipher_ctx = ctx;

	if (NULL != cipher_ctx) {
		if (NULL != cipher_ctx->key_close) {
			cipher_ctx->key_close(cipher_ctx->key_id);
		}
		free(cipher_ctx);
	}
}

int32_t LLCRYPTO_RSA_CIPHER_IMPL_get_transformation_description(uint8_t *transformation_name,
                                                                LLCRYPTO_RSA_CIPHER_transformation_desc *
                                                                transformation_desc) {
	VEEPORT_LOGGER_LOG_DBG("transformation_name %s", transformation_name);
	int32_t return_code = LLCRYPTO_ERROR;

	static const LLCRYPTO_RSA_CIPHER_transformation available_transformations[] = {
#if defined(PSA_WANT_ALG_RSA_PKCS1V15_CRYPT)
		{
			.name = "RSA/ECB/PKCS1Padding",
			.description = {
				.padding_type = PAD_PKCS1_TYPE,
				.oaep_hash_algorithm = OAEP_HASH_SHA_1_ALGORITHM,
			},
		},
#endif //PSA_WANT_ALG_RSA_PKCS1V15_CRYPT
#if defined(PSA_WANT_ALG_RSA_OAEP)
		{
			.name = "RSA/ECB/OAEPWithSHA-1AndMGF1Padding",
			.description = {
				.padding_type = PAD_OAEP_MGF1_TYPE,
				.oaep_hash_algorithm = OAEP_HASH_SHA_1_ALGORITHM,
			},
		},
		{
			.name = "RSA/ECB/OAEPWithSHA-256AndMGF1Padding",
			.description = {
				.padding_type = PAD_OAEP_MGF1_TYPE,
				.oaep_hash_algorithm = OAEP_HASH_SHA_256_ALGORITHM,
			},
		},
#endif //PSA_WANT_ALG_RSA_OAEP
	};

	/* Reject a transformation_desc array too small to hold the description before the memcpy writes into it. */
	if (microej_sni_get_array_length(transformation_desc) < (int32_t)sizeof(LLCRYPTO_RSA_CIPHER_transformation_desc)) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_BUFFER_TOO_SMALL);
		return_code = SNI_IGNORED_RETURNED_VALUE;
	} else {
		int32_t nb_transformations = sizeof(available_transformations) / sizeof(LLCRYPTO_RSA_CIPHER_transformation);
		const LLCRYPTO_RSA_CIPHER_transformation *transformation = &available_transformations[0];

		while (--nb_transformations >= 0) {
			if (strcmp((const char *)transformation_name, transformation->name) == 0) {
				(void)memcpy(transformation_desc, &(transformation->description),
				             sizeof(LLCRYPTO_RSA_CIPHER_transformation_desc));
				break;
			}
			transformation++;
		}

		if (0 <= nb_transformations) {
			return_code = (intptr_t)transformation;
		}
	}
	return return_code;
}

int32_t LLCRYPTO_RSA_CIPHER_IMPL_init(int32_t transformation_id, uint8_t is_decrypting, int32_t key_id,
                                      int32_t padding_type, int32_t oaep_hash_algorithm) {
	LLCRYPTO_UNUSED_PARAM(transformation_id);
	VEEPORT_LOGGER_LOG_DBG("is_decrypting %d", is_decrypting);
	LLCRYPTO_PROFILE_START();
	int32_t return_code = LLCRYPTO_SUCCESS;
	LLCRYPTO_RSA_CIPHER_ctx *cipher_ctx = NULL;
	struct llcrypto_key *key = (struct llcrypto_key *)key_id;

	psa_algorithm_t algo = llcrypto_rsa_cipher_resolve_algo(padding_type, oaep_hash_algorithm);
	if (PSA_ALG_NONE == algo) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "Unsupported RSA cipher padding");
		return_code = LLCRYPTO_ERROR;
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		cipher_ctx = calloc(1, sizeof(LLCRYPTO_RSA_CIPHER_ctx));
		if (NULL == cipher_ctx) {
			VEEPORT_ERRORS_throw_runtime_exception(LLCRYPTO_ERROR_OOM);
			return_code = LLCRYPTO_ERROR;
		}
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		psa_key_usage_t usage = (is_decrypting != (uint8_t)0) ? PSA_KEY_USAGE_DECRYPT : PSA_KEY_USAGE_ENCRYPT;
		cipher_ctx->algo = algo;
		int32_t key_rc = llcrypto_key_get_psa_id(key, usage, algo, &cipher_ctx->key_id, &cipher_ctx->key_close);
		if (LLCRYPTO_SUCCESS != key_rc) {
			llcrypto_throw_error(key_rc, LLCRYPTO_MSG_GET_PSA_KEY_FAILED);
			return_code = LLCRYPTO_ERROR;
		}
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		if (SNI_OK != SNI_registerResource(cipher_ctx, llcrypto_rsa_cipher_close, NULL)) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_SNI_REGISTER_FAILED);
			return_code = LLCRYPTO_ERROR;
		}
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		return_code = (intptr_t)cipher_ctx;
	} else {
		/* Single teardown for every failure path: close() is NULL-safe and releases the PSA key (via
		 * key_close) and the context itself. The SNI resource was either never reached or its
		 * registration failed, so there is nothing to unregister here. */
		llcrypto_rsa_cipher_close(cipher_ctx);
	}

	LLCRYPTO_PROFILE_END();
	return return_code;
}

/* Shared one-shot RSA asymmetric operation for both encrypt and decrypt: the two LLAPI entry points
 * differ only in which PSA primitive they call, so the bounds checking and result handling live here. */
static int32_t llcrypto_rsa_cipher_crypt(bool is_decrypting, int32_t native_id, uint8_t *buffer,
                                         int32_t buffer_offset, int32_t buffer_length, uint8_t *output,
                                         int32_t output_offset) {
	int32_t return_code = LLCRYPTO_ERROR;
	int32_t buffer_array_length = microej_sni_get_array_length(buffer);
	int32_t output_array_length = microej_sni_get_array_length(output);
	const LLCRYPTO_RSA_CIPHER_ctx *cipher_ctx = (const LLCRYPTO_RSA_CIPHER_ctx *)native_id;

	if (!llcrypto_bounds_ok(buffer_array_length, buffer_offset, buffer_length) ||
	    !llcrypto_offset_ok(output_array_length, output_offset)) {
		VEEPORT_LOGGER_LOG_DBG("invalid offsets/lengths");
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_OFFSET_LENGTH);
		return_code = SNI_IGNORED_RETURNED_VALUE;
	} else {
		size_t output_length = 0;
		psa_status_t status = PSA_ERROR_GENERIC_ERROR;
		/* output_array_length >= output_offset >= 0 guaranteed by the guard. Operands can be casted to size_t */
		size_t output_capacity = (size_t)output_array_length - (size_t)output_offset;
		if (is_decrypting) {
			status = psa_asymmetric_decrypt(cipher_ctx->key_id, cipher_ctx->algo, &buffer[buffer_offset],
			                                (size_t)buffer_length, NULL, 0, &output[output_offset],
			                                output_capacity, &output_length);
		} else {
			status = psa_asymmetric_encrypt(cipher_ctx->key_id, cipher_ctx->algo, &buffer[buffer_offset],
			                                (size_t)buffer_length, NULL, 0, &output[output_offset],
			                                output_capacity, &output_length);
		}
		if (PSA_SUCCESS != status) {
			VEEPORT_LOGGER_LOG_DBG("psa_asymmetric_%s failed (status %d)",
			                       (is_decrypting) ? "decrypt" : "encrypt", status);
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "crypt failed");
		} else {
			return_code = (int32_t)output_length;
		}
	}

	return return_code;
}

int32_t LLCRYPTO_RSA_CIPHER_IMPL_decrypt(int32_t transformation_id, int32_t native_id, uint8_t *buffer,
                                         int32_t buffer_offset, int32_t buffer_length, uint8_t *output,
                                         int32_t output_offset) {
	LLCRYPTO_UNUSED_PARAM(transformation_id);
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	int32_t return_code = llcrypto_rsa_cipher_crypt(true, native_id, buffer, buffer_offset, buffer_length, output,
	                                                output_offset);
	LLCRYPTO_PROFILE_END();
	return return_code;
}

int32_t LLCRYPTO_RSA_CIPHER_IMPL_encrypt(int32_t transformation_id, int32_t native_id, uint8_t *buffer,
                                         int32_t buffer_offset, int32_t buffer_length, uint8_t *output,
                                         int32_t output_offset) {
	LLCRYPTO_UNUSED_PARAM(transformation_id);
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	int32_t return_code = llcrypto_rsa_cipher_crypt(false, native_id, buffer, buffer_offset, buffer_length, output,
	                                                output_offset);
	LLCRYPTO_PROFILE_END();
	return return_code;
}

void LLCRYPTO_RSA_CIPHER_IMPL_close(int32_t transformation_id, int32_t native_id) {
	LLCRYPTO_UNUSED_PARAM(transformation_id);
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	LLCRYPTO_RSA_CIPHER_ctx *cipher_ctx = (LLCRYPTO_RSA_CIPHER_ctx *)native_id;

	llcrypto_unregister_and_close(cipher_ctx, llcrypto_rsa_cipher_close);
	LLCRYPTO_PROFILE_END();
}

int32_t LLCRYPTO_RSA_CIPHER_IMPL_get_close_id(int32_t transformation_id) {
	LLCRYPTO_UNUSED_PARAM(transformation_id);
	VEEPORT_LOGGER_LOG_DBG("");
	// cppcheck-suppress [misra-c2012-11.1, misra-c2012-11.6] // Abstract data type for SNI usage
	return (intptr_t)llcrypto_rsa_cipher_close;
}
