/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>
#include <LLCRYPTO_CERT_PATH_VALIDATOR_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_key_psa.h"
#include "LLCRYPTO_utils.h"
#include "LLCRYPTO_x509.h"
#include "sni_extras.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

/*
 * Validation algorithm. The path is held target-first and is valid if:
 *  1. every certificate path[i] is issued by path[i+1]: its issuer Name is byte-equal to path[i+1]'s
 *     subject Name and its signature verifies with path[i+1]'s public key;
 *  2. every path[i+1] acting as an issuer is entitled to: it is a CA (basicConstraints), may sign
 *     certificates (keyUsage keyCertSign, an absent extension allowing every usage per RFC 5280), and
 *     its pathLenConstraint covers the i intermediate CAs below it;
 *  3. the last certificate is issued, in the sense of rule 1, by one of the trust anchors;
 *  4. every certificate of the path is within its validity period (notBefore/notAfter) at the current
 *     time. This rule is only enforced when MBEDTLS_HAVE_TIME_DATE is set: it reads the realtime
 *     clock, which starts at 1970 on a board without an RTC unless the application sets it
 *     (ej.bon.Util.setCurrentTimeMillis()).
 * An empty path is valid, a path longer than CONFIG_MICROEJ_CRYPTO_CERT_PATH_MAX_LENGTH certificates is
 * refused as it is built. Trust anchors are exempt from rule 2: RFC 5280 section 6.1 does not process
 * trust anchor constraints, the application chose them. Chaining is strict: a certificate is never
 * matched against the trust anchors before its successor in the path, so [leaf, anchor] with an
 * intermediate missing is invalid even when the anchor is trusted.
 *
 * Not checked: revocation.
 * mbedtls_x509_crt_verify() is not used because it looks a certificate's parent up in the trust store
 * before the rest of the path, which accepts the shortcut described above.
 */

/* One parsed certificate. Each node is parsed on its own so a failed parse cannot disturb the
 * certificates already accepted (llcrypto_x509_crt_parse resets the context it is given between its
 * DER and PEM attempts). */
struct llcrypto_cpv_cert {
	mbedtls_x509_crt crt;
	struct llcrypto_cpv_cert *next;
};

/* Both lists keep their append order: the path is target-first (see expect_forward_cert_path). */
struct llcrypto_cpv {
	struct llcrypto_cpv_cert *trusted_certs;
	struct llcrypto_cpv_cert *cert_chain;
	uint32_t cert_chain_length;
};

/**
 * @brief Frees a certificate list built by llcrypto_cpv_append().
 *
 * @param[in] list the head of the list, or NULL.
 */
static void llcrypto_cpv_free_list(struct llcrypto_cpv_cert *list) {
	struct llcrypto_cpv_cert *node = list;
	while (NULL != node) {
		struct llcrypto_cpv_cert *next = node->next;
		mbedtls_x509_crt_free(&node->crt);
		free(node);
		node = next;
	}
}

/**
 * @brief Releases a validator and its certificates. SNI close function of the validator resource.
 *
 * @param[in] resource the validator handle, or NULL.
 */
static void llcrypto_cpv_close(void *resource) {
	struct llcrypto_cpv *cpv = resource;
	VEEPORT_LOGGER_LOG_DBG("%p", resource);

	if (NULL != cpv) {
		llcrypto_cpv_free_list(cpv->trusted_certs);
		llcrypto_cpv_free_list(cpv->cert_chain);
		free(cpv);
	}
}

/**
 * @brief Parses a certificate and appends it to a list.
 *
 * x509 carries no length parameter (see the header warning), so the Java array length is the only
 * bound. The return code matters to the Java caller: LLCRYPTO_ERROR_OOM makes it retry once after a
 * GC before throwing OutOfMemoryError, any other error becomes IllegalArgumentException.
 *
 * @param[in,out] list the list to append to.
 * @param[in]     x509 the certificate, a Java byte array holding DER or PEM.
 * @return LLCRYPTO_SUCCESS, LLCRYPTO_ERROR_OOM when memory ran out, LLCRYPTO_ERROR when the certificate
 *         does not parse.
 */
static int32_t llcrypto_cpv_append(struct llcrypto_cpv_cert **list, uint8_t *x509) {
	int32_t return_code = LLCRYPTO_ERROR;
	int32_t x509_length = microej_sni_get_array_length(x509);

	if ((NULL == list) || (x509_length <= 0)) {
		VEEPORT_LOGGER_LOG_DBG("invalid argument (length %d)", x509_length);
	} else {
		struct llcrypto_cpv_cert *node = calloc(1, sizeof(struct llcrypto_cpv_cert));
		if (NULL == node) {
			return_code = LLCRYPTO_ERROR_OOM;
		} else {
			mbedtls_x509_crt_init(&node->crt);
			int32_t rc = llcrypto_x509_crt_parse(&node->crt, x509, (size_t)x509_length);
			if (LLCRYPTO_MBEDTLS_SUCCESS == rc) {
				struct llcrypto_cpv_cert **tail = list;
				while (NULL != *tail) {
					tail = &(*tail)->next;
				}
				*tail = node;
				return_code = LLCRYPTO_SUCCESS;
			} else {
				VEEPORT_LOGGER_LOG_DBG("certificate parse failed: -0x%04x",
				                       (unsigned int)(-rc));
				llcrypto_cpv_free_list(node);
				return_code = (MBEDTLS_ERR_X509_ALLOC_FAILED == rc) ? LLCRYPTO_ERROR_OOM : LLCRYPTO_ERROR;
			}
		}
	}

	return return_code;
}

/**
 * @brief Checks that a certificate is within its validity period at the current time.
 *
 * Rule 4 of the validation algorithm. Without MBEDTLS_HAVE_TIME_DATE the check is skipped and every
 * certificate is accepted.
 *
 * @param[in] crt the parsed certificate.
 *
 * @return true when the certificate is currently valid (or the check is disabled), false otherwise.
 */
static bool llcrypto_cpv_time_valid(const mbedtls_x509_crt *crt) {
	bool valid = true;
#if defined(MBEDTLS_HAVE_TIME_DATE)
	if (1 == mbedtls_x509_time_is_past(&crt->valid_to)) {
		VEEPORT_LOGGER_LOG_DBG("certificate expired");
		valid = false;
	} else if (1 == mbedtls_x509_time_is_future(&crt->valid_from)) {
		VEEPORT_LOGGER_LOG_DBG("certificate not yet valid");
		valid = false;
	} else {
		/* within [valid_from, valid_to] */
	}
#else
	LLCRYPTO_UNUSED_PARAM(crt);
	VEEPORT_LOGGER_LOG_DBG("skipped: MBEDTLS_HAVE_TIME_DATE not set");
#endif
	return valid;
}

/**
 * @brief Checks the constraints a certificate must carry to act as an issuer in the path.
 *
 * A valid signature is not enough to make a certificate an issuer: a leaf whose key happens to sign
 * another certificate must not extend the chain (RFC 5280 basicConstraints / keyUsage). An absent
 * keyUsage extension allows every usage, which mbedtls_x509_crt_check_key_usage() already honours.
 * mbedtls stores max_pathlen as the RFC 5280 pathLenConstraint + 1, with 0 meaning "no constraint";
 * there is no public accessor, hence MBEDTLS_PRIVATE().
 *
 * @param[in] issuer              the candidate issuer.
 * @param[in] nb_intermediates_below the number of intermediate CAs between issuer and the end-entity
 *                                certificate, i.e. what its pathLenConstraint must cover.
 * @return true if issuer is a CA, may sign certificates and its path length constraint is satisfied.
 */
static bool llcrypto_cpv_issuer_valid(const mbedtls_x509_crt *issuer, int32_t nb_intermediates_below) {
	int max_pathlen = issuer->MBEDTLS_PRIVATE(max_pathlen);
	return (1 == mbedtls_x509_crt_get_ca_istrue(issuer))
	       && (0 == mbedtls_x509_crt_check_key_usage(issuer, MBEDTLS_X509_KU_KEY_CERT_SIGN))
	       && ((0 == max_pathlen) || ((max_pathlen - 1) >= nb_intermediates_below));
}

/**
 * @brief Checks that child names parent as its issuer.
 *
 * Byte comparison of the DER Names, without the RFC 5280 canonicalization (case folding, whitespace):
 * the certificates of one PKI carry the same encoding of a given Name, and mbedtls_x509_crt_verify()
 * itself compares raw bytes.
 *
 * @param[in] child  the certificate whose issuer Name is compared.
 * @param[in] parent the certificate whose subject Name is compared.
 * @return true if child's issuer Name equals parent's subject Name.
 */
static bool llcrypto_cpv_names_chain(const mbedtls_x509_crt *child, const mbedtls_x509_crt *parent) {
	return (child->issuer_raw.len == parent->subject_raw.len)
	       && (0 == memcmp(child->issuer_raw.p, parent->subject_raw.p, child->issuer_raw.len));
}

/**
 * @brief Checks that child was issued by parent: parent is named as the issuer and its key verifies
 *        child's signature.
 *
 * The name check comes first so that the signature verification, by far the expensive part, only runs
 * on a plausible link. The parent's key is imported into PSA for the duration of the check rather than
 * kept in the node: the trust store is usually larger than the number of links that actually reach it.
 *
 * @param[in] child  the certificate to check.
 * @param[in] parent the candidate issuer.
 * @return LLCRYPTO_SUCCESS when parent issued child, LLCRYPTO_ERROR when it did not (or the check
 *         could not run), or LLCRYPTO_ERROR_OOM when the key import or the signature check failed for
 *         lack of memory or of a free PSA key slot.
 */
static int32_t llcrypto_cpv_is_issued_by(const mbedtls_x509_crt *child, const mbedtls_x509_crt *parent) {
	int32_t return_code = LLCRYPTO_ERROR;

	if (llcrypto_cpv_names_chain(child, parent)) {
		/* A volatile PSA copy of the issuer's public key; the parsed mbedtls_pk stays owned by parent. */
		psa_key_id_t issuer_key = PSA_KEY_ID_NULL;
		int32_t import_rc = llcrypto_psa_from_mbedtls_pk(&parent->pk, PSA_KEY_USAGE_VERIFY_MESSAGE, PSA_ALG_NONE,
		                                                 &issuer_key);
		if (LLCRYPTO_SUCCESS != import_rc) {
			/* A full key store is an OOM too, so that the Crypto library retries after a GC. */
			VEEPORT_LOGGER_LOG_DBG("issuer public key import failed: %d", import_rc);
			return_code = import_rc;
		} else {
			int32_t rc = llcrypto_x509_verify_signature(child, issuer_key);
			if (LLCRYPTO_X509_VERIFY_OK == rc) {
				return_code = LLCRYPTO_SUCCESS;
			} else if (LLCRYPTO_X509_VERIFY_OUT_OF_MEMORY == rc) {
				return_code = LLCRYPTO_ERROR_OOM;
			} else {
				VEEPORT_LOGGER_LOG_DBG("signature check failed: %d", rc);
				return_code = LLCRYPTO_ERROR;
			}

			psa_status_t status = psa_destroy_key(issuer_key);
			if (PSA_SUCCESS != status) {
				VEEPORT_LOGGER_LOG_WRN("psa_destroy_key failed: %d", (int)status);
			}
		}
	} else {
		VEEPORT_LOGGER_LOG_DBG("issuer/subject names do not chain");
	}

	return return_code;
}

int32_t LLCRYPTO_CERT_PATH_VALIDATOR_IMPL_new_cert_path_validator(void) {
	VEEPORT_LOGGER_LOG_DBG("");
	int32_t return_code = LLCRYPTO_ERROR;

	struct llcrypto_cpv *cpv = calloc(1, sizeof(struct llcrypto_cpv));
	if (NULL == cpv) {
		return_code = LLCRYPTO_ERROR_OOM;
	} else if (SNI_OK != SNI_registerResource(cpv, llcrypto_cpv_close, NULL)) {
		llcrypto_cpv_close(cpv);
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_SNI_REGISTER_FAILED);
		return_code = SNI_IGNORED_RETURNED_VALUE;
	} else {
		return_code = (intptr_t)cpv;
	}

	return return_code;
}

int32_t LLCRYPTO_CERT_PATH_VALIDATOR_IMPL_append_trusted_certificate(int32_t cpv, uint8_t *x509) {
	VEEPORT_LOGGER_LOG_DBG("");
	struct llcrypto_cpv *validator = (struct llcrypto_cpv *)cpv;

	return llcrypto_cpv_append(&validator->trusted_certs, x509);
}

jboolean LLCRYPTO_CERT_PATH_VALIDATOR_IMPL_expect_forward_cert_path(void) {
	VEEPORT_LOGGER_LOG_DBG("");

	/* Forward order: the target certificate first, then each issuer toward the trust anchor. */
	return JTRUE;
}

int32_t LLCRYPTO_CERT_PATH_VALIDATOR_IMPL_append_cert_to_path(int32_t cpv, uint8_t *x509) {
	VEEPORT_LOGGER_LOG_DBG("");
	struct llcrypto_cpv *validator = (struct llcrypto_cpv *)cpv;
	int32_t return_code = LLCRYPTO_ERROR;

	if (validator->cert_chain_length >= (uint32_t)CONFIG_MICROEJ_CRYPTO_CERT_PATH_MAX_LENGTH) {
		VEEPORT_LOGGER_LOG_DBG(
			"path longer than CONFIG_MICROEJ_CRYPTO_CERT_PATH_MAX_LENGTH (%u)",
			(unsigned int)CONFIG_MICROEJ_CRYPTO_CERT_PATH_MAX_LENGTH);
	} else {
		return_code = llcrypto_cpv_append(&validator->cert_chain, x509);
		if (LLCRYPTO_SUCCESS == return_code) {
			validator->cert_chain_length++;
		}
	}

	return return_code;
}

int32_t LLCRYPTO_CERT_PATH_VALIDATOR_IMPL_validate_cert_path(int32_t cpv) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	struct llcrypto_cpv *validator = (struct llcrypto_cpv *)cpv;

	int32_t return_code = LLCRYPTO_SUCCESS;

	const struct llcrypto_cpv_cert *node = validator->cert_chain;

	/*
	 * Step 1:
	 * - Verify that each certificate of the path is within its validity period.
	 * - Verify issuer/subject name chaining along the path.
	 * - Verify signatures along the chain, and that each certificate acting as an issuer is entitled to.
	 */
	int32_t index = 0;
	while ((LLCRYPTO_SUCCESS == return_code) && (NULL != node) && (NULL != node->next)) {
		if (!llcrypto_cpv_time_valid(&node->crt)) {
			VEEPORT_LOGGER_LOG_DBG("certificate %u is outside its validity period", (unsigned int)index);
			return_code = LLCRYPTO_ERROR;
		} else if (!llcrypto_cpv_issuer_valid(&node->next->crt, index)) {
			VEEPORT_LOGGER_LOG_DBG("certificate %u cannot issue certificates",
			                       (unsigned int)((uint32_t)index + 1U));
			return_code = LLCRYPTO_ERROR;
		} else {
			return_code = llcrypto_cpv_is_issued_by(&node->crt, &node->next->crt);
		}
		node = node->next;
		index++;
	}

	/*
	 * Step 2: the last certificate must be issued by a trust anchor. An empty path has nothing to check.
	 */
	if ((LLCRYPTO_SUCCESS == return_code) && (NULL != node)) {
		if (!llcrypto_cpv_time_valid(&node->crt)) {
			VEEPORT_LOGGER_LOG_DBG("certificate %u is outside its validity period", (unsigned int)index);
			return_code = LLCRYPTO_ERROR;
		} else {
			/* A match (LLCRYPTO_SUCCESS) or an out-of-memory (LLCRYPTO_ERROR_OOM) ends the search and is the result. */
			return_code = LLCRYPTO_ERROR;
			for (const struct llcrypto_cpv_cert *anchor = validator->trusted_certs;
			     (LLCRYPTO_ERROR == return_code) && (NULL != anchor); anchor = anchor->next) {
				return_code = llcrypto_cpv_is_issued_by(&node->crt, &anchor->crt);
			}
			if (LLCRYPTO_SUCCESS != return_code) {
				VEEPORT_LOGGER_LOG_DBG("no trust anchor issued the path root");
			}
		}
	}

	LLCRYPTO_PROFILE_END();
	return return_code;
}

void LLCRYPTO_CERT_PATH_VALIDATOR_IMPL_close_cert_path_validator(int32_t cpv) {
	VEEPORT_LOGGER_LOG_DBG("");
	struct llcrypto_cpv *validator = (struct llcrypto_cpv *)cpv;

	llcrypto_unregister_and_close(validator, llcrypto_cpv_close);
}
