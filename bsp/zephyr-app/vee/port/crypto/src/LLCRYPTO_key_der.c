/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <mbedtls/asn1.h>
#include <mbedtls/asn1write.h>
#include <mbedtls/pk.h>
#include <mbedtls/platform_util.h>
#include <psa/crypto.h>
#include <LLCRYPTO_CONSTANTS.h>

#include "LLCRYPTO_key_der.h"
#include "LLCRYPTO_key_psa.h"
#include "LLCRYPTO_ec_curve.h"
#include "LLCRYPTO_utils.h"
#include "LLCRYPTO_configuration.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

/*
 * Accumulate one ASN.1 write step (which fills the buffer from its end, returning the byte count or a
 * negative error) into `total`, but only while no earlier step has failed. This keeps the DER builders
 * to a single function exit: once `ret` is negative it stays negative and the remaining steps are
 * skipped. Mirrors mbedtls' own MBEDTLS_ASN1_CHK_ADD without its early `return`.
 */
#define LLCRYPTO_ASN1_CHK_ADD(total, ret, expr) \
		do {                                    \
			if ((ret) >= 0) {                   \
				(ret) = (expr);                 \
				if ((ret) >= 0) {               \
					(total) += (ret);           \
				}                               \
			}                                   \
		} while (0)

/* PKCS#8 PrivateKeyInfo.version (RFC 5208) and SEC1 ECPrivateKey.version (RFC 5915). */
#define LLCRYPTO_PKCS8_VERSION          (0)
#define LLCRYPTO_EC_PRIVATE_KEY_VERSION (1)

/* Content octets of the privateKeyAlgorithm OIDs of the supported private keys, read by the PKCS#8 parser and
 * written by the PKCS#8 writer. TF-PSA-Crypto keeps its OID tables private, so the port holds the bytes; the
 * named-curve OIDs are in the curve table (LLCRYPTO_ec_curve.c). */
#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
/* rsaEncryption, 1.2.840.113549.1.1.1 (RFC 8017) */
static const uint8_t llcrypto_oid_rsa_encryption[] = { 0x2Au, 0x86u, 0x48u, 0x86u, 0xF7u, 0x0Du, 0x01u, 0x01u, 0x01u };
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
/* id-ecPublicKey, 1.2.840.10045.2.1 (RFC 5480) */
static const uint8_t llcrypto_oid_ec_public_key[] = { 0x2Au, 0x86u, 0x48u, 0xCEu, 0x3Du, 0x02u, 0x01u };
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

#if defined(MBEDTLS_PK_WRITE_C)

/**
 * @brief Write the privateKeyAlgorithm of a PSA key pair into a DER buffer, backward from `*p`.
 *
 * Emits {@code AlgorithmIdentifier ::= SEQUENCE { algorithm OID, parameters }}: rsaEncryption with NULL
 * parameters for an RSA key, id-ecPublicKey with the named-curve OID for an EC key (RFC 5480).
 *
 * @param p     the current position, moved backward over the written bytes.
 * @param start the start of the buffer.
 * @param type  the PSA key type.
 * @param bits  the key size in bits.
 * @return Bytes written on success, or a negative mbedtls error code (MBEDTLS_ERR_ASN1_INVALID_DATA for an
 *         unsupported key type or curve).
 */
static int llcrypto_asn1_write_key_alg_id(unsigned char **p, const unsigned char *start, psa_key_type_t type,
                                          size_t bits) {
	/* Stays an error when the key type, or its curve, is not supported. The key types are mutually exclusive,
	 * so each enabled one has its own test rather than a branch of an if-else chain. */
	int ret = MBEDTLS_ERR_ASN1_INVALID_DATA;

#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
	/* cppcheck-suppress misra-c2012-10.4 ; mismatch is inside vendored macro PSA_KEY_TYPE_IS_RSA */
	if (PSA_KEY_TYPE_IS_RSA(type)) {
		ret = mbedtls_asn1_write_null(p, start);
		if (ret >= 0) {
			ret = mbedtls_asn1_write_algorithm_identifier(p, start, (const char *)llcrypto_oid_rsa_encryption,
			                                              sizeof(llcrypto_oid_rsa_encryption), (size_t)ret);
		}
	}
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */
#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
	/* cppcheck-suppress misra-c2012-10.4 ; mismatch is inside vendored macro PSA_KEY_TYPE_IS_ECC */
	if (PSA_KEY_TYPE_IS_ECC(type)) {
		/* cppcheck-suppress misra-c2012-10.4 ; mismatch is inside vendored macro PSA_KEY_TYPE_ECC_GET_FAMILY */
		const llcrypto_ec_curve *curve = llcrypto_ec_curve_from_psa(PSA_KEY_TYPE_ECC_GET_FAMILY(type), bits);
		if (NULL != curve) {
			ret = mbedtls_asn1_write_oid(p, start, (const char *)curve->oid, curve->oid_length);
			if (ret >= 0) {
				ret = mbedtls_asn1_write_algorithm_identifier(p, start, (const char *)llcrypto_oid_ec_public_key,
				                                              sizeof(llcrypto_oid_ec_public_key), (size_t)ret);
			}
		}
	}
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */
	/* Some parameters are unused when RSA or EC key types are compiled out. */
	LLCRYPTO_UNUSED_PARAM(p);
	LLCRYPTO_UNUSED_PARAM(start);
	LLCRYPTO_UNUSED_PARAM(type);
	LLCRYPTO_UNUSED_PARAM(bits);

	return ret;
}

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
/**
 * @brief Write a minimal ECPrivateKey body into a DER buffer, backward from `*p`.
 *
 * Emits {@code ECPrivateKey ::= SEQUENCE { version INTEGER(1), privateKey OCTET STRING }} with no
 * optional curve parameters or public key. The curve is carried by the enclosing privateKeyAlgorithm,
 * so this minimal form matches the PKCS#8 encoding produced by Java's reference implementation.
 *
 * @return Bytes written on success, or a negative mbedtls error code.
 */
static int llcrypto_asn1_write_ec_private_key(unsigned char **p, unsigned char *start, const uint8_t *scalar,
                                              size_t scalar_len) {
	int ret = 0;
	int32_t len = 0;

	LLCRYPTO_ASN1_CHK_ADD(len, ret, mbedtls_asn1_write_octet_string(p, start, scalar, scalar_len));
	LLCRYPTO_ASN1_CHK_ADD(len, ret, mbedtls_asn1_write_int(p, start, LLCRYPTO_EC_PRIVATE_KEY_VERSION));
	LLCRYPTO_ASN1_CHK_ADD(len, ret, mbedtls_asn1_write_len(p, start, (size_t)len));
	LLCRYPTO_ASN1_CHK_ADD(len, ret,
	                      mbedtls_asn1_write_tag(p, start, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));
	return (ret < 0) ? ret : len;
}

#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

/**
 * @brief Wrap an already-written privateKey body into a PKCS#8 PrivateKeyInfo, front-aligned.
 *
 * On entry `p` points at the start of the inner privateKey body (a PKCS#1 RSAPrivateKey or a minimal
 * ECPrivateKey) lying at the tail of {@code output}, and `len` is its length. Prepends the OCTET STRING
 * wrapper, the privateKeyAlgorithm of a key of the given type and size and the version, closes with the
 * enclosing SEQUENCE, then moves the result to the front of {@code output}.
 *
 * @return Total DER length on success, or {@code LLCRYPTO_ERROR} on failure.
 */
static int32_t llcrypto_pkcs8_wrap_private_key(uint8_t *output, unsigned char *p, int32_t len, psa_key_type_t type,
                                               size_t bits) {
	int32_t return_code = LLCRYPTO_ERROR;
	int ret = 0;
	/* Accumulate into a local copy; MISRA C:2012 17.8 forbids modifying the `len` parameter in place. */
	int32_t total = len;

	/* wrap as PrivateKeyInfo.privateKey OCTET STRING */
	LLCRYPTO_ASN1_CHK_ADD(total, ret, mbedtls_asn1_write_len(&p, output, (size_t)total));
	LLCRYPTO_ASN1_CHK_ADD(total, ret, mbedtls_asn1_write_tag(&p, output, MBEDTLS_ASN1_OCTET_STRING));
	/* privateKeyAlgorithm */
	LLCRYPTO_ASN1_CHK_ADD(total, ret, llcrypto_asn1_write_key_alg_id(&p, output, type, bits));
	/* version */
	LLCRYPTO_ASN1_CHK_ADD(total, ret, mbedtls_asn1_write_int(&p, output, LLCRYPTO_PKCS8_VERSION));
	/* enclosing SEQUENCE */
	LLCRYPTO_ASN1_CHK_ADD(total, ret, mbedtls_asn1_write_len(&p, output, (size_t)total));
	LLCRYPTO_ASN1_CHK_ADD(total, ret,
	                      mbedtls_asn1_write_tag(&p, output,
	                                             MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));

	if (ret >= 0) {
		(void)memmove(output, p, (size_t)total);
		return_code = total;
	} else {
		VEEPORT_LOGGER_LOG_DBG("pkcs8 encode failed: %d", ret);
	}
	return return_code;
}

#endif /* MBEDTLS_PK_WRITE_C */

/**
 * @brief Tell whether a parsed ASN.1 element is the OBJECT IDENTIFIER whose content octets are `expected`.
 */
static bool llcrypto_asn1_oid_equals(const mbedtls_asn1_buf *oid, const uint8_t *expected, size_t expected_length) {
	return (MBEDTLS_ASN1_OID == oid->tag) && (expected_length == oid->len) &&
	       (0 == memcmp(oid->p, expected, expected_length));
}

/**
 * @brief Import private key material into a volatile PSA key.
 *
 * The key gets the policy of a generated key, exportable only, so that {@code llcrypto_key_get_psa_id}
 * re-imports it with the usage and algorithm each operation asks for.
 *
 * @return {@code LLCRYPTO_SUCCESS}, {@code LLCRYPTO_ERROR_OOM} when PSA has no free key slot or memory left,
 *         or {@code LLCRYPTO_ERROR} when PSA rejects the key material.
 */
static int32_t llcrypto_psa_import_private_key(psa_key_type_t type, size_t bits, const uint8_t *data, size_t length,
                                               psa_key_id_t *id) {
	int32_t return_code = LLCRYPTO_SUCCESS;
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_type(&attributes, type);
	psa_set_key_bits(&attributes, bits);
	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_EXPORT);
	psa_set_key_algorithm(&attributes, PSA_ALG_NONE);
	psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_VOLATILE);

	psa_status_t status = psa_import_key(&attributes, data, length, id);
	if (PSA_SUCCESS != status) {
		VEEPORT_LOGGER_LOG_DBG("psa_import_key failed: %d", (int)status);
		return_code = llcrypto_sni_error_code(status);
	}

	psa_reset_key_attributes(&attributes);
	return return_code;
}

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
/**
 * @brief Import the SEC1 ECPrivateKey held by a PKCS#8 PrivateKeyInfo into PSA.
 *
 * {@code ECPrivateKey ::= SEQUENCE { version INTEGER(1), privateKey OCTET STRING, parameters [0] OPTIONAL,
 * publicKey [1] OPTIONAL }} (RFC 5915). PSA imports the privateKey scalar alone, on the curve named by the
 * privateKeyAlgorithm parameters. The optional fields that follow the scalar are ignored: the [0] parameters
 * repeat the curve, and PSA derives the [1] public key from the scalar when it is needed.
 *
 * @param curve_oid the privateKeyAlgorithm parameters, which must be a named-curve OID.
 * @param p         start of the ECPrivateKey DER.
 * @param end       end of the ECPrivateKey DER.
 * @param[out] id   the imported key.
 * @return {@code LLCRYPTO_SUCCESS}, {@code LLCRYPTO_ERROR_OOM} when PSA has no free key slot or memory left,
 *         or {@code LLCRYPTO_ERROR} when the key is malformed or its curve is not supported.
 */
static int32_t llcrypto_psa_import_ec_private_key(const mbedtls_asn1_buf *curve_oid, unsigned char *p,
                                                  const unsigned char *end, psa_key_id_t *id) {
	int32_t return_code = LLCRYPTO_ERROR;
	const llcrypto_ec_curve *curve = NULL;
	unsigned char *cursor = p;
	const unsigned char *sequence_end = end;
	size_t len = 0;
	int version = 0;

	if (MBEDTLS_ASN1_OID == curve_oid->tag) {
		curve = llcrypto_ec_curve_from_oid(curve_oid->p, curve_oid->len);
	}

	int rc = (NULL != curve) ? LLCRYPTO_MBEDTLS_SUCCESS : MBEDTLS_ERR_ASN1_INVALID_DATA;
	if (LLCRYPTO_MBEDTLS_SUCCESS == rc) {
		rc = mbedtls_asn1_get_tag(&cursor, end, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
	}
	if (LLCRYPTO_MBEDTLS_SUCCESS == rc) {
		sequence_end = &cursor[len];
		rc = mbedtls_asn1_get_int(&cursor, sequence_end, &version);
	}
	if ((LLCRYPTO_MBEDTLS_SUCCESS == rc) && (LLCRYPTO_EC_PRIVATE_KEY_VERSION != version)) {
		rc = MBEDTLS_ERR_ASN1_INVALID_DATA;
	}
	if (LLCRYPTO_MBEDTLS_SUCCESS == rc) {
		rc = mbedtls_asn1_get_tag(&cursor, sequence_end, &len, MBEDTLS_ASN1_OCTET_STRING);
	}
	if (LLCRYPTO_MBEDTLS_SUCCESS == rc) {
		/* cppcheck-suppress misra-c2012-10.4 ; mismatch is inside vendored macro PSA_KEY_TYPE_ECC_KEY_PAIR */
		return_code = llcrypto_psa_import_private_key(PSA_KEY_TYPE_ECC_KEY_PAIR(curve->family), curve->bits, cursor,
		                                              len, id);
	} else {
		VEEPORT_LOGGER_LOG_DBG("invalid EC private key or unsupported curve: %d", rc);
	}

	return return_code;
}

#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

int32_t llcrypto_psa_import_pkcs8(uint8_t *der, size_t der_length, psa_key_id_t *id) {
	int32_t return_code = LLCRYPTO_ERROR;
	unsigned char *p = der;
	const unsigned char *end = &der[der_length];
	mbedtls_asn1_buf algorithm;
	mbedtls_asn1_buf parameters;
	size_t len = 0;
	int version = -1;
	*id = PSA_KEY_ID_NULL;

	/* PrivateKeyInfo ::= SEQUENCE { version INTEGER(0), privateKeyAlgorithm AlgorithmIdentifier,
	 * privateKey OCTET STRING, attributes [0] OPTIONAL } (RFC 5208). Bytes after the SEQUENCE are ignored,
	 * as the Mbed TLS parser does. */
	int rc = mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
	if (LLCRYPTO_MBEDTLS_SUCCESS == rc) {
		end = &p[len];
		rc = mbedtls_asn1_get_int(&p, end, &version);
	}
	if ((LLCRYPTO_MBEDTLS_SUCCESS == rc) && (LLCRYPTO_PKCS8_VERSION != version)) {
		rc = MBEDTLS_ERR_ASN1_INVALID_DATA;
	}
	if (LLCRYPTO_MBEDTLS_SUCCESS == rc) {
		rc = mbedtls_asn1_get_alg(&p, end, &algorithm, &parameters);
	}
	if (LLCRYPTO_MBEDTLS_SUCCESS == rc) {
		rc = mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_OCTET_STRING);
	}

	if (LLCRYPTO_MBEDTLS_SUCCESS != rc) {
		VEEPORT_LOGGER_LOG_DBG("invalid PrivateKeyInfo: %d", rc);
	}
#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
	else if (llcrypto_asn1_oid_equals(&algorithm, llcrypto_oid_rsa_encryption, sizeof(llcrypto_oid_rsa_encryption))) {
		/* The privateKey of an RSA key is the PKCS#1 RSAPrivateKey, which PSA imports as is. */
		return_code = llcrypto_psa_import_private_key(PSA_KEY_TYPE_RSA_KEY_PAIR, 0u, p, len, id);
	}
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */
#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
	else if (llcrypto_asn1_oid_equals(&algorithm, llcrypto_oid_ec_public_key, sizeof(llcrypto_oid_ec_public_key))) {
		return_code = llcrypto_psa_import_ec_private_key(&parameters, p, &p[len], id);
	}
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */
	else {
		VEEPORT_LOGGER_LOG_DBG("unsupported privateKeyAlgorithm");
	}

	return return_code;
}

int32_t llcrypto_psa_export_pkcs8(psa_key_id_t psa_id, uint8_t *output, size_t output_length) {
#if defined(MBEDTLS_PK_WRITE_C)
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	int32_t return_code = LLCRYPTO_ERROR;

	if (PSA_SUCCESS == psa_get_key_attributes(psa_id, &attributes)) {
		psa_key_type_t type = psa_get_key_type(&attributes);
		/* PKCS#8 carries a private key, so only key-pair handles are exportable here. */
#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
		/* cppcheck-suppress misra-c2012-10.4 ; mismatch is inside vendored macros PSA_KEY_TYPE_IS_RSA/IS_KEY_PAIR */
		bool is_rsa = PSA_KEY_TYPE_IS_RSA(type) && PSA_KEY_TYPE_IS_KEY_PAIR(type);
#else
		/* RSA key types compiled out (PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY undefined): no RSA key to encode. */
		bool is_rsa = false;
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */
		/* cppcheck-suppress misra-c2012-10.4 ; mismatch is inside vendored macros PSA_KEY_TYPE_IS_ECC/IS_KEY_PAIR */
		bool is_ecc = PSA_KEY_TYPE_IS_ECC(type) && PSA_KEY_TYPE_IS_KEY_PAIR(type);

		/* cppcheck-suppress misra-c2012-12.1 ; precedence is inside vendored macro PSA_EXPORT_KEY_PAIR_MAX_SIZE */
		uint8_t raw[PSA_EXPORT_KEY_PAIR_MAX_SIZE];
		size_t raw_length = 0;
		psa_status_t status = psa_export_key(psa_id, raw, sizeof(raw), &raw_length);

		if ((PSA_SUCCESS == status) && (is_rsa || is_ecc)) {
			unsigned char *p = &output[output_length];
			int32_t len = 0;
			int ret = 0;

			/* privateKey field content: the PSA export of an RSA key pair is already the PKCS#1
			 * RSAPrivateKey; the export of an EC key pair is the raw private scalar. */
			if (is_rsa) {
				LLCRYPTO_ASN1_CHK_ADD(len, ret, mbedtls_asn1_write_raw_buffer(&p, output, raw, raw_length));
			} else {
#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
				ret = llcrypto_asn1_write_ec_private_key(&p, output, raw, raw_length);
				len = ret;
#else
				/* EC key types compiled out (PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY undefined): cannot encode
				 * an EC private key, report the type as unsupported. */
				VEEPORT_LOGGER_LOG_DBG("EC private key encoding not supported, "
				                       "PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY is disabled");
				ret = MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE;
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */
			}

			if (ret >= 0) {
				return_code = llcrypto_pkcs8_wrap_private_key(output, p, len, type, psa_get_key_bits(&attributes));
			} else {
				VEEPORT_LOGGER_LOG_DBG("pkcs8 encode failed: %d", ret);
			}
		} else {
			VEEPORT_LOGGER_LOG_DBG("psa_export_key failed (%d) or unsupported type 0x%x",
			                       (int)status, (unsigned int)type);
		}
		/* No ASLR on Cortex-M33/Zephyr: clear the exported private material from the stack. */
		mbedtls_platform_zeroize(raw, sizeof(raw));
	}
	psa_reset_key_attributes(&attributes);
	return return_code;
#else
	/* The DER writer (MBEDTLS_PK_WRITE_C / CONFIG_MBEDTLS_PK_WRITE_C) is disabled: PKCS#8 private-key
	* encoding is unavailable. The caller native turns the negative return into a native exception. */
	VEEPORT_LOGGER_LOG_DBG("PKCS#8 export unavailable, MBEDTLS_PK_WRITE_C is disabled");
	LLCRYPTO_UNUSED_PARAM(psa_id);
	LLCRYPTO_UNUSED_PARAM(output);
	LLCRYPTO_UNUSED_PARAM(output_length);
	return LLCRYPTO_ERROR;
#endif /* MBEDTLS_PK_WRITE_C */
}

int32_t llcrypto_pk_write_spki(const mbedtls_pk_context *pk, uint8_t *output, int32_t output_length) {
#if defined(MBEDTLS_PK_WRITE_C)
	int32_t return_code = LLCRYPTO_ERROR;

	int ret = mbedtls_pk_write_pubkey_der(pk, output, (size_t)output_length);
	if (ret > 0) {
		(void)memmove(output, &output[output_length - ret], (size_t)ret);
		return_code = ret;
	} else {
		VEEPORT_LOGGER_LOG_DBG("mbedtls_pk_write_pubkey_der failed: %d", ret);
	}

	return return_code;
#else
	/* The DER writer (MBEDTLS_PK_WRITE_C / CONFIG_MBEDTLS_PK_WRITE_C) is disabled: X.509 SPKI public-key
	 * encoding is unavailable. The caller native turns the negative return into a native exception. */
	VEEPORT_LOGGER_LOG_DBG("SPKI export unavailable, MBEDTLS_PK_WRITE_C is disabled");
	LLCRYPTO_UNUSED_PARAM(pk);
	LLCRYPTO_UNUSED_PARAM(output);
	LLCRYPTO_UNUSED_PARAM(output_length);
	return LLCRYPTO_ERROR;
#endif /* MBEDTLS_PK_WRITE_C */
}

int32_t llcrypto_psa_export_spki(psa_key_id_t psa_id, uint8_t *output, int32_t output_length) {
#if defined(MBEDTLS_PK_WRITE_C)
	int32_t return_code = LLCRYPTO_ERROR;
	mbedtls_pk_context pk;
	mbedtls_pk_init(&pk);

	int ret = mbedtls_pk_copy_public_from_psa(psa_id, &pk);
	if (LLCRYPTO_MBEDTLS_SUCCESS == ret) {
		return_code = llcrypto_pk_write_spki(&pk, output, output_length);
	} else {
		VEEPORT_LOGGER_LOG_DBG("mbedtls_pk_copy_public_from_psa failed: %d", ret);
	}

	mbedtls_pk_free(&pk);
	return return_code;
#else
	/* The DER writer (MBEDTLS_PK_WRITE_C / CONFIG_MBEDTLS_PK_WRITE_C) is disabled: X.509 SPKI public-key
	 * encoding is unavailable. The caller native turns the negative return into a native exception. */
	VEEPORT_LOGGER_LOG_DBG("SPKI export unavailable, MBEDTLS_PK_WRITE_C is disabled");
	LLCRYPTO_UNUSED_PARAM(psa_id);
	LLCRYPTO_UNUSED_PARAM(output);
	LLCRYPTO_UNUSED_PARAM(output_length);
	return LLCRYPTO_ERROR;
#endif /* MBEDTLS_PK_WRITE_C */
}
