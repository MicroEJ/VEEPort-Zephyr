/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdint.h>
#include <string.h>
#include <mbedtls/platform_util.h>
#include <LLCRYPTO_CONSTANTS.h>

#include "LLCRYPTO_key_psa.h"
#include "LLCRYPTO_utils.h"
#include "LLCRYPTO_configuration.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

psa_key_type_t llcrypto_psa_type_from_java_algorithm(uint8_t *algorithm) {
	psa_key_type_t type = PSA_KEY_TYPE_NONE;
	if (NULL != algorithm) {
		if (0 == strcmp("AES", (const char *)algorithm)) {
			type = PSA_KEY_TYPE_AES;
#if defined(PSA_WANT_KEY_TYPE_DES)
		} else if (0 == strcmp("DESede", (const char *)algorithm)) {
			/* Java "DESede" is 3-key Triple-DES; PSA derives single vs triple DES from the key length
			 * (8/16/24 bytes) at import time, so the key type alone is PSA_KEY_TYPE_DES. */
			type = PSA_KEY_TYPE_DES;
#endif
		} else {
			VEEPORT_LOGGER_LOG_DBG("Secret Key type not supported %s", algorithm);
		}
	}
	return type;
}

int32_t llcrypto_psa_from_mbedtls_pk(const mbedtls_pk_context *pk, psa_key_usage_t usage,
                                     psa_algorithm_t algorithm, psa_key_id_t *id) {
	int32_t return_code = LLCRYPTO_ERROR;
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	*id = PSA_KEY_ID_NULL;

	int rc = mbedtls_pk_get_psa_attributes(pk, usage, &attributes);
	if (LLCRYPTO_MBEDTLS_SUCCESS == rc) {
		psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_VOLATILE);
		if (PSA_ALG_NONE != algorithm) {
			psa_set_key_algorithm(&attributes, algorithm);
		}
		rc = mbedtls_pk_import_into_psa(pk, &attributes, id);
		if (LLCRYPTO_MBEDTLS_SUCCESS == rc) {
			return_code = LLCRYPTO_SUCCESS;
		} else {
			VEEPORT_LOGGER_LOG_DBG("mbedtls_pk_import_into_psa failed: %d", rc);
			/* The PK layer maps PSA_ERROR_INSUFFICIENT_MEMORY, which a full key store returns, to
			 * MBEDTLS_ERR_PK_ALLOC_FAILED. */
			return_code = (MBEDTLS_ERR_PK_ALLOC_FAILED == rc) ? LLCRYPTO_ERROR_OOM : LLCRYPTO_ERROR;
		}
	}

	psa_reset_key_attributes(&attributes);

	return return_code;
}

int32_t llcrypto_psa_from_secret_key(struct llcrypto_secret_key secret_key, psa_key_usage_t usage,
                                     psa_algorithm_t algorithm, psa_key_id_t *id) {
	int32_t return_code = LLCRYPTO_SUCCESS;
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	*id = PSA_KEY_ID_NULL;

	psa_set_key_type(&attributes, secret_key.type);
	psa_set_key_bits(&attributes, secret_key.length * LLCRYPTO_BITS_PER_BYTE);
	psa_set_key_usage_flags(&attributes, usage);
	psa_set_key_algorithm(&attributes, algorithm);
	psa_status_t status = psa_import_key(&attributes, secret_key.buffer, secret_key.length, id);
	if (PSA_SUCCESS != status) {
		VEEPORT_LOGGER_LOG_DBG("psa_import_key failed: %d", (int)status);
		return_code = llcrypto_sni_error_code(status);
	}

	psa_reset_key_attributes(&attributes);

	return return_code;
}

int32_t llcrypto_psa_reimport_with_attributes(psa_key_id_t src, psa_key_usage_t usage,
                                              psa_algorithm_t algorithm, psa_key_id_t *id) {
	int32_t return_code = LLCRYPTO_ERROR;
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	*id = PSA_KEY_ID_NULL;

	if (PSA_SUCCESS == psa_get_key_attributes(src, &attributes)) {
		psa_key_type_t original_type = psa_get_key_type(&attributes);
		size_t original_bits = psa_get_key_bits(&attributes);
		/* Large enough for both symmetric (AES-256: 32 bytes) and asymmetric (EC P-256 key pair: 32 bytes)
		 * exports */
		/* cppcheck-suppress misra-c2012-12.1 ; precedence is inside vendored macro PSA_EXPORT_KEY_PAIR_MAX_SIZE */
		uint8_t raw_key[PSA_EXPORT_KEY_PAIR_MAX_SIZE];
		size_t raw_key_length = 0;
		psa_status_t status = psa_export_key(src, raw_key, sizeof(raw_key), &raw_key_length);
		if (PSA_SUCCESS == status) {
			psa_reset_key_attributes(&attributes);
			psa_set_key_type(&attributes, original_type);
			psa_set_key_bits(&attributes, original_bits);
			psa_set_key_usage_flags(&attributes, usage);
			psa_set_key_algorithm(&attributes, algorithm);
			psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_VOLATILE);
			status = psa_import_key(&attributes, raw_key, raw_key_length, id);
		}
		/* Zeroize unconditionally: no ASLR on Cortex-M33/Zephyr, so stack residue is
		 * recoverable at a predictable address by a subsequent native call. */
		mbedtls_platform_zeroize(raw_key, sizeof(raw_key));

		if (PSA_SUCCESS == status) {
			VEEPORT_LOGGER_LOG_DBG("Imported Key ID 0x%08x with usage 0x%08x algo 0x%08x", *id, usage, algorithm);
			return_code = LLCRYPTO_SUCCESS;
		} else {
			VEEPORT_LOGGER_LOG_DBG("%d export/reimport failed: %d", __LINE__, (int)status);
			return_code = llcrypto_sni_error_code(status);
		}
	}

	psa_reset_key_attributes(&attributes);

	return return_code;
}
