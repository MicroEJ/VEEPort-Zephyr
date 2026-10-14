/*
 * Copyright 2021-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <mbedtls/psa_util.h>
#include <LLCRYPTO_SIG_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_key.h"
#include "LLCRYPTO_utils.h"
#include "sni_extras.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

typedef struct {
	const char *name;
	psa_algorithm_t algorithm;
	const char *digest_name;
	const char *oid;
} LLCRYPTO_SIG_algorithm;

/**
 * @brief Find a signature algorithm by its Java name.
 *
 * @param name the null-terminated algorithm name.
 * @return the algorithm, or NULL when it is not supported by this build.
 */
static const LLCRYPTO_SIG_algorithm *llcrypto_sig_find_algorithm(const char *name) {
	const LLCRYPTO_SIG_algorithm *found = NULL;
#if defined(PSA_WANT_ALG_RSA_PKCS1V15_SIGN) || defined(PSA_WANT_ALG_ECDSA)
	/* Signature algorithms, each present only when its PSA algorithm is enabled, so that a board without it reports
	 * the algorithm as not supported instead of failing at the first use. */
	static const LLCRYPTO_SIG_algorithm available_sig_algorithms[] = {
	#if defined(PSA_WANT_ALG_RSA_PKCS1V15_SIGN)
		{
			.name = "SHA256withRSA",
			.algorithm = PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256),
			.digest_name = "SHA-256",
			.oid = "1.2.840.113549.1.1.11",
		},
	#endif /* PSA_WANT_ALG_RSA_PKCS1V15_SIGN */
	#if defined(PSA_WANT_ALG_ECDSA)
		{
			.name = "SHA256withECDSA",
			.algorithm = PSA_ALG_ECDSA(PSA_ALG_SHA_256),
			.digest_name = "SHA-256",
			.oid = "1.2.840.10045.4.3.2",
		},
	#endif /* PSA_WANT_ALG_ECDSA */
	};
	for (size_t i = 0; i < (sizeof(available_sig_algorithms) / sizeof(available_sig_algorithms[0])); i++) {
		if (0 == strcmp(name, available_sig_algorithms[i].name)) {
			found = &available_sig_algorithms[i];
			break;
		}
	}
#else
	LLCRYPTO_UNUSED_PARAM(name);
	VEEPORT_LOGGER_LOG_DBG("no signature algorithm: PSA_WANT_ALG_RSA_PKCS1V15_SIGN and PSA_WANT_ALG_ECDSA not set");
#endif /* PSA_WANT_ALG_RSA_PKCS1V15_SIGN || PSA_WANT_ALG_ECDSA */
	return found;
}

int32_t LLCRYPTO_SIG_IMPL_get_algorithm_description(uint8_t *algorithm_name, uint8_t *digest_algorithm_name,
                                                    int32_t digest_algorithm_name_length) {
	int32_t return_code = LLCRYPTO_ERROR;
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();

	const LLCRYPTO_SIG_algorithm *algorithm = llcrypto_sig_find_algorithm((const char *)algorithm_name);
	if (NULL != algorithm) {
		/* Defense-in-depth: bound the claimed length against the actual output array before strncpy,
		 * which pads to the full length and would otherwise write past an undersized array. */
		if ((digest_algorithm_name_length > 0) &&
		    (digest_algorithm_name_length <= microej_sni_get_array_length(digest_algorithm_name))) {
			(void)strncpy((char *)digest_algorithm_name, algorithm->digest_name, digest_algorithm_name_length);
			/* strncpy result may not be null-terminated. */
			digest_algorithm_name[digest_algorithm_name_length - 1] = '\0';
			return_code = (intptr_t)algorithm;
		}
	}

	LLCRYPTO_PROFILE_END();
	return return_code;
}

void LLCRYPTO_SIG_IMPL_get_algorithm_oid(uint8_t *algorithm_name, uint8_t *oid, int32_t oid_length) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();

	const LLCRYPTO_SIG_algorithm *algorithm = llcrypto_sig_find_algorithm((const char *)algorithm_name);
	if (NULL != algorithm) {
		int32_t length = strlen(algorithm->oid);
		/* Defense-in-depth: the caller-claimed oid_length must not exceed the actual output array. */
		if (oid_length > microej_sni_get_array_length(oid)) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_LENGTH);
		} else if ((length + 1) > oid_length) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_BUFFER_TOO_SMALL);
		} else {
			/* length + 1 fits within oid_length (checked above), which fits the output array.
			 * Same bounded-copy idiom as the other *_get_*_name/description natives: strncpy stops
			 * at the source NUL (no over-read, no garbage tail) and the forced terminator guarantees
			 * termination. The runtime buffer-size bound + explicit NUL do not trip
			 * -Wstringop-truncation, unlike a strlen-sized bound. */
			(void)strncpy((char *)oid, algorithm->oid, oid_length);
			/* strncpy result may not be null-terminated. */
			oid[oid_length - 1] = '\0';
		}
	} else {
		/* Algorithm not supported. */
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_ALGORITHM_NOT_SUPPORTED);
	}
	LLCRYPTO_PROFILE_END();
}

uint8_t LLCRYPTO_SIG_IMPL_verify(int32_t algorithm_id, uint8_t *signature, int32_t signature_length,
                                 int32_t public_key_id,
                                 uint8_t *digest, int32_t digest_length) {
	VEEPORT_LOGGER_LOG_DBG("");

	uint8_t result = JFALSE;
	LLCRYPTO_SIG_algorithm *algorithm = (LLCRYPTO_SIG_algorithm *)algorithm_id;
	struct llcrypto_key *key = (struct llcrypto_key *)public_key_id;

	/* obtain a psa_key_id_t from the abstract `key` (only after validating caller-supplied lengths) */
	llcrypto_key_close_with_psa key_close = NULL;
	psa_key_id_t psa_id = PSA_KEY_ID_NULL;
	if ((signature_length < 0) || (signature_length > microej_sni_get_array_length(signature)) ||
	    (digest_length < 0) || (digest_length > microej_sni_get_array_length(digest))) {
		VEEPORT_LOGGER_LOG_DBG("invalid signature_length %d / digest_length %d",
		                       signature_length, digest_length);
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_LENGTH);
		return SNI_IGNORED_RETURNED_VALUE;
	}

	LLCRYPTO_PROFILE_START();

	int32_t key_rc = llcrypto_key_get_psa_id(key, PSA_KEY_USAGE_VERIFY_HASH, algorithm->algorithm, &psa_id,
	                                         &key_close);
	if (LLCRYPTO_SUCCESS == key_rc) {
		/* a pointer to a psa compatible signature format */
		uint8_t *psa_sign = NULL;
		size_t psa_sign_len = 0;
		/* use a local buffer for signature converted to a psa compatible format */
#if defined(PSA_HAVE_ALG_SOME_ECDSA)
		uint8_t sign_buffer[PSA_SIGNATURE_MAX_SIZE];
#endif /* PSA_HAVE_ALG_SOME_ECDSA */
		psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;

		if (PSA_SUCCESS == psa_get_key_attributes(psa_id, &attrs)) {
			psa_algorithm_t alg = psa_get_key_algorithm(&attrs);
			bool skip_verify = false;

			if (PSA_ALG_IS_ECDSA(alg)) {
#if defined(PSA_HAVE_ALG_SOME_ECDSA)
				/* PSA processes ECDSA signature as raw buffer, while Java specification uses a DER ASN.1
				 * representation.
				 * Convert the signature so that it can be verified by PSA. */
				psa_sign = sign_buffer;
				int conv_rc = mbedtls_ecdsa_der_to_raw(psa_get_key_bits(&attrs), signature, signature_length,
				                                       psa_sign, PSA_SIGNATURE_MAX_SIZE, &psa_sign_len);

				if (LLCRYPTO_MBEDTLS_SUCCESS != conv_rc) {
					VEEPORT_LOGGER_LOG_DBG("mbedtls_ecdsa_der_to_raw failed: %d", conv_rc);
					/* Malformed DER treated as invalid signature — result stays JFALSE */
					skip_verify = true;
				}
#else
				/* ECDSA unavailable in this build (PSA_HAVE_ALG_SOME_ECDSA unset): no DER->raw
				 * converter, so the signature cannot be checked. Fail closed as invalid. */
				VEEPORT_LOGGER_LOG_DBG("ECDSA disabled: PSA_HAVE_ALG_SOME_ECDSA not set");
				skip_verify = true;
#endif /* PSA_HAVE_ALG_SOME_ECDSA */
			} else {
				/* Treat any other kind of signature as compatible with PSA. */
				psa_sign = signature;
				psa_sign_len = signature_length;
			}

			if (!skip_verify) {
				psa_status_t status = psa_verify_hash(psa_id, algorithm->algorithm, digest, digest_length,
				                                      psa_sign, psa_sign_len);
				switch (status) {
				case PSA_SUCCESS:
					result = JTRUE;
					break;
				case PSA_ERROR_INVALID_SIGNATURE:
					result = JFALSE;
					break;
				default:
				{
					VEEPORT_LOGGER_LOG_DBG("verify error (status %d)", (int)status);
					VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "verify error");
					result = SNI_IGNORED_RETURNED_VALUE;
					break;
				}
				}
			}
		} else {
			VEEPORT_LOGGER_LOG_DBG("psa_get_key_attributes failed");
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "psa_get_key_attributes failed");
			result = SNI_IGNORED_RETURNED_VALUE;
		}
		/* Reset key attributes and free any allocated resources. */
		psa_reset_key_attributes(&attrs);

		psa_status_t close_rc = key_close(psa_id);
		if (PSA_SUCCESS != close_rc) {
			VEEPORT_LOGGER_LOG_WRN("key_close failed: %d", (int)close_rc);
		}
	} else {
		llcrypto_throw_error(key_rc, "Failed to get PSA key identifier from public key");
		result = SNI_IGNORED_RETURNED_VALUE;
	}

	LLCRYPTO_PROFILE_END();
	return result;
}

int32_t LLCRYPTO_SIG_IMPL_sign(int32_t algorithm_id, uint8_t *signature, int32_t signature_length,
                               int32_t private_key_id,
                               uint8_t *digest, int32_t digest_length) {
	VEEPORT_LOGGER_LOG_DBG("");

	size_t signature_output_length = SNI_IGNORED_RETURNED_VALUE;
	LLCRYPTO_SIG_algorithm *algorithm = (LLCRYPTO_SIG_algorithm *)algorithm_id;
	struct llcrypto_key *key = (struct llcrypto_key *)private_key_id;

	/* obtain a psa_key_id_t from the abstract `key` (only after validating caller-supplied lengths) */
	llcrypto_key_close_with_psa key_close = NULL;
	psa_key_id_t psa_id = PSA_KEY_ID_NULL;
	if ((signature_length < 0) || (signature_length > microej_sni_get_array_length(signature)) ||
	    (digest_length < 0) || (digest_length > microej_sni_get_array_length(digest))) {
		VEEPORT_LOGGER_LOG_DBG("invalid signature_length %d / digest_length %d",
		                       signature_length, digest_length);
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_LENGTH);
		return SNI_IGNORED_RETURNED_VALUE;
	}

	LLCRYPTO_PROFILE_START();

	int32_t key_rc = llcrypto_key_get_psa_id(key, PSA_KEY_USAGE_SIGN_HASH, algorithm->algorithm, &psa_id,
	                                         &key_close);
	if (LLCRYPTO_SUCCESS == key_rc) {
		psa_status_t status = psa_sign_hash(psa_id, algorithm->algorithm, digest, digest_length, signature,
		                                    signature_length, &signature_output_length);

		if (PSA_SUCCESS == status) {
			psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
			if (PSA_SUCCESS == psa_get_key_attributes(psa_id, &attrs)) {
				psa_algorithm_t alg = psa_get_key_algorithm(&attrs);
				if (PSA_ALG_IS_ECDSA(alg)) {
#if defined(PSA_HAVE_ALG_SOME_ECDSA)
					/* mbedtls' usage of this method shows that it's legal to use the same buffer for raw input and der
					 * output. */
					int rc = mbedtls_ecdsa_raw_to_der(psa_get_key_bits(&attrs), signature, signature_output_length,
					                                  signature, signature_length, &signature_output_length);
					if (LLCRYPTO_MBEDTLS_SUCCESS != rc) {
						/* A failure in the DER conversion can only come from a programming error, either providing too
						 * small a
						 * buffer, or an invalid signature. Throw a runtime exception on failure. */
						signature_output_length = SNI_IGNORED_RETURNED_VALUE;
						VEEPORT_LOGGER_LOG_DBG("ecdsa raw to der error: %d", rc);
						VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "ecdsa raw to der error");
					}
#else
					/* ECDSA unavailable in this build (PSA_HAVE_ALG_SOME_ECDSA unset): cannot DER-encode
					 * the raw signature. */
					VEEPORT_LOGGER_LOG_DBG("ECDSA disabled: PSA_HAVE_ALG_SOME_ECDSA not set");
					signature_output_length = SNI_IGNORED_RETURNED_VALUE;
					VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR,
					                                               LLCRYPTO_MSG_ALGORITHM_NOT_SUPPORTED);
#endif /* PSA_HAVE_ALG_SOME_ECDSA */
				} else {
					/* Other type of signature algorithm return psa signature output without further transform. */
				}
			} else {
				/* No attributes: cannot DER-encode; fail rather than return a raw signature. */
				signature_output_length = SNI_IGNORED_RETURNED_VALUE;
				VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "cannot read key attributes");
			}
			/* Reset key attributes and free any allocated resources. */
			psa_reset_key_attributes(&attrs);
		} else {
			VEEPORT_LOGGER_LOG_DBG("sign error (status %d)", (int)status);
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "sign error");
		}

		psa_status_t close_rc = key_close(psa_id);
		if (PSA_SUCCESS != close_rc) {
			VEEPORT_LOGGER_LOG_WRN("key_close failed: %d", (int)close_rc);
		}
	} else {
		llcrypto_throw_error(key_rc, "Failed to get PSA key identifier from private key");
	}

	LLCRYPTO_PROFILE_END();
	return signature_output_length;
}
