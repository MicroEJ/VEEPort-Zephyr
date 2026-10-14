/*
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdint.h>
#include <string.h>
#include <LLCRYPTO_KEY_AGREEMENT_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_key.h"
#include "LLCRYPTO_utils.h"
#include "sni_extras.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

typedef struct {
	const char *name;
	psa_algorithm_t psa_algorithm;
} LLCRYPTO_KEY_AGREEMENT_algorithm;

int32_t LLCRYPTO_KEY_AGREEMENT_IMPL_get_algorithm_id(uint8_t *algorithm_name) {
	static const LLCRYPTO_KEY_AGREEMENT_algorithm available_algorithms[] = {
		{
			.name = "ECDH",
			.psa_algorithm = PSA_ALG_ECDH,
		},
	};
	VEEPORT_LOGGER_LOG_DBG("%s", algorithm_name);
	LLCRYPTO_PROFILE_START();

	int32_t return_code = LLCRYPTO_ERROR;

	for (size_t i = 0; i < sizeof(available_algorithms) / sizeof(available_algorithms[0]); i++) {
		if (strcmp((char *)algorithm_name, available_algorithms[i].name) == 0) {
			return_code = (intptr_t)&available_algorithms[i];
			break;
		}
	}

	LLCRYPTO_PROFILE_END();
	return return_code;
}

/**
 * @brief Compute the PSA output-buffer size required for a raw key agreement with the given private key.
 *
 * Shared by get_max_size and the buffer-too-small diagnostic in generate_secret.
 *
 * @param priv_psa_id PSA key identifier of the local private key.
 * @return The required output size in bytes, or {@code 0} if the key's attributes cannot be read.
 */
static size_t llcrypto_key_agreement_output_size(psa_key_id_t priv_psa_id) {
	size_t output_size = 0;
	psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
	if (PSA_SUCCESS == psa_get_key_attributes(priv_psa_id, &attrs)) {
		psa_key_type_t key_type = psa_get_key_type(&attrs);
		size_t key_bits = psa_get_key_bits(&attrs);
		/* cppcheck-suppress misra-c2012-10.4 ; mismatch is inside vendored macro PSA_RAW_KEY_AGREEMENT_OUTPUT_SIZE */
		output_size = PSA_RAW_KEY_AGREEMENT_OUTPUT_SIZE(key_type, key_bits);
	}
	psa_reset_key_attributes(&attrs);
	return output_size;
}

int32_t LLCRYPTO_KEY_AGREEMENT_IMPL_get_max_size(int32_t algorithm, int32_t local_private_key,
                                                 int32_t remote_public_key) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	LLCRYPTO_UNUSED_PARAM(remote_public_key);

	int32_t return_code = LLCRYPTO_ERROR;
	LLCRYPTO_KEY_AGREEMENT_algorithm *algo = (LLCRYPTO_KEY_AGREEMENT_algorithm *)algorithm;
	struct llcrypto_key *priv_key = (struct llcrypto_key *)local_private_key;

	llcrypto_key_close_with_psa key_close = NULL;
	psa_key_id_t priv_psa_id = PSA_KEY_ID_NULL;
	int32_t key_rc = llcrypto_key_get_psa_id(priv_key, PSA_KEY_USAGE_DERIVE, algo->psa_algorithm, &priv_psa_id,
	                                         &key_close);

	if (LLCRYPTO_SUCCESS == key_rc) {
		size_t output_size = llcrypto_key_agreement_output_size(priv_psa_id);
		if (output_size > 0u) {
			return_code = (int32_t)output_size;
			VEEPORT_LOGGER_LOG_DBG("max_size=%d", return_code);
		} else {
			VEEPORT_LOGGER_LOG_DBG("could not determine output size");
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR,
			                                               "Failed to determine key agreement output size");
		}
		psa_status_t close_rc = key_close(priv_psa_id);
		if (PSA_SUCCESS != close_rc) {
			VEEPORT_LOGGER_LOG_WRN("key_close failed: %d", (int)close_rc);
		}
	} else {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_GET_PSA_PRIVATE_KEY_FAILED);
	}

	LLCRYPTO_PROFILE_END();
	return return_code;
}

int32_t LLCRYPTO_KEY_AGREEMENT_IMPL_generate_secret(int32_t algorithm, int32_t local_private_key,
                                                    int32_t remote_public_key, uint8_t *secret_buffer) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();

	int32_t return_code = LLCRYPTO_ERROR;
	psa_status_t status = PSA_ERROR_GENERIC_ERROR;

	/* alias pointer type */
	LLCRYPTO_KEY_AGREEMENT_algorithm *algo = (LLCRYPTO_KEY_AGREEMENT_algorithm *)algorithm;
	struct llcrypto_key *priv_key = (struct llcrypto_key *)local_private_key;
	struct llcrypto_key *pub_key = (struct llcrypto_key *)remote_public_key;

	/* Staged cleanup: each failure `goto`s the label that releases exactly what was acquired so far.
	 * `cleanup_pub` closes the public key, `cleanup_priv`
	 * closes the private key, `end` releases nothing; each stage falls through to the next, and the
	 * success path falls through all of them. */
	llcrypto_key_close_with_psa priv_key_close = NULL;
	llcrypto_key_close_with_psa pub_key_close = NULL;

	psa_key_id_t priv_psa_id = PSA_KEY_ID_NULL;
	psa_key_id_t pub_psa_id = PSA_KEY_ID_NULL;

	/* cppcheck-suppress misra-c2012-12.1 ; precedence is inside vendored macro PSA_EXPORT_PUBLIC_KEY_MAX_SIZE */
	uint8_t peer_key_buf[PSA_EXPORT_PUBLIC_KEY_MAX_SIZE];

	/* Defense-in-depth: reject a negative output length up front, before any PSA resource is acquired
	 * (so no cleanup is needed), mirroring the length guards every other native in this module applies
	 * before casting to an unsigned (size_t) capacity. */
	int32_t secret_array_length = microej_sni_get_array_length(secret_buffer);
	if (secret_array_length < 0) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_LENGTH);
		return_code = SNI_IGNORED_RETURNED_VALUE;
		goto end;
	}

	/* Get PSA key ID for the private key with DERIVE usage */
	int32_t key_rc = llcrypto_key_get_psa_id(priv_key, PSA_KEY_USAGE_DERIVE, algo->psa_algorithm, &priv_psa_id,
	                                         &priv_key_close);
	if (LLCRYPTO_ERROR_OOM == key_rc) {
		/* No free PSA key slot: returned, not thrown, so that the Crypto library retries after a GC. */
		return_code = LLCRYPTO_ERROR_OOM;
		goto end;
	} else if (LLCRYPTO_SUCCESS != key_rc) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_GET_PSA_PRIVATE_KEY_FAILED);
		return_code = SNI_IGNORED_RETURNED_VALUE;
		goto end;
	} else {
		/* private key imported */
	}

	/* Get PSA key ID for the public key so we can export its raw bytes.
	 * Use PSA_KEY_USAGE_VERIFY_HASH because mbedtls_pk_get_psa_attributes() only accepts
	 * sign/verify/derive usages for EC keys, and VERIFY_HASH is the only one that does not
	 * require a private key. The function automatically adds EXPORT|COPY to the usage flags. */
	key_rc = llcrypto_key_get_psa_id(pub_key, PSA_KEY_USAGE_VERIFY_HASH, PSA_ALG_ECDSA(PSA_ALG_ANY_HASH),
	                                 &pub_psa_id, &pub_key_close);
	if (LLCRYPTO_ERROR_OOM == key_rc) {
		return_code = LLCRYPTO_ERROR_OOM;
		goto cleanup_priv;
	} else if (LLCRYPTO_SUCCESS != key_rc) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "Failed to get PSA key for public key");
		return_code = SNI_IGNORED_RETURNED_VALUE;
		goto cleanup_priv;
	} else {
		/* public key imported */
	}

	/* Export the peer public key to raw bytes (uncompressed EC point: 04 || x || y) */
	size_t peer_key_len = 0;
	status = psa_export_public_key(pub_psa_id, peer_key_buf, sizeof(peer_key_buf), &peer_key_len);
	if (PSA_SUCCESS != status) {
		VEEPORT_LOGGER_LOG_DBG("psa_export_public_key failed: %d", (int)status);
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_EXPORT_FAILED);
		return_code = SNI_IGNORED_RETURNED_VALUE;
		goto cleanup_pub;
	}

	size_t secret_length = 0;
	VEEPORT_LOGGER_LOG_DBG("peer_key_len=%d secret_array_length=%d",
	                       (int)peer_key_len, secret_array_length);

	/* Pass the caller's array capacity as the output size: PSA writes at most that many bytes and
	 * returns PSA_ERROR_BUFFER_TOO_SMALL if the agreement output does not fit, so no separate size
	 * check against the key is needed. */
	status = psa_raw_key_agreement(algo->psa_algorithm, priv_psa_id, peer_key_buf, peer_key_len,
	                               secret_buffer, (size_t)secret_array_length, &secret_length);
	if (PSA_SUCCESS == status) {
		return_code = (int32_t)secret_length;
		VEEPORT_LOGGER_LOG_DBG("secret_length=%d", return_code);
	} else if (PSA_ERROR_BUFFER_TOO_SMALL == status) {
		/* Buffer too small: leave return_code = LLCRYPTO_ERROR (negative) so the Java caller raises
		 * IllegalArgumentException; a bad-size buffer is a caller argument error, not a native
		 * exception. */
		return_code = LLCRYPTO_ERROR;
		VEEPORT_LOGGER_LOG_DBG("secret_buffer too small (have %d, need %d)",
		                       secret_array_length,
		                       (int)llcrypto_key_agreement_output_size(priv_psa_id));
	} else {
		VEEPORT_LOGGER_LOG_DBG("psa_raw_key_agreement failed: %d", (int)status);
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "psa_raw_key_agreement failed");
		return_code = SNI_IGNORED_RETURNED_VALUE;
	}

cleanup_pub:
	/* peer_key_buf holds only the peer's public key (exported public EC point) — not secret, so it is
	 * not zeroized. */
	status = pub_key_close(pub_psa_id);
	if (PSA_SUCCESS != status) {
		VEEPORT_LOGGER_LOG_WRN("pub_key_close failed: %d", (int)status);
	}

cleanup_priv:
	status = priv_key_close(priv_psa_id);
	if (PSA_SUCCESS != status) {
		VEEPORT_LOGGER_LOG_WRN("priv_key_close failed: %d", (int)status);
	}

end:
	LLCRYPTO_PROFILE_END();
	return return_code;
}
