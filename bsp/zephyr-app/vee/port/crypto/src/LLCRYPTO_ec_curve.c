/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "LLCRYPTO_ec_curve.h"

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)

/* Content octets of the named-curve OIDs (RFC 5480). TF-PSA-Crypto keeps its OID tables private, so the port
 * holds the bytes of the curves it supports. */
#if defined(PSA_WANT_ECC_SECP_R1_192)
/* secp192r1, 1.2.840.10045.3.1.1 */
static const uint8_t llcrypto_oid_secp192r1[] = { 0x2Au, 0x86u, 0x48u, 0xCEu, 0x3Du, 0x03u, 0x01u, 0x01u };
#endif
#if defined(PSA_WANT_ECC_SECP_R1_224)
/* secp224r1, 1.3.132.0.33 */
static const uint8_t llcrypto_oid_secp224r1[] = { 0x2Bu, 0x81u, 0x04u, 0x00u, 0x21u };
#endif
#if defined(PSA_WANT_ECC_SECP_R1_256)
/* secp256r1, 1.2.840.10045.3.1.7 */
static const uint8_t llcrypto_oid_secp256r1[] = { 0x2Au, 0x86u, 0x48u, 0xCEu, 0x3Du, 0x03u, 0x01u, 0x07u };
#endif
#if defined(PSA_WANT_ECC_SECP_R1_384)
/* secp384r1, 1.3.132.0.34 */
static const uint8_t llcrypto_oid_secp384r1[] = { 0x2Bu, 0x81u, 0x04u, 0x00u, 0x22u };
#endif
#if defined(PSA_WANT_ECC_SECP_R1_521)
/* secp521r1, 1.3.132.0.35 */
static const uint8_t llcrypto_oid_secp521r1[] = { 0x2Bu, 0x81u, 0x04u, 0x00u, 0x23u };
#endif
#if defined(PSA_WANT_ECC_SECP_K1_256)
/* secp256k1, 1.3.132.0.10 */
static const uint8_t llcrypto_oid_secp256k1[] = { 0x2Bu, 0x81u, 0x04u, 0x00u, 0x0Au };
#endif

static const llcrypto_ec_curve llcrypto_ec_curves[] = {
#if defined(PSA_WANT_ECC_SECP_R1_192)
	{ "secp192r1", llcrypto_oid_secp192r1, sizeof(llcrypto_oid_secp192r1), PSA_ECC_FAMILY_SECP_R1, 192u },
#endif
#if defined(PSA_WANT_ECC_SECP_R1_224)
	{ "secp224r1", llcrypto_oid_secp224r1, sizeof(llcrypto_oid_secp224r1), PSA_ECC_FAMILY_SECP_R1, 224u },
#endif
#if defined(PSA_WANT_ECC_SECP_R1_256)
	{ "secp256r1", llcrypto_oid_secp256r1, sizeof(llcrypto_oid_secp256r1), PSA_ECC_FAMILY_SECP_R1, 256u },
#endif
#if defined(PSA_WANT_ECC_SECP_R1_384)
	{ "secp384r1", llcrypto_oid_secp384r1, sizeof(llcrypto_oid_secp384r1), PSA_ECC_FAMILY_SECP_R1, 384u },
#endif
#if defined(PSA_WANT_ECC_SECP_R1_521)
	{ "secp521r1", llcrypto_oid_secp521r1, sizeof(llcrypto_oid_secp521r1), PSA_ECC_FAMILY_SECP_R1, 521u },
#endif
#if defined(PSA_WANT_ECC_SECP_K1_256)
	{ "secp256k1", llcrypto_oid_secp256k1, sizeof(llcrypto_oid_secp256k1), PSA_ECC_FAMILY_SECP_K1, 256u },
#endif
};

#define LLCRYPTO_EC_CURVE_COUNT (sizeof(llcrypto_ec_curves) / sizeof(llcrypto_ec_curves[0]))

const llcrypto_ec_curve *llcrypto_ec_curve_from_name(const char *name) {
	const llcrypto_ec_curve *curve = NULL;
	for (size_t i = 0; i < LLCRYPTO_EC_CURVE_COUNT; i++) {
		if (0 == strcmp(name, llcrypto_ec_curves[i].name)) {
			curve = &llcrypto_ec_curves[i];
			break;
		}
	}
	return curve;
}

const llcrypto_ec_curve *llcrypto_ec_curve_from_oid(const uint8_t *oid, size_t oid_length) {
	const llcrypto_ec_curve *curve = NULL;
	for (size_t i = 0; i < LLCRYPTO_EC_CURVE_COUNT; i++) {
		if ((oid_length == llcrypto_ec_curves[i].oid_length) &&
		    (0 == memcmp(oid, llcrypto_ec_curves[i].oid, oid_length))) {
			curve = &llcrypto_ec_curves[i];
			break;
		}
	}
	return curve;
}

const llcrypto_ec_curve *llcrypto_ec_curve_from_psa(psa_ecc_family_t family, size_t bits) {
	const llcrypto_ec_curve *curve = NULL;
	for (size_t i = 0; i < LLCRYPTO_EC_CURVE_COUNT; i++) {
		if ((family == llcrypto_ec_curves[i].family) && (bits == llcrypto_ec_curves[i].bits)) {
			curve = &llcrypto_ec_curves[i];
			break;
		}
	}
	return curve;
}

#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */
