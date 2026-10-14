/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <mbedtls/asn1.h>
#include <mbedtls/pk.h>
#include <mbedtls/platform_util.h>
#include <sni.h>
#include <LLCRYPTO_CONSTANTS.h>

#include "LLCRYPTO_key.h"
#include "LLCRYPTO_x509.h"
#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_key_der.h"
#include "LLCRYPTO_key_psa.h"
#include "LLCRYPTO_utils.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

static struct llcrypto_key *llcrypto_key_new(enum llcrypto_key_backend backend) {
	struct llcrypto_key *key = malloc(sizeof(struct llcrypto_key));

	if (NULL != key) {
		key->backend = backend;

		switch (backend) {
		case LLCRYPTO_KEY_BACKEND_MBEDTLS_PK:
			key->mbedtls_pk = malloc(sizeof(mbedtls_pk_context));

			if (NULL != key->mbedtls_pk) {
				mbedtls_pk_init(key->mbedtls_pk);
			} else {
				free(key);
				key = NULL;
			}
			break;
		case LLCRYPTO_KEY_BACKEND_MBEDTLS_X509:
			key->mbedtls_crt = malloc(sizeof(mbedtls_x509_crt));

			if (NULL != key->mbedtls_crt) {
				mbedtls_x509_crt_init(key->mbedtls_crt);
			} else {
				free(key);
				key = NULL;
			}
			break;
		case LLCRYPTO_KEY_BACKEND_PSA:
			key->psa_id = PSA_KEY_ID_NULL;
			break;
		case LLCRYPTO_KEY_BACKEND_SECRET_KEY:
			key->secret_key.buffer = NULL;
			key->secret_key.length = 0;
			break;
		default:
			VEEPORT_LOGGER_LOG_DBG("Unknown key type %d", backend);
			free(key);
			key = NULL;
			break;
		}
	}

	if (NULL != key) {
		/* The key is handed to Java as a live handle, so it must be tracked by the SNI resource table to
		 * guarantee llcrypto_key_free runs on GC. If registration fails (resource table exhausted), free the
		 * key here and return NULL so the caller throws, rather than leaking an untracked handle. */
		if (SNI_OK != SNI_registerResource(key, (SNI_closeFunction)llcrypto_key_free, NULL)) {
			VEEPORT_LOGGER_LOG_DBG("SNI_registerResource failed");
			llcrypto_key_free(key);
			key = NULL;
		}
	}

	return key;
}

int32_t llcrypto_key_get_encoded_max_size(const struct llcrypto_key *key) {
	int32_t max_size = 0;

	switch (key->backend) {
	case LLCRYPTO_KEY_BACKEND_SECRET_KEY:
		/* The buffer is heap-allocated and far below 2 GB, so this never truncates; the assert
		 * traps a corrupt length in debug before the narrowing cast. */
		LLCRYPTO_ASSERT(key->secret_key.length <= (size_t)INT32_MAX);
		max_size = (int32_t)key->secret_key.length;
		break;
	case LLCRYPTO_KEY_BACKEND_PSA:
	{
		psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
		if (PSA_SUCCESS == psa_get_key_attributes(key->psa_id, &attributes)) {
			/* Round up: a non-byte-aligned key size (e.g. EC secp521r1 = 521 bits) needs
			 * ceil(bits / 8) bytes, so use PSA_BITS_TO_BYTES rather than truncating division. */
			size_t key_bytes = PSA_BITS_TO_BYTES(psa_get_key_bits(&attributes));
			/* PSA caps key sizes at PSA_MAX_KEY_BITS (a few KB of bytes), so this never truncates;
			 * the assert documents the invariant and traps a corrupt attribute set in debug. */
			LLCRYPTO_ASSERT(key_bytes <= (size_t)INT32_MAX);
			max_size = (int32_t)key_bytes;
		}
		psa_reset_key_attributes(&attributes);
		break;
	}
	default:
		break;
	}

	VEEPORT_LOGGER_LOG_DBG("max_size = %d", max_size);
	return max_size;
}

int32_t llcrypto_key_get_output_size(const struct llcrypto_key *key) {
	int32_t output_size = 0;

	switch (key->backend) {
	case LLCRYPTO_KEY_BACKEND_PSA:
	{
		psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
		if (PSA_SUCCESS == psa_get_key_attributes(key->psa_id, &attributes)) {
			psa_key_type_t key_type = psa_get_key_type(&attributes);
			size_t key_bits = psa_get_key_bits(&attributes);
			/* The native getOutputSize contract is the RSA modulus length in bytes (the ciphertext/block
			 * size), which is identical for encryption and decryption. PSA_ASYMMETRIC_ENCRYPT_OUTPUT_SIZE
			 * expands to PSA_BITS_TO_BYTES(key_bits) for RSA keys and is algorithm-independent, so
			 * PSA_ALG_NONE is a valid placeholder. It returns 0 for non-asymmetric/unsupported key types,
			 * which the callers report as LLCRYPTO_ERROR. The decrypt macro must NOT be used here: it subtracts
			 * a padding allowance (defaulting to PKCS#1 v1.5's 11 bytes under PSA_ALG_NONE), yielding the
			 * maximum plaintext length rather than the modulus length. */
			/* cppcheck-suppress misra-c2012-10.4 ; mismatch is inside vendored macro PSA_ASYMMETRIC_ENCRYPT_OUTPUT_SIZE
			 */
			output_size = (int32_t)PSA_ASYMMETRIC_ENCRYPT_OUTPUT_SIZE(key_type, key_bits, PSA_ALG_NONE);
		}
		psa_reset_key_attributes(&attributes);
		break;
	}
	case LLCRYPTO_KEY_BACKEND_MBEDTLS_PK:
	{
		size_t key_bytes = PSA_BITS_TO_BYTES(mbedtls_pk_get_bitlen(key->mbedtls_pk));
		output_size = (int32_t)key_bytes;
		break;
	}
	case LLCRYPTO_KEY_BACKEND_MBEDTLS_X509:
	{
		size_t key_bytes = PSA_BITS_TO_BYTES(mbedtls_pk_get_bitlen(&key->mbedtls_crt->pk));
		output_size = (int32_t)key_bytes;
		break;
	}
	default:
		/* Symmetric (SECRET_KEY) and unknown backends have no asymmetric output size: output_size stays 0,
		 * which the callers report as LLCRYPTO_ERROR. */
		VEEPORT_LOGGER_LOG_DBG("unsupported backend %d, output_size = 0", key->backend);
		break;
	}

	VEEPORT_LOGGER_LOG_DBG("output_size = %d", output_size);
	return output_size;
}

int32_t llcrypto_key_get_encoded(const struct llcrypto_key *key, uint8_t *output, int32_t output_length) {
	int32_t key_length = 0;

	switch (key->backend) {
	case LLCRYPTO_KEY_BACKEND_SECRET_KEY:
		if (key->secret_key.length <= (size_t)output_length) {
			(void)memcpy(output, key->secret_key.buffer, key->secret_key.length);
			key_length = (int32_t)key->secret_key.length;
		}
		break;
	case LLCRYPTO_KEY_BACKEND_PSA:
	{
		psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
		if ((PSA_SUCCESS == psa_get_key_attributes(key->psa_id, &attributes)) &&
		    (psa_get_key_usage_flags(&attributes) & PSA_KEY_USAGE_EXPORT) == PSA_KEY_USAGE_EXPORT) {
			size_t exported_length = 0;
			psa_status_t status =
				psa_export_key(key->psa_id, output, (size_t)output_length, &exported_length);
			if (PSA_SUCCESS == status) {
				key_length = (int32_t)exported_length;
			}
		}
		psa_reset_key_attributes(&attributes);
		break;
	}
	default:
		break;
	}

	VEEPORT_LOGGER_LOG_DBG("key_length = %d", key_length);
	return key_length;
}

int32_t llcrypto_key_export_pkcs8(const struct llcrypto_key *key, uint8_t *output, size_t output_length) {
	int32_t return_code = LLCRYPTO_ERROR;

	switch (key->backend) {
	case LLCRYPTO_KEY_BACKEND_PSA:
		return_code = llcrypto_psa_export_pkcs8(key->psa_id, output, output_length);
		break;
	default:
		/* A SECRET_KEY has no PKCS#8 encoding; MBEDTLS_PK and X509 hold only a public key. */
		VEEPORT_LOGGER_LOG_DBG("unsupported backend %d", key->backend);
		break;
	}

	VEEPORT_LOGGER_LOG_DBG("return_code = %d", return_code);
	return return_code;
}

int32_t llcrypto_key_export_spki(const struct llcrypto_key *key, uint8_t *output, int32_t output_length) {
	int32_t return_code = LLCRYPTO_ERROR;

	switch (key->backend) {
	case LLCRYPTO_KEY_BACKEND_PSA:
		/* A PSA key has no PK context: copy its public part into one and serialize it. */
		return_code = llcrypto_psa_export_spki(key->psa_id, output, output_length);
		break;
	case LLCRYPTO_KEY_BACKEND_MBEDTLS_PK:
		return_code = llcrypto_pk_write_spki(key->mbedtls_pk, output, output_length);
		break;
	case LLCRYPTO_KEY_BACKEND_MBEDTLS_X509:
		return_code = llcrypto_pk_write_spki(&key->mbedtls_crt->pk, output, output_length);
		break;
	default:
		/* A SECRET_KEY has no SubjectPublicKeyInfo encoding. */
		VEEPORT_LOGGER_LOG_DBG("unsupported backend %d", key->backend);
		break;
	}

	VEEPORT_LOGGER_LOG_DBG("return_code = %d", return_code);
	return return_code;
}

void llcrypto_key_free(struct llcrypto_key *key) {
	switch (key->backend) {
	case LLCRYPTO_KEY_BACKEND_MBEDTLS_PK:
		mbedtls_pk_free(key->mbedtls_pk);
		free(key->mbedtls_pk);
		break;
	case LLCRYPTO_KEY_BACKEND_MBEDTLS_X509:
		mbedtls_x509_crt_free(key->mbedtls_crt);
		free(key->mbedtls_crt);
		break;
	case LLCRYPTO_KEY_BACKEND_PSA:
	{
		// Volatile keys that we imported into PSA, or unwrapped keys (indirectly imported) must be destroyed
		psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
		if (PSA_SUCCESS == psa_get_key_attributes(key->psa_id, &attributes)) {
			psa_key_lifetime_t lifetime = psa_get_key_lifetime(&attributes);
			/* cppcheck-suppress misra-c2012-10.4 ; mismatch is inside vendored macro PSA_KEY_LIFETIME_IS_VOLATILE */
			if (PSA_KEY_LIFETIME_IS_VOLATILE(lifetime)) {
				VEEPORT_LOGGER_LOG_DBG("Destroy Key ID 0x%08x", key->psa_id);
				psa_status_t destroy_rc = psa_destroy_key(key->psa_id);
				if (PSA_SUCCESS != destroy_rc) {
					VEEPORT_LOGGER_LOG_DBG("psa_destroy_key failed for Key ID 0x%08x: %d",
					                       key->psa_id, (int)destroy_rc);
				}
			}
		} else {
			VEEPORT_LOGGER_LOG_DBG("psa_get_key_attributes failed for Key ID 0x%08x", key->psa_id);
		}
		/* Reset key attributes and free any allocated resources. */
		psa_reset_key_attributes(&attributes);
		break;
	}
	case LLCRYPTO_KEY_BACKEND_SECRET_KEY:
		mbedtls_platform_zeroize(key->secret_key.buffer, key->secret_key.length);
		free(key->secret_key.buffer);
		break;
	default:
		VEEPORT_LOGGER_LOG_DBG("Unknown key type %d", key->backend);
		break;
	}

	free(key);
}

static void llcrypto_key_unregister_and_free(struct llcrypto_key *key) {
	if (SNI_OK != SNI_unregisterResource(key, (SNI_closeFunction)llcrypto_key_free)) {
		VEEPORT_LOGGER_LOG_DBG("SNI_unregisterResource failed");
	} else {
		llcrypto_key_free(key);
	}
}

/**
 * @brief Length of the SubjectPublicKeyInfo the parser must see: the DER SEQUENCE without the trailing
 * padding the Java layer may append after an X509EncodedKeySpec, or the whole buffer when the input
 * is not DER (PEM), which the parser handles itself.
 *
 * mbedtls_pk_parse_public_key rejects any byte left after the DER structure, so the SEQUENCE header
 * is read to recover the exact size: the header octets plus the content length they announce. The
 * header size comes from llcrypto_asn1_header_length rather than from the parser pointer, so that no
 * pointer arithmetic is needed (MISRA C:2012 rules 10.8 and 18.4).
 */
static size_t llcrypto_key_public_key_length(uint8_t *encoded_key, size_t key_length) {
	size_t parse_length = key_length;
	unsigned char *content = encoded_key;
	size_t content_length = 0;

	int rc = mbedtls_asn1_get_tag(&content, &encoded_key[key_length], &content_length,
	                              MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
	if (LLCRYPTO_MBEDTLS_SUCCESS == rc) {
		size_t header_length = llcrypto_asn1_header_length(encoded_key, key_length);
		parse_length = header_length + content_length;
	}

	return parse_length;
}

struct llcrypto_key *llcrypto_key_from_x509_public_key(uint8_t *encoded_key, size_t key_length) {
	struct llcrypto_key *key = llcrypto_key_new(LLCRYPTO_KEY_BACKEND_MBEDTLS_PK);

	if (NULL != key) {
		/* X509EncodedKeySpec is a SubjectPublicKeyInfo, DER (trimmed to its actual length) or PEM. */
		size_t parse_length = llcrypto_key_public_key_length(encoded_key, key_length);
		int rc = mbedtls_pk_parse_public_key(key->mbedtls_pk, encoded_key, parse_length);
		if (LLCRYPTO_MBEDTLS_SUCCESS != rc) {
			VEEPORT_LOGGER_LOG_DBG("mbedtls_pk_parse_public_key failed: %d", rc);
			llcrypto_key_unregister_and_free(key);
			key = NULL;
		}
	}

	return key;
}

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
int32_t llcrypto_key_from_ec_public_key(psa_ecc_family_t family, size_t bits, const uint8_t *point,
                                        size_t point_length, struct llcrypto_key **key_out) {
	int32_t return_code = LLCRYPTO_ERROR_OOM;
	struct llcrypto_key *key = llcrypto_key_new(LLCRYPTO_KEY_BACKEND_PSA);

	if (NULL != key) {
		psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
		/* Same policy as a generated key: exportable only, so that llcrypto_key_get_psa_id re-imports
		 * it with the usage and algorithm each operation asks for. psa_import_key checks that the
		 * point lies on the curve. */
		/* cppcheck-suppress misra-c2012-10.4 ; mismatch is inside vendored macro PSA_KEY_TYPE_ECC_PUBLIC_KEY */
		psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(family));
		psa_set_key_bits(&attributes, bits);
		psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_EXPORT);
		psa_set_key_algorithm(&attributes, PSA_ALG_NONE);
		psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_VOLATILE);

		psa_status_t status = psa_import_key(&attributes, point, point_length, &key->psa_id);
		psa_reset_key_attributes(&attributes);

		if (PSA_SUCCESS == status) {
			return_code = LLCRYPTO_SUCCESS;
		} else {
			VEEPORT_LOGGER_LOG_DBG("psa_import_key failed: %d", (int)status);
			llcrypto_key_unregister_and_free(key);
			key = NULL;
			return_code = llcrypto_sni_error_code(status);
		}
	}

	*key_out = key;
	return return_code;
}

#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

int32_t llcrypto_key_from_pkcs8_private_key(uint8_t *encoded_key, size_t key_length, struct llcrypto_key **key_out) {
	int32_t return_code = LLCRYPTO_ERROR_OOM;
	struct llcrypto_key *key = llcrypto_key_new(LLCRYPTO_KEY_BACKEND_PSA);

	if (NULL != key) {
		return_code = llcrypto_psa_import_pkcs8(encoded_key, key_length, &key->psa_id);
		if (LLCRYPTO_SUCCESS != return_code) {
			VEEPORT_LOGGER_LOG_DBG("llcrypto_psa_import_pkcs8 failed: %d", return_code);
			llcrypto_key_unregister_and_free(key);
			key = NULL;
		}
	}

	*key_out = key;
	return return_code;
}

struct llcrypto_key *llcrypto_key_from_x509_certificate(uint8_t *encoded_cert, size_t encoded_length) {
	struct llcrypto_key *key = llcrypto_key_new(LLCRYPTO_KEY_BACKEND_MBEDTLS_X509);

	if (NULL != key) {
		int32_t rc = llcrypto_x509_crt_parse(key->mbedtls_crt, encoded_cert, encoded_length);

		if (LLCRYPTO_MBEDTLS_SUCCESS != rc) {
			llcrypto_key_unregister_and_free(key);
			key = NULL;
		}
	}

	return key;
}

struct llcrypto_key *llcrypto_key_from_secret_key(uint8_t *secret_key, size_t key_length, uint8_t *algorithm) {
	struct llcrypto_key *key = llcrypto_key_new(LLCRYPTO_KEY_BACKEND_SECRET_KEY);

	if (NULL != key) {
		key->secret_key.buffer = (uint8_t *)malloc(key_length);
		if (NULL != key->secret_key.buffer) {
			(void)memcpy(key->secret_key.buffer, secret_key, key_length);
			key->secret_key.length = key_length;
			key->secret_key.type = llcrypto_psa_type_from_java_algorithm(algorithm);
		} else {
			llcrypto_key_unregister_and_free(key);
			key = NULL;
		}
	}

	return key;
}

struct llcrypto_key *llcrypto_key_from_psa_id(psa_key_id_t psa_id) {
	struct llcrypto_key *key = llcrypto_key_new(LLCRYPTO_KEY_BACKEND_PSA);

	if (NULL != key) {
		key->psa_id = psa_id;
	}

	return key;
}

/**
 * @brief A close function that does nothing for `psa_key_id_t` obtained from `llcrypto_key_get_psa_id()`.
 *
 * This function is returned as a pointer to close a psa_key_id_t the previously existed in the source `struct
 * llcrypto_key` of type `LLCRYPTO_KEY_BACKEND_PSA`
 *
 * @param id ignored
 * @return psa_status_t always return PSA_SUCCESS
 */
static psa_status_t llcrypto_key_close_with_psa_noop(psa_key_id_t id) {
	VEEPORT_LOGGER_LOG_DBG("Destroy Key ID 0x%08x (noop)", id);
	LLCRYPTO_UNUSED_PARAM(id);
	return PSA_SUCCESS;
}

static psa_status_t llcrypto_key_close_with_psa_destroy(psa_key_id_t id) {
	VEEPORT_LOGGER_LOG_DBG("Destroy Key ID 0x%08x", id);
	return psa_destroy_key(id);
}

int32_t llcrypto_key_get_psa_id(struct llcrypto_key *key, psa_key_usage_t requested_usage,
                                psa_algorithm_t requested_algorithm, psa_key_id_t *id,
                                llcrypto_key_close_with_psa *key_close) {
	int32_t return_code = LLCRYPTO_ERROR;
	*id = PSA_KEY_ID_NULL;
	*key_close = llcrypto_key_close_with_psa_noop;

	switch (key->backend) {
	case LLCRYPTO_KEY_BACKEND_MBEDTLS_PK:
		return_code = llcrypto_psa_from_mbedtls_pk(key->mbedtls_pk, requested_usage, requested_algorithm, id);
		if (LLCRYPTO_SUCCESS == return_code) {
			*key_close = llcrypto_key_close_with_psa_destroy;
		} else {
			VEEPORT_LOGGER_LOG_DBG("failed to get PSA key from MBEDTLS_PK");
		}
		break;
	case LLCRYPTO_KEY_BACKEND_MBEDTLS_X509:
		return_code = llcrypto_psa_from_mbedtls_pk(&key->mbedtls_crt->pk, requested_usage, requested_algorithm, id);
		if (LLCRYPTO_SUCCESS == return_code) {
			*key_close = llcrypto_key_close_with_psa_destroy;
		} else {
			VEEPORT_LOGGER_LOG_DBG("failed to get PSA key from MBEDTLS_X509");
		}
		break;
	case LLCRYPTO_KEY_BACKEND_PSA:
	{
		psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
		if (PSA_SUCCESS == psa_get_key_attributes(key->psa_id, &attributes)) {
			psa_key_usage_t key_usage = psa_get_key_usage_flags(&attributes);
			psa_algorithm_t key_algorithm = psa_get_key_algorithm(&attributes);
			VEEPORT_LOGGER_LOG_DBG("usage     0x%08x requested_usage     0x%08x", key_usage,
			                       requested_usage);
			VEEPORT_LOGGER_LOG_DBG("algorithm 0x%08x requested_algorithm 0x%08x", key_algorithm,
			                       requested_algorithm);
			if (((key_usage & requested_usage) == requested_usage) &&
			    ((key_algorithm & requested_algorithm) == requested_algorithm)) {
				*id = key->psa_id;
				*key_close = llcrypto_key_close_with_psa_noop;
				return_code = LLCRYPTO_SUCCESS;
			} else if ((PSA_ALG_NONE != requested_algorithm) &&
			           ((key_usage & PSA_KEY_USAGE_EXPORT) == PSA_KEY_USAGE_EXPORT)) {
				/* This key is not provisioned with the requested usage and algorithm but it is
				 * exportable, so re-import it into PSA with the proper attributes. */
				return_code = llcrypto_psa_reimport_with_attributes(key->psa_id, requested_usage,
				                                                    requested_algorithm, id);
				if (LLCRYPTO_SUCCESS == return_code) {
					*key_close = llcrypto_key_close_with_psa_destroy;
				}
			} else {
				/* Key neither provisioned with the requested usage/algorithm nor exportable:
				 * leave the error code so the caller reports the failure. */
				VEEPORT_LOGGER_LOG_DBG("key not usable for requested usage/algorithm and not exportable");
			}
		}
		psa_reset_key_attributes(&attributes);
		break;
	}
	case LLCRYPTO_KEY_BACKEND_SECRET_KEY:
		return_code = llcrypto_psa_from_secret_key(key->secret_key, requested_usage, requested_algorithm, id);
		if (LLCRYPTO_SUCCESS == return_code) {
			*key_close = llcrypto_key_close_with_psa_destroy;
		} else {
			VEEPORT_LOGGER_LOG_DBG("failed to get PSA key from SECRET_KEY");
		}
		break;
	default:
		return_code = LLCRYPTO_ERROR;
		break;
	}

	return return_code;
}

int32_t llcrypto_key_get_type(const struct llcrypto_key *key) {
	psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
	int32_t ret = LLCRYPTO_ERROR;

	if (LLCRYPTO_KEY_BACKEND_PSA == key->backend) {
		if (PSA_SUCCESS == psa_get_key_attributes(key->psa_id, &attrs)) {
			psa_key_type_t psa_type = psa_get_key_type(&attrs);
			if (PSA_KEY_TYPE_IS_KEY_PAIR(psa_type)) {
				ret = LLCRYPTO_KEY_TYPE_PRIVATE;
			} else if (PSA_KEY_TYPE_IS_PUBLIC_KEY(psa_type)) {
				ret = LLCRYPTO_KEY_TYPE_PUBLIC;
			} else {
				ret = LLCRYPTO_KEY_TYPE_SECRET;
			}
		}
		psa_reset_key_attributes(&attrs);
	} else {
		VEEPORT_LOGGER_LOG_DBG("key->backend %d is not LLCRYPTO_KEY_BACKEND_PSA", key->backend);
	}
	return ret;
}

int32_t llcrypto_key_get_algorithm_name(const struct llcrypto_key *key, char *out, size_t out_len) {
	psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
	int32_t ret = LLCRYPTO_ERROR;

	if (LLCRYPTO_KEY_BACKEND_PSA == key->backend) {
		if (PSA_SUCCESS == psa_get_key_attributes(key->psa_id, &attrs)) {
			psa_key_type_t psa_type = psa_get_key_type(&attrs);
			const char *name = NULL;
			/*
			 * https://docs.nordicsemi.com/bundle/ncs-3.1.1/page/nrf/app_dev/device_guides/nrf54l/cryptography.html#supported_key_types
			 */
			if (PSA_KEY_TYPE_AES == psa_type) {
				name = "AES";
			}
			/* cppcheck-suppress misra-c2012-10.4 ; mismatch is inside vendored macro PSA_KEY_TYPE_IS_ECC */
			else if (PSA_KEY_TYPE_IS_ECC(psa_type)) {
				name = "EC";
			} else if (PSA_KEY_TYPE_HMAC == psa_type) {
				/* The HMAC hash is carried by the key's algorithm policy, not its type. */
				/* cppcheck-suppress misra-c2012-10.4 ; mismatch is inside vendored macro PSA_ALG_GET_HASH */
				/* cppcheck-suppress misra-c2012-12.1 ; precedence is inside vendored macro PSA_ALG_GET_HASH */
				psa_algorithm_t hash = PSA_ALG_GET_HASH(psa_get_key_algorithm(&attrs));
				if (PSA_ALG_SHA_256 == hash) {
					name = "HmacSHA256";
				} else if (PSA_ALG_SHA_1 == hash) {
					name = "HmacSHA1";
				} else if (PSA_ALG_MD5 == hash) {
					name = "HmacMD5";
				} else {
					/* Unsupported/absent HMAC hash: leave name NULL so the caller gets LLCRYPTO_ERROR. */
				}
			} else {
				/* Unsupported key type: leave name NULL; the name == NULL branch below logs it
				 * and the function returns LLCRYPTO_ERROR. */
			}

			if (NULL != name) {
				if (out_len > 0u) {
					(void)strncpy(out, name, out_len);
					/* strncpy result may not be null-terminated. */
					out[out_len - 1u] = '\0';
					ret = LLCRYPTO_SUCCESS;
				} else {
					VEEPORT_LOGGER_LOG_DBG("output buffer too small (out_len<=0)");
				}
			} else {
				VEEPORT_LOGGER_LOG_DBG("unsupported psa_key_type_t 0x%x",
				                       (unsigned int)psa_type);
			}
		}
		psa_reset_key_attributes(&attrs);
	} else {
		VEEPORT_LOGGER_LOG_DBG("key->backend %d is not LLCRYPTO_KEY_BACKEND_PSA", key->backend);
	}
	return ret;
}
