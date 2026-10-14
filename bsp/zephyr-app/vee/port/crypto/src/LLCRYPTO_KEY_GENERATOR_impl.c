/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdint.h>
#include <string.h>
#include <LLCRYPTO_KEY_GENERATOR_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_key.h"
#include "LLCRYPTO_utils.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

/* Algorithm ids exchanged with Java across the SNI boundary. Per the boundary rule they use a
 * fixed-width type together with #define constants (not a C enum), and are defined unconditionally
 * so the port keeps compiling under every Kconfig combination: the PSA_WANT_* guards gate only the
 * implementations, never these boundary constants. */
#define LLCRYPTO_KEY_GENERATOR_ALGO_AES      ((int32_t)0)

/* AES key sizes accepted by generate_key, in bits. */
#define LLCRYPTO_KEY_GENERATOR_AES_BITS_128  (128)
#define LLCRYPTO_KEY_GENERATOR_AES_BITS_192  (192)
#define LLCRYPTO_KEY_GENERATOR_AES_BITS_256  (256)

/**
 * @brief Checks whether a key can be generated for the given algorithm and key size.
 *
 * @param[in] algorithm_id the algorithm id (one of the LLCRYPTO_KEY_GENERATOR_ALGO_* constants).
 * @param[in] key_size     the key size in bits.
 * @return JTRUE if the pair is supported, JFALSE otherwise.
 */
static uint8_t llcrypto_key_generator_key_size_supported(int32_t algorithm_id, int32_t key_size) {
	uint8_t supported = JFALSE;
#if defined(PSA_WANT_KEY_TYPE_AES)
	if ((LLCRYPTO_KEY_GENERATOR_ALGO_AES == algorithm_id) &&
	    ((LLCRYPTO_KEY_GENERATOR_AES_BITS_128 == key_size) ||
	     (LLCRYPTO_KEY_GENERATOR_AES_BITS_192 == key_size) ||
	     (LLCRYPTO_KEY_GENERATOR_AES_BITS_256 == key_size))) {
		supported = JTRUE;
	}
#else
	LLCRYPTO_UNUSED_PARAM(algorithm_id);
	LLCRYPTO_UNUSED_PARAM(key_size);
#endif /* PSA_WANT_KEY_TYPE_AES */
	return supported;
}

int32_t LLCRYPTO_KEY_GENERATOR_IMPL_get_algorithm_id(uint8_t *algorithm_name) {
	VEEPORT_LOGGER_LOG_DBG("");
	int32_t algorithm_id = LLCRYPTO_ERROR;
#if defined(PSA_WANT_KEY_TYPE_AES)
	if (strcmp((char *)algorithm_name, "AES") == 0) {
		algorithm_id = LLCRYPTO_KEY_GENERATOR_ALGO_AES;
	}
#else
	LLCRYPTO_UNUSED_PARAM(algorithm_name);
	VEEPORT_LOGGER_LOG_DBG("AES key generation disabled (PSA_WANT_KEY_TYPE_AES)");
#endif /* PSA_WANT_KEY_TYPE_AES */

	return algorithm_id;
}

uint8_t LLCRYPTO_KEY_GENERATOR_IMPL_is_key_size_supported(int32_t algorithm_id, int32_t key_size) {
	VEEPORT_LOGGER_LOG_DBG("");
	return llcrypto_key_generator_key_size_supported(algorithm_id, key_size);
}

int32_t LLCRYPTO_KEY_GENERATOR_IMPL_generate_key(int32_t algorithm_id, int32_t key_size) {
	VEEPORT_LOGGER_LOG_DBG("");
	int32_t return_code = SNI_IGNORED_RETURNED_VALUE;

	LLCRYPTO_PROFILE_START();

	if ((uint8_t)JTRUE != llcrypto_key_generator_key_size_supported(algorithm_id, key_size)) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "Algorithm or key size not supported");
	} else {
#if defined(PSA_WANT_KEY_TYPE_AES)
		/* Generate the key inside PSA so the material never transits the C heap. The key usage is
		 * unknown at generation time, so the key only carries EXPORT usage: enough to encode it and
		 * to re-import it later with the attributes the requesting operation needs. */
		psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
		psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
		psa_set_key_bits(&attributes, (size_t)key_size);
		psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_EXPORT);
		psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_VOLATILE);

		psa_key_id_t psa_id = PSA_KEY_ID_NULL;
		psa_status_t status = psa_generate_key(&attributes, &psa_id);
		psa_reset_key_attributes(&attributes);

		if (PSA_SUCCESS != status) {
			VEEPORT_LOGGER_LOG_DBG("psa_generate_key failed: %d", status);
			llcrypto_throw_psa_error(status, LLCRYPTO_MSG_KEY_GENERATION_FAILED);
		} else {
			struct llcrypto_key *key = llcrypto_key_from_psa_id(psa_id);
			if (NULL == key) {
				psa_status_t destroy_status = psa_destroy_key(psa_id);
				if (PSA_SUCCESS != destroy_status) {
					VEEPORT_LOGGER_LOG_WRN("psa_destroy_key failed: %d", destroy_status);
				}
				VEEPORT_ERRORS_throw_runtime_exception(LLCRYPTO_ERROR_OOM);
			} else {
				return_code = (intptr_t)key;
			}
		}
#endif /* PSA_WANT_KEY_TYPE_AES */
	}

	LLCRYPTO_PROFILE_END();
	return return_code;
}
