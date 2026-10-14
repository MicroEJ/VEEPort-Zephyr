/*
 * Copyright 2021-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdint.h>
#include <string.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/psa_util.h>
#include <LLCRYPTO_X509_CERT_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_key.h"
#include "LLCRYPTO_utils.h"
#include "LLCRYPTO_x509.h"
#include "sni_extras.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

int32_t LLCRYPTO_X509_CERT_IMPL_parse(int8_t *cert, int32_t off, int32_t len) {
	VEEPORT_LOGGER_LOG_DBG("");

	int32_t encoding = J_CERT_PARSE_ERROR;
	int32_t cert_array_length = microej_sni_get_array_length(cert);

	/* Defense-in-depth: bound off/len against the array before dereferencing cert + off.
	 * Return J_CERT_PARSE_ERROR (the header-documented parse-error code) so the Java caller
	 * detects the bad argument. */
	if ((off < 0) || (len < 0) || (off > (cert_array_length - len))) {
		VEEPORT_LOGGER_LOG_DBG("invalid off %d / len %d (array %d)", off, len, cert_array_length);
	} else {
		char *pcert = &((char *)cert)[off];
		const char pem_header[] = "-----BEGIN CERTIFICATE-----";
		const size_t pem_header_len = sizeof(pem_header) - 1u;

		/* Detect PEM vs DER from the content: a PEM certificate carries the "-----BEGIN CERTIFICATE-----"
		 * armor, DER is raw ASN.1. The buffer is not guaranteed to be null-terminated (net-pack hands the
		 * certificate down without a trailing '\0'), so scan within [0, len) with a bounded search rather
		 * than strstr(), which requires null termination and could over-read past the array. A previous
		 * implementation gated PEM detection on pcert[len - 1] == '\0' and so misclassified a
		 * non-null-terminated PEM certificate as DER. */
		encoding = CERT_DER_FORMAT;
		for (size_t i = 0; (i + pem_header_len) <= (size_t)len; i++) {
			if (0 == strncmp(&pcert[i], pem_header, pem_header_len)) {
				encoding = CERT_PEM_FORMAT;
				break;
			}
		}
	}

	return encoding;
}

int32_t LLCRYPTO_X509_CERT_IMPL_get_x500_principal_data(int8_t *cert_data, int32_t cert_data_length,
                                                        uint8_t *principal_data, int32_t principal_data_length,
                                                        uint8_t get_issuer) {
	VEEPORT_LOGGER_LOG_DBG("");
	mbedtls_x509_crt crt;
	int32_t len = SNI_IGNORED_RETURNED_VALUE;

	mbedtls_x509_crt_init(&crt);

	/* Defense-in-depth: bound the caller-supplied lengths before the MbedTLS parser reads cert_data
	 * and before principal_data_length is used as the copy capacity. */
	if ((cert_data_length < 0) || (cert_data_length > microej_sni_get_array_length(cert_data)) ||
	    (principal_data_length < 0) || (principal_data_length > microej_sni_get_array_length(principal_data))) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_LENGTH);
	} else if (llcrypto_x509_crt_parse(&crt, (const uint8_t *)cert_data,
	                                   (size_t)cert_data_length) == LLCRYPTO_MBEDTLS_SUCCESS) {
		const mbedtls_x509_buf *name = (get_issuer != (uint8_t)0) ? &crt.issuer_raw : &crt.subject_raw;

		if (name->len > (size_t)principal_data_length) {
			VEEPORT_LOGGER_LOG_DBG("principal_data too small (%u needed, %d given)",
			                       (unsigned int)name->len, principal_data_length);
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_BUFFER_TOO_SMALL);
		} else {
			(void)memcpy(principal_data, name->p, name->len);
			len = (int32_t)name->len;
		}
	} else {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_PARSE_FAILED);
	}

	mbedtls_x509_crt_free(&crt);

	return len;
}

int32_t LLCRYPTO_X509_CERT_IMPL_get_key(int8_t *cert_data, int32_t cert_data_length) {
	VEEPORT_LOGGER_LOG_DBG("");

	struct llcrypto_key *key = NULL;

	/* Defense-in-depth: bound cert_data_length against the array before the MbedTLS parser reads it. */
	if ((cert_data_length < 0) || (cert_data_length > microej_sni_get_array_length(cert_data))) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_LENGTH);
	} else {
		key = llcrypto_key_from_x509_certificate(cert_data, cert_data_length);
		if (NULL == key) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_PARSE_FAILED);
		}
	}

	return (intptr_t)key;
}

/**
 * @brief Maps a result of llcrypto_x509_verify_signature to a LLCRYPTO_X509_CERT_IMPL_verify return code.
 *
 * @param[in] verify_result a LLCRYPTO_X509_VERIFY_* code.
 * @return LLCRYPTO_SUCCESS or one of the LLCRYPTO_X509_*_EXCEPTION codes.
 */
static int32_t llcrypto_x509_cert_verify_result_to_llapi(int32_t verify_result) {
	int32_t return_code = LLCRYPTO_X509_SIGNATURE_EXCEPTION;

	switch (verify_result) {
	case LLCRYPTO_X509_VERIFY_OK:
		return_code = LLCRYPTO_SUCCESS;
		break;
	case LLCRYPTO_X509_VERIFY_BAD_ENCODING:
		return_code = LLCRYPTO_X509_CERTIFICATE_EXCEPTION;
		break;
	case LLCRYPTO_X509_VERIFY_NO_SUCH_ALGORITHM:
		return_code = LLCRYPTO_X509_NO_SUCH_ALGORITHM_EXCEPTION;
		break;
	case LLCRYPTO_X509_VERIFY_INVALID_KEY:
		return_code = LLCRYPTO_X509_INVALID_KEY_EXCEPTION;
		break;
	case LLCRYPTO_X509_VERIFY_BAD_SIGNATURE:
	default:
		/* Fail closed: anything unexpected is an invalid signature. */
		return_code = LLCRYPTO_X509_SIGNATURE_EXCEPTION;
		break;
	}

	return return_code;
}

int32_t LLCRYPTO_X509_CERT_IMPL_verify(int8_t *cert_data, int32_t cert_data_length, int32_t public_key_id) {
	VEEPORT_LOGGER_LOG_DBG("");

	// set the default return value to 'invalid signature' as a defence mechanism
	int32_t return_code = LLCRYPTO_X509_SIGNATURE_EXCEPTION;
	mbedtls_x509_crt crt;

	LLCRYPTO_PROFILE_START();
	mbedtls_x509_crt_init(&crt);
	if ((cert_data_length < 0) || (cert_data_length > microej_sni_get_array_length(cert_data))) {
		/* Out-of-bounds cert_data length: treat as an unparseable certificate per the return-code
		 * contract (the Java caller maps -2 to CertificateException). */
		return_code = LLCRYPTO_X509_CERTIFICATE_EXCEPTION;
	} else if (llcrypto_x509_crt_parse(&crt, (const uint8_t *)cert_data,
	                                   (size_t)cert_data_length) == LLCRYPTO_MBEDTLS_SUCCESS) {
		llcrypto_key_close_with_psa key_close = NULL;
		psa_key_id_t psa_id = PSA_KEY_ID_NULL;
		int32_t key_rc = llcrypto_key_get_psa_id((struct llcrypto_key *)public_key_id,
		                                         PSA_KEY_USAGE_VERIFY_MESSAGE, PSA_ALG_NONE, &psa_id, &key_close);
		if (LLCRYPTO_ERROR_OOM == key_rc) {
			llcrypto_throw_error(key_rc, LLCRYPTO_MSG_GET_PSA_KEY_FAILED);
			return_code = SNI_IGNORED_RETURNED_VALUE;
		} else if (LLCRYPTO_SUCCESS == key_rc) {
			int32_t verify_result = llcrypto_x509_verify_signature(&crt, psa_id);
			if (LLCRYPTO_X509_VERIFY_OUT_OF_MEMORY == verify_result) {
				VEEPORT_ERRORS_throw_runtime_exception(LLCRYPTO_ERROR_OOM);
				return_code = SNI_IGNORED_RETURNED_VALUE;
			} else {
				return_code = llcrypto_x509_cert_verify_result_to_llapi(verify_result);
			}

			psa_status_t close_rc = key_close(psa_id);
			if (PSA_SUCCESS != close_rc) {
				VEEPORT_LOGGER_LOG_WRN("key_close failed: %d", (int)close_rc);
			}
		} else {
			/* The public key could not be loaded into PSA: report an invalid key rather than
			 * masking it as an invalid signature, matching the InvalidKeyException declared by
			 * java.security.cert.Certificate.verify(PublicKey). */
			return_code = LLCRYPTO_X509_INVALID_KEY_EXCEPTION;
		}
	} else {
		return_code = LLCRYPTO_X509_CERTIFICATE_EXCEPTION;
	}

	mbedtls_x509_crt_free(&crt);

	LLCRYPTO_PROFILE_END();
	return return_code;
}

int32_t LLCRYPTO_X509_CERT_IMPL_check_validity(int8_t *cert_data, int32_t cert_data_length) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	int32_t return_code = SNI_IGNORED_RETURNED_VALUE;

	mbedtls_x509_crt crt;

	mbedtls_x509_crt_init(&crt);
	if ((cert_data_length < 0) || (cert_data_length > microej_sni_get_array_length(cert_data))) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_LENGTH);
	} else if (llcrypto_x509_crt_parse(&crt, (const uint8_t *)cert_data,
	                                   (size_t)cert_data_length) == LLCRYPTO_MBEDTLS_SUCCESS) {
		int mbedtls_is_past = mbedtls_x509_time_is_past(&crt.valid_to);
		int mbedtls_is_future = mbedtls_x509_time_is_future(&crt.valid_from);
		if (LLCRYPTO_MBEDTLS_SUCCESS != mbedtls_is_past) {
			VEEPORT_LOGGER_LOG_DBG("time is past");
			return_code = J_X509_CERT_EXPIRED_ERROR;
		} else if (LLCRYPTO_MBEDTLS_SUCCESS != mbedtls_is_future) {
			VEEPORT_LOGGER_LOG_DBG("time is future");
			return_code = J_X509_CERT_NOT_YET_VALID_ERROR;
		} else {
			return_code = J_SEC_NO_ERROR;
		}
	} else {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_PARSE_FAILED);
	}

	mbedtls_x509_crt_free(&crt);

	LLCRYPTO_PROFILE_END();
	return return_code;
}

int32_t LLCRYPTO_X509_CERT_IMPL_get_close_key(void) {
	return (intptr_t)llcrypto_key_free;
}
