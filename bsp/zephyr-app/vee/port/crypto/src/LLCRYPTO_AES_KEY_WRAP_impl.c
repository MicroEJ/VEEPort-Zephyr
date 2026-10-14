/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <LLCRYPTO_AES_KEY_WRAP_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "sni_extras.h"
#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_utils.h"
#include "LLCRYPTO_key.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

/*
 * AES Key Wrap (RFC 3394) relies on the Oberon-specific PSA APIs (oberon_psa_wrap_key /
 * oberon_psa_unwrap_key) and PSA_ALG_AES_KW, which are NOT part of the TF-PSA-Crypto PSA API.
 * Each native is therefore guarded with CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU:
 *   - defined (e.g. nRF Connect SDK): the full Oberon-backed implementation.
 *   - otherwise (generic Zephyr / Mbed TLS 4.1): the operation is reported as unsupported so the
 *     build does not depend on any Oberon-only symbol.
 */

typedef struct {
	psa_key_id_t key_id;
	llcrypto_key_close_with_psa key_close;
} LLCRYPTO_AES_KEY_WRAP_ctx;

static void llcrypto_aes_key_wrap_close(void *ctx) {
	VEEPORT_LOGGER_LOG_DBG("native_id %p", ctx);
	LLCRYPTO_AES_KEY_WRAP_ctx *aes_key_wrap_ctx = ctx;

	if (NULL != aes_key_wrap_ctx) {
		if (NULL != aes_key_wrap_ctx->key_close) {
			aes_key_wrap_ctx->key_close(aes_key_wrap_ctx->key_id);
		}
		free(aes_key_wrap_ctx);
	}
}

#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
/* Resolves the wrappable PSA key type for a key-type string. Pass type == NULL to validate the
 * string only (the caller does not need the resolved type, as in the wrap path). */
static int32_t llcrypto_wrappable_psa_key_type_from_string(const char *s, psa_key_type_t *type) {
	psa_key_type_t resolved = PSA_KEY_TYPE_NONE;
	int32_t return_code = LLCRYPTO_SUCCESS;

	if (strcmp(s, "AES") == 0) {
		resolved = PSA_KEY_TYPE_AES;
	} else if (strcmp(s, "CHACHA20") == 0) {
		resolved = PSA_KEY_TYPE_CHACHA20;
	} else if (strcmp(s, "HMAC") == 0) {
		resolved = PSA_KEY_TYPE_HMAC;
#if defined(PSA_WANT_KEY_TYPE_DES)
	} else if (strcmp(s, "DES") == 0) {
		resolved = PSA_KEY_TYPE_DES;
#endif
	} else {
		return_code = LLCRYPTO_ERROR;
	}

	if ((LLCRYPTO_SUCCESS == return_code) && (NULL != type)) {
		*type = resolved;
	}

	return return_code;
}

#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */

int32_t LLCRYPTO_AES_KEY_WRAP_IMPL_get_transformation(uint8_t *transformation) {
	VEEPORT_LOGGER_LOG_DBG("transformation_name %s", transformation);

	/**
	 * There is no specific configuration for this cipher (ex: blksize is always 128bits).
	 * So we just return that we support the transformation or not.
	 */
	int32_t return_code = LLCRYPTO_SUCCESS;
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
	if (0 != strcmp("AESWrap", (char *)transformation)) {
		return_code = LLCRYPTO_ERROR;
	}
#else /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	/* No key-wrap PSA API without the Oberon driver: report the transformation as unsupported. */
	LLCRYPTO_UNUSED_PARAM(transformation);
	return_code = LLCRYPTO_ERROR;
#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	VEEPORT_LOGGER_LOG_DBG("return_code %d", return_code);
	return return_code;
}

int32_t LLCRYPTO_AES_KEY_WRAP_IMPL_init(int32_t transformation_id, uint8_t is_decrypting, int32_t secret_key_id) {
	LLCRYPTO_UNUSED_PARAM(transformation_id);
	VEEPORT_LOGGER_LOG_DBG("is_decrypting %d", is_decrypting);
	LLCRYPTO_PROFILE_START();
	int32_t return_code = LLCRYPTO_SUCCESS;
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
	struct llcrypto_key *key = (struct llcrypto_key *)secret_key_id;

	LLCRYPTO_AES_KEY_WRAP_ctx *aes_key_wrap_ctx = calloc(1, sizeof(LLCRYPTO_AES_KEY_WRAP_ctx));
	if (NULL == aes_key_wrap_ctx) {
		VEEPORT_ERRORS_throw_runtime_exception(LLCRYPTO_ERROR_OOM);
		return_code = LLCRYPTO_ERROR;
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		/* Request only the usage matching the operation direction. The KEK must be provisioned with the
		 * matching flag (PSA_KEY_USAGE_WRAP to encrypt, PSA_KEY_USAGE_UNWRAP to decrypt); a KEK provisioned
		 * with both still matches a single-flag request (subset). */
		psa_key_usage_t usage = (0 != is_decrypting) ? PSA_KEY_USAGE_UNWRAP : PSA_KEY_USAGE_WRAP;
		int32_t key_rc = llcrypto_key_get_psa_id(key, usage, PSA_ALG_AES_KW, &aes_key_wrap_ctx->key_id,
		                                         &aes_key_wrap_ctx->key_close);
		if (LLCRYPTO_SUCCESS != key_rc) {
			llcrypto_throw_error(key_rc, "failed to load the Key Encryption Key");
			return_code = LLCRYPTO_ERROR;
		}
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		if (SNI_OK != SNI_registerResource(aes_key_wrap_ctx, llcrypto_aes_key_wrap_close, NULL)) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_SNI_REGISTER_FAILED);
			return_code = LLCRYPTO_ERROR;
		}
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		return_code = (intptr_t)(aes_key_wrap_ctx);
	} else {
		llcrypto_aes_key_wrap_close(aes_key_wrap_ctx);
	}
	VEEPORT_LOGGER_LOG_DBG("returned %d", return_code);
#else /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_UNUSED_PARAM(is_decrypting);
	LLCRYPTO_UNUSED_PARAM(secret_key_id);
	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_REQUIRES_OBERON_PSA);
	return_code = LLCRYPTO_ERROR;
#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_PROFILE_END();
	return return_code;
}

int32_t LLCRYPTO_AES_KEY_WRAP_IMPL_wrap(int32_t transformation_id, int32_t native_id, int32_t key_id, uint8_t *output,
                                        int32_t output_offset, int32_t output_length, uint8_t *algorithm) {
	LLCRYPTO_UNUSED_PARAM(transformation_id);
	/* output_length is the plaintext key size; the wrapped output needs key size + 8 (RFC 3394), so the
	 * capacity is taken from the output array, not from output_length. */
	LLCRYPTO_UNUSED_PARAM(output_length);
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	int32_t return_code = LLCRYPTO_SUCCESS;
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
	LLCRYPTO_AES_KEY_WRAP_ctx *aes_key_wrap_ctx = (LLCRYPTO_AES_KEY_WRAP_ctx *)native_id;
	struct llcrypto_key *key = (struct llcrypto_key *)key_id;

	/* The wrapped key type is not needed here (oberon_psa_wrap_key derives it from the key id);
	 * validate the algorithm string only. */
	if (LLCRYPTO_SUCCESS != llcrypto_wrappable_psa_key_type_from_string((const char *)algorithm, NULL)) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_ALGORITHM_NOT_SUPPORTED);
		return_code = LLCRYPTO_ERROR;
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		int32_t output_array_length = microej_sni_get_array_length(output);
		if (!llcrypto_offset_ok(output_array_length, output_offset)) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_OFFSET_LENGTH);
			return_code = LLCRYPTO_ERROR;
		}
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		llcrypto_key_close_with_psa key_close = NULL;
		psa_key_id_t key_id_to_wrap = PSA_KEY_ID_NULL;
		int32_t key_rc = llcrypto_key_get_psa_id(key, PSA_KEY_USAGE_EXPORT, PSA_ALG_AES_KW, &key_id_to_wrap,
		                                         &key_close);
		if (LLCRYPTO_SUCCESS == key_rc) {
			/* Calls the Oberon PSA backend directly; the standard PSA key-wrap API
			 * (psa_key_export_wrapped) is not used by this port. */
			size_t wrapped_length = 0;
			psa_status_t status = oberon_psa_wrap_key(aes_key_wrap_ctx->key_id,
			                                          PSA_ALG_AES_KW,
			                                          key_id_to_wrap,
			                                          output + output_offset,
			                                          microej_sni_get_array_length(output) - output_offset,
			                                          &wrapped_length);

			key_close(key_id_to_wrap);
			if (PSA_SUCCESS != status) {
				VEEPORT_LOGGER_LOG_DBG("wrap failed: %d", (int)status);
				VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "wrap failed");
				return_code = LLCRYPTO_ERROR;
			} else {
				return_code = (int32_t)wrapped_length;
			}
		} else {
			llcrypto_throw_error(key_rc, LLCRYPTO_MSG_GET_PSA_KEY_FAILED);
			return_code = LLCRYPTO_ERROR;
		}
	}
#else /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_UNUSED_PARAM(native_id);
	LLCRYPTO_UNUSED_PARAM(key_id);
	LLCRYPTO_UNUSED_PARAM(output);
	LLCRYPTO_UNUSED_PARAM(output_offset);
	LLCRYPTO_UNUSED_PARAM(algorithm);
	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_REQUIRES_OBERON_PSA);
	return_code = LLCRYPTO_ERROR;
#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_PROFILE_END();
	return return_code;
}

int32_t LLCRYPTO_AES_KEY_WRAP_IMPL_unwrap(int32_t transformation_id, int32_t native_id, uint8_t *buffer,
                                          int32_t buffer_offset, int32_t buffer_length, uint8_t *algorithm) {
	LLCRYPTO_UNUSED_PARAM(transformation_id);
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	int32_t return_code = LLCRYPTO_SUCCESS;
#if defined(CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU)
	LLCRYPTO_AES_KEY_WRAP_ctx *aes_key_wrap_ctx = (LLCRYPTO_AES_KEY_WRAP_ctx *)native_id;
	psa_key_id_t unwrapped_key_id = PSA_KEY_ID_NULL;
	psa_key_type_t key_type = PSA_KEY_TYPE_NONE;

	if (LLCRYPTO_SUCCESS != llcrypto_wrappable_psa_key_type_from_string((const char *)algorithm, &key_type)) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_ALGORITHM_NOT_SUPPORTED);
		return_code = LLCRYPTO_ERROR;
	}

	/*
	 * A valid RFC 3394 wrapped blob is the integrity register (A) plus key material, so a length
	 * <= LLCRYPTO_RFC3394_INTEGRITY_REGISTER_BYTES cannot contain a key. Reject it here: the key-bits
	 * estimate below subtracts the integrity register from buffer_length, and that subtraction is
	 * evaluated in unsigned arithmetic, so a shorter buffer would underflow to a bogus key size.
	 * The signed comparison also catches a negative buffer_length.
	 */
	if ((LLCRYPTO_SUCCESS == return_code) && (buffer_length <= (int32_t)LLCRYPTO_RFC3394_INTEGRITY_REGISTER_BYTES)) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_BUFFER_TOO_SMALL);
		return_code = LLCRYPTO_ERROR;
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		int32_t buffer_array_length = microej_sni_get_array_length(buffer);
		if (!llcrypto_bounds_ok(buffer_array_length, buffer_offset, buffer_length)) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_OFFSET_LENGTH);
			return_code = LLCRYPTO_ERROR;
		}
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		psa_key_attributes_t unwrapped_attr = PSA_KEY_ATTRIBUTES_INIT;
		psa_set_key_type(&unwrapped_attr, key_type);
		/* Estimate the unwrapped key size by removing the RFC 3394 integrity register (A) overhead. */
		psa_set_key_bits(&unwrapped_attr,
		                 (buffer_length - LLCRYPTO_RFC3394_INTEGRITY_REGISTER_BYTES) * LLCRYPTO_BITS_PER_BYTE);
		/**
		 * At this point we cannot know the usage of the key (encrypt/decrypt?).
		 * So we will just set the EXPORT usage
		 */
		psa_set_key_usage_flags(&unwrapped_attr, PSA_KEY_USAGE_EXPORT);
		/**
		 * Similarly, the unwrapped key can be used for any PSA_ALG_*
		 */
		psa_set_key_algorithm(&unwrapped_attr, PSA_ALG_NONE);

		/* Calls the Oberon PSA backend directly; the standard PSA key-wrap API
		 * (psa_import_wrapped_key) is not used by this port. */
		psa_status_t status = oberon_psa_unwrap_key(&unwrapped_attr,
		                                            aes_key_wrap_ctx->key_id,
		                                            PSA_ALG_AES_KW,
		                                            buffer + buffer_offset,
		                                            buffer_length,
		                                            &unwrapped_key_id);
		VEEPORT_LOGGER_LOG_DBG("Key unwrapped into new Key ID 0x%08x", unwrapped_key_id);

		if (PSA_SUCCESS != status) {
			VEEPORT_LOGGER_LOG_DBG("unwrap failed: %d", (int)status);
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "unwrap failed");
			return_code = LLCRYPTO_ERROR;
		} else {
			struct llcrypto_key *key = llcrypto_key_from_psa_id(unwrapped_key_id);
			if (NULL == key) {
				psa_status_t destroy_status = psa_destroy_key(unwrapped_key_id);
				if (PSA_SUCCESS != destroy_status) {
					VEEPORT_LOGGER_LOG_WRN("psa_destroy_key failed: %d", destroy_status);
				}
				VEEPORT_ERRORS_throw_runtime_exception(LLCRYPTO_ERROR_OOM);
				return_code = LLCRYPTO_ERROR;
			} else {
				return_code = (intptr_t)key;
			}
		}
	}
#else /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_UNUSED_PARAM(native_id);
	LLCRYPTO_UNUSED_PARAM(buffer);
	LLCRYPTO_UNUSED_PARAM(buffer_offset);
	LLCRYPTO_UNUSED_PARAM(buffer_length);
	LLCRYPTO_UNUSED_PARAM(algorithm);
	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_REQUIRES_OBERON_PSA);
	return_code = LLCRYPTO_ERROR;
#endif /* CONFIG_MICROEJ_CRYPTO_NRF54L15_KMU */
	LLCRYPTO_PROFILE_END();
	return return_code;
}

void LLCRYPTO_AES_KEY_WRAP_IMPL_close(int32_t transformation_id, int32_t native_id) {
	LLCRYPTO_UNUSED_PARAM(transformation_id);
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	LLCRYPTO_AES_KEY_WRAP_ctx *aes_key_wrap_ctx = (LLCRYPTO_AES_KEY_WRAP_ctx *)native_id;

	llcrypto_unregister_and_close(aes_key_wrap_ctx, llcrypto_aes_key_wrap_close);
	LLCRYPTO_PROFILE_END();
}

int32_t LLCRYPTO_AES_KEY_WRAP_IMPL_get_close_id(int32_t transformation_id) {
	LLCRYPTO_UNUSED_PARAM(transformation_id);
	VEEPORT_LOGGER_LOG_DBG("");
	return (intptr_t)llcrypto_aes_key_wrap_close;
}
