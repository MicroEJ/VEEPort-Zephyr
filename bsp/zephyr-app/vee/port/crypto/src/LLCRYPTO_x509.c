/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/psa_util.h>
#include <psa/crypto.h>

#include "LLCRYPTO_x509.h"
#include "LLCRYPTO_configuration.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

int32_t llcrypto_x509_crt_parse(mbedtls_x509_crt *crt, const uint8_t *cert_data, size_t cert_data_length) {
	/* Try DER first: mbedtls_x509_crt_parse_der takes the exact length and is terminator-agnostic. */
	int rc = mbedtls_x509_crt_parse_der(crt, cert_data, cert_data_length);

	if (LLCRYPTO_MBEDTLS_SUCCESS != rc) {
		/* Not DER: retry PEM. mbedtls_x509_crt_parse only attempts the PEM branch when the buffer is a
		 * NUL-terminated string whose terminator is included in the length, so parse a NUL-terminated
		 * copy. Reset the context first in case the failed DER attempt left partial state. */
		mbedtls_x509_crt_free(crt);
		mbedtls_x509_crt_init(crt);

		uint8_t *pem = malloc(cert_data_length + 1U);
		if (NULL == pem) {
			rc = MBEDTLS_ERR_X509_ALLOC_FAILED;
		} else {
			(void)memcpy(pem, cert_data, cert_data_length);
			pem[cert_data_length] = (uint8_t)'\0';
			rc = mbedtls_x509_crt_parse(crt, pem, cert_data_length + 1U);
			free(pem);
		}
	}

	return rc;
}

/**
 * @brief Whether this build can compute `hash_alg`.
 *
 * {@code mbedtls_md_psa_alg_from_type()} maps every digest type, supported or not, and the legacy
 * {@code mbedtls_md_info_from_type()} probe needs MBEDTLS_MD_C, which a crypto-only build does not enable.
 * Setting up (then aborting) a PSA hash operation is the PSA-only way to ask.
 */
static bool llcrypto_x509_hash_supported(psa_algorithm_t hash_alg) {
	bool supported = false;

	/* cppcheck-suppress misra-c2012-10.4 ; mismatch is inside vendored macro PSA_ALG_IS_HASH */
	if (PSA_ALG_IS_HASH(hash_alg)) {
		psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
		supported = (PSA_SUCCESS == psa_hash_setup(&operation, hash_alg));
		psa_status_t abort_status = psa_hash_abort(&operation);
		if (PSA_SUCCESS != abort_status) {
			VEEPORT_LOGGER_LOG_DBG("psa_hash_abort failed: %d", (int)abort_status);
		}
	}

	return supported;
}

int32_t llcrypto_x509_verify_signature(const mbedtls_x509_crt *crt, psa_key_id_t issuer_key) {
	int32_t return_code = LLCRYPTO_X509_VERIFY_BAD_SIGNATURE;

	/* MbedTLS exposes no public accessor for the signature algorithm or the raw signature needed to
	 * verify a certificate against a caller-supplied key, so these MBEDTLS_PRIVATE fields are read
	 * directly. Access them through the MBEDTLS_PRIVATE() macro rather than the bare private_ name so
	 * this keeps compiling regardless of MBEDTLS_ALLOW_PRIVATE_ACCESS (a legacy opt-in being phased
	 * out). */
	mbedtls_md_type_t sig_md = crt->MBEDTLS_PRIVATE(sig_md);
	psa_algorithm_t hash_alg = mbedtls_md_psa_alg_from_type(sig_md);
	psa_algorithm_t sign_alg = PSA_ALG_NONE;
#if defined(PSA_HAVE_ALG_SOME_ECDSA)
	uint8_t raw_sign[PSA_SIGNATURE_MAX_SIZE];
#endif /* PSA_HAVE_ALG_SOME_ECDSA */

	const uint8_t *p_sign = NULL;
	size_t sign_len = 0;

	if (!llcrypto_x509_hash_supported(hash_alg)) {
		VEEPORT_LOGGER_LOG_DBG("unsupported signature digest %d", (int)sig_md);
		return_code = LLCRYPTO_X509_VERIFY_NO_SUCH_ALGORITHM;
	} else {
		switch (crt->MBEDTLS_PRIVATE(sig_pk)) {
		case MBEDTLS_PK_SIGALG_RSA_PKCS1V15:
			sign_alg = PSA_ALG_RSA_PKCS1V15_SIGN(hash_alg);

			/* For RSA PKCS#1 v1.5 the certificate signatureValue is the raw signature (a big-endian integer
			 * the size of the modulus), not an ASN.1 structure, so it is fed to psa_verify_message as-is.
			 * Only the ECDSA branch below needs a conversion, because mbedtls stores an ECDSA signature as
			 * the DER SEQUENCE { r, s } while PSA expects raw r || s. */
			p_sign = crt->MBEDTLS_PRIVATE(sig).p;
			sign_len = crt->MBEDTLS_PRIVATE(sig).len;
			break;
		case MBEDTLS_PK_SIGALG_ECDSA:
		{
#if defined(PSA_HAVE_ALG_SOME_ECDSA)
			psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
			if (PSA_SUCCESS == psa_get_key_attributes(issuer_key, &attrs)) {
				int status = mbedtls_ecdsa_der_to_raw(psa_get_key_bits(&attrs), crt->MBEDTLS_PRIVATE(sig).p,
				                                      crt->MBEDTLS_PRIVATE(sig).len, raw_sign,
				                                      PSA_SIGNATURE_MAX_SIZE, &sign_len);
				if (LLCRYPTO_MBEDTLS_SUCCESS == status) {
					// if conversion succeeded, set signature pointer to raw buffer
					sign_alg = PSA_ALG_ECDSA(hash_alg);
					p_sign = raw_sign;
				} else {
					// Failing to convert comes from an encoding error in the signature value.
					return_code = LLCRYPTO_X509_VERIFY_BAD_ENCODING;
				}
			} else {
				// The key loaded into PSA but its attributes are unreadable: the key is unusable, so
				// report it as an invalid key rather than masking it as an invalid signature.
				VEEPORT_LOGGER_LOG_DBG("psa_get_key_attributes failed for EC key");
				return_code = LLCRYPTO_X509_VERIFY_INVALID_KEY;
			}
			/* Reset key attributes and free any allocated resources. */
			psa_reset_key_attributes(&attrs);
#else
			/* ECDSA unavailable in this build (PSA_HAVE_ALG_SOME_ECDSA unset): the certificate uses an
			 * EC signature this port cannot verify. */
			VEEPORT_LOGGER_LOG_DBG("ECDSA disabled: PSA_HAVE_ALG_SOME_ECDSA not set");
			return_code = LLCRYPTO_X509_VERIFY_NO_SUCH_ALGORITHM;
#endif /* PSA_HAVE_ALG_SOME_ECDSA */
			break;
		}
		default:
			/* RSASSA-PSS (MBEDTLS_PK_SIGALG_RSA_PSS) and unknown algorithms are not verified by this port. */
			return_code = LLCRYPTO_X509_VERIFY_NO_SUCH_ALGORITHM;
			break;
		}
	}

	if ((PSA_ALG_NONE != sign_alg) && (NULL != p_sign)) {
		// check the certificate signature over its TBS bytes
		psa_status_t status = psa_verify_message(issuer_key, sign_alg, crt->tbs.p, crt->tbs.len, p_sign,
		                                         sign_len);

		if (PSA_SUCCESS == status) {
			return_code = LLCRYPTO_X509_VERIFY_OK;
		} else if (PSA_ERROR_INSUFFICIENT_MEMORY == status) {
			VEEPORT_LOGGER_LOG_DBG("psa_verify_message out of memory");
			return_code = LLCRYPTO_X509_VERIFY_OUT_OF_MEMORY;
		} else {
			VEEPORT_LOGGER_LOG_DBG("psa_verify_message failed (status %d)", (int)status);
			return_code = LLCRYPTO_X509_VERIFY_BAD_SIGNATURE;
		}
	}

	return return_code;
}
