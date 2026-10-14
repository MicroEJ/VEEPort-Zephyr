/*
 * Copyright 2023-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <mbedtls/platform_util.h>
#include <psa/crypto.h>
#include <LLCRYPTO_SECRET_KEY_FACTORY_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_key.h"
#include "LLCRYPTO_utils.h"
#include "sni_extras.h"

#if defined(PSA_WANT_ALG_PBKDF2_HMAC)
/* Maps a Java SecretKeyFactory PBKDF2 algorithm name to its PSA hash. The hash, combined into
 * PSA_ALG_PBKDF2_HMAC(hash), is returned by get_algorithm and threaded back into get_key_data by the
 * Java SPI. Each entry is gated by its digest so it disappears with the digest. */
typedef struct {
	const char *name;
	psa_algorithm_t hash;
} LLCRYPTO_SECRET_KEY_FACTORY_algorithm;

#endif // PSA_WANT_ALG_PBKDF2_HMAC

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

/* get_algorithm returns a psa_algorithm_t in an int32_t; get_key_data recovers it. Verify at build
 * time that the return type is wide enough to hold psa_algorithm_t without truncation. */
/* cppcheck-suppress misra-c2012-2.3 ; the typedef LLCRYPTO_STATIC_ASSERT generates is deliberately unused -- its only
 * role is to make its array size negative when the condition is false (portable C99 static assertion). */
LLCRYPTO_STATIC_ASSERT(sizeof(psa_algorithm_t) <= sizeof(int32_t),
                       "psa_algorithm_t does not fit in the returned int32_t type");

int32_t LLCRYPTO_SECRET_KEY_FACTORY_IMPL_get_algorithm(uint8_t *algorithm_name) {
	VEEPORT_LOGGER_LOG_DBG("");
	int32_t algorithm_id = LLCRYPTO_ERROR;

#if defined(PSA_WANT_ALG_PBKDF2_HMAC)
	static const LLCRYPTO_SECRET_KEY_FACTORY_algorithm available_algorithms[] = {
#if defined(PSA_WANT_ALG_SHA_1)
		{ "PBKDF2WithHmacSHA1", PSA_ALG_SHA_1 },
#endif
#if defined(PSA_WANT_ALG_SHA_224)
		{ "PBKDF2WithHmacSHA224", PSA_ALG_SHA_224 },
#endif
#if defined(PSA_WANT_ALG_SHA_256)
		{ "PBKDF2WithHmacSHA256", PSA_ALG_SHA_256 },
#endif
#if defined(PSA_WANT_ALG_SHA_384)
		{ "PBKDF2WithHmacSHA384", PSA_ALG_SHA_384 },
#endif
#if defined(PSA_WANT_ALG_SHA_512)
		{ "PBKDF2WithHmacSHA512", PSA_ALG_SHA_512 },
#endif
	};
	const char *name = (const char *)algorithm_name;
	size_t nb_algorithms = sizeof(available_algorithms) / sizeof(available_algorithms[0]);
	for (size_t i = 0; i < nb_algorithms; i++) {
		if (strcmp(name, available_algorithms[i].name) == 0) {
			/* Encode the full PBKDF2-HMAC(hash) PSA algorithm in the id. The value is a small positive
			 * integer, so it round-trips through the int32_t LLAPI contract and is recovered as-is in
			 * get_key_data. */
			psa_algorithm_t alg = PSA_ALG_PBKDF2_HMAC(available_algorithms[i].hash);
			algorithm_id = (int32_t)alg;
			break;
		}
	}
#else
	LLCRYPTO_UNUSED_PARAM(algorithm_name);
#endif // PSA_WANT_ALG_PBKDF2_HMAC

	return algorithm_id;
}

int32_t LLCRYPTO_SECRET_KEY_FACTORY_IMPL_get_key_data(int32_t algorithm_id, uint8_t *password, int32_t password_length,
                                                      uint8_t *salt, int32_t salt_length, int32_t iterations,
                                                      int32_t key_length) {
	VEEPORT_LOGGER_LOG_DBG("");
	int32_t native_id = SNI_IGNORED_RETURNED_VALUE;

	LLCRYPTO_PROFILE_START();

#if defined(PSA_WANT_ALG_PBKDF2_HMAC)
	if ((password_length < 0) || (password_length > microej_sni_get_array_length(password)) ||
	    (salt_length < 0) || (salt_length > microej_sni_get_array_length(salt)) ||
	    (key_length <= 0) || (((size_t)key_length % LLCRYPTO_BITS_PER_BYTE) != 0u) ||
	    (iterations <= 0)) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "Invalid PBKDF2 parameters");
	} else {
		size_t key_bytes = (size_t)key_length / LLCRYPTO_BITS_PER_BYTE;
		uint8_t *derived = malloc(key_bytes);
		if (NULL == derived) {
			VEEPORT_ERRORS_throw_runtime_exception(LLCRYPTO_ERROR_OOM);
		} else {
			psa_key_derivation_operation_t operation = PSA_KEY_DERIVATION_OPERATION_INIT;
			psa_algorithm_t alg = (psa_algorithm_t)algorithm_id;

			/* PBKDF2 requires the inputs in this order: cost (iteration count), then salt, then
			 * password (see the mbedtls soft-PBKDF2 state machine). */
			psa_status_t status = psa_key_derivation_setup(&operation, alg);
			if (PSA_SUCCESS == status) {
				status = psa_key_derivation_input_integer(&operation, PSA_KEY_DERIVATION_INPUT_COST,
				                                          (uint64_t)iterations);
			}
			if (PSA_SUCCESS == status) {
				status = psa_key_derivation_input_bytes(&operation, PSA_KEY_DERIVATION_INPUT_SALT, salt,
				                                        (size_t)salt_length);
			}
			if (PSA_SUCCESS == status) {
				status = psa_key_derivation_input_bytes(&operation, PSA_KEY_DERIVATION_INPUT_PASSWORD,
				                                        password, (size_t)password_length);
			}
			if (PSA_SUCCESS == status) {
				status = psa_key_derivation_output_bytes(&operation, derived, key_bytes);
			}
			psa_status_t abort_status = psa_key_derivation_abort(&operation);

			/*
			 * psa_key_derivation_abort can fail with PSA_ERROR_COMMUNICATION_FAILURE,
			 * PSA_ERROR_HARDWARE_FAILURE or PSA_ERROR_CORRUPTION_DETECTED. The derivation
			 * result is carried by `status` above, so the abort status can only be logged.
			 */
			if (PSA_SUCCESS != abort_status) {
				VEEPORT_LOGGER_LOG_WRN("psa_key_derivation_abort failed: %d",
				                       abort_status);
			}

			if (PSA_SUCCESS == status) {
				/* The derived bytes are returned to Java only via getEncoded() (a raw copy), so the PSA
				* key type is irrelevant: pass NULL to store a raw secret key with PSA_KEY_TYPE_NONE. */
				struct llcrypto_key *key = llcrypto_key_from_secret_key(derived, key_bytes, NULL);
				if (NULL != key) {
					native_id = (intptr_t)key;
				} else {
					VEEPORT_ERRORS_throw_runtime_exception(LLCRYPTO_ERROR_OOM);
				}
			} else {
				VEEPORT_LOGGER_LOG_DBG("PBKDF2 key derivation failed: %d", (int)status);
				llcrypto_throw_psa_error(status, "PBKDF2 key derivation failed");
			}

			mbedtls_platform_zeroize(derived, key_bytes);
			free(derived);
		}
	}
#else
	LLCRYPTO_UNUSED_PARAM(algorithm_id);
	LLCRYPTO_UNUSED_PARAM(password);
	LLCRYPTO_UNUSED_PARAM(password_length);
	LLCRYPTO_UNUSED_PARAM(salt);
	LLCRYPTO_UNUSED_PARAM(salt_length);
	LLCRYPTO_UNUSED_PARAM(iterations);
	LLCRYPTO_UNUSED_PARAM(key_length);

	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_NOT_IMPLEMENTED);
#endif // PSA_WANT_ALG_PBKDF2_HMAC

	LLCRYPTO_PROFILE_END();
	return native_id;
}

int32_t LLCRYPTO_SECRET_KEY_FACTORY_IMPL_get_close_id(int32_t algorithm_id) {
	LLCRYPTO_UNUSED_PARAM(algorithm_id);
	VEEPORT_LOGGER_LOG_DBG("");

	return (intptr_t)llcrypto_key_free;
}
