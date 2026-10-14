/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#ifndef LLCRYPTO_EC_CURVE_H
#define LLCRYPTO_EC_CURVE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <psa/crypto.h>

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)

/**
 * @brief A named elliptic curve supported by the port.
 *
 * The table of these curves is the only place that lists them: the key factory and the key pair generator look
 * a curve up by its Java standard name, and the PKCS#8 parser and writer by its OID or its PSA family and size.
 * An entry exists only when its curve is enabled in PSA ({@code PSA_WANT_ECC_*}), so the table follows the
 * board's Kconfig.
 */
typedef struct {
	const char *name;        /**< Java standard name, e.g. "secp256r1" */
	const uint8_t *oid;      /**< Content octets of the named-curve OID (RFC 5480), without tag and length */
	size_t oid_length;       /**< Length of {@code oid}, in bytes */
	psa_ecc_family_t family; /**< PSA curve family, e.g. {@code PSA_ECC_FAMILY_SECP_R1} */
	size_t bits;             /**< Key size in bits, e.g. 256 */
} llcrypto_ec_curve;

/**
 * @brief Find a curve by its Java standard name.
 *
 * @param name the null-terminated curve name.
 * @return the curve, or NULL when the curve is not supported.
 */
const llcrypto_ec_curve *llcrypto_ec_curve_from_name(const char *name);

/**
 * @brief Find a curve by the content octets of its named-curve OID.
 *
 * @param oid        the OID content octets.
 * @param oid_length length of {@code oid}, in bytes.
 * @return the curve, or NULL when the curve is not supported.
 */
const llcrypto_ec_curve *llcrypto_ec_curve_from_oid(const uint8_t *oid, size_t oid_length);

/**
 * @brief Find a curve by its PSA family and key size.
 *
 * @param family the PSA curve family.
 * @param bits   the key size in bits.
 * @return the curve, or NULL when the curve is not supported.
 */
const llcrypto_ec_curve *llcrypto_ec_curve_from_psa(psa_ecc_family_t family, size_t bits);

#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

#ifdef __cplusplus
}
#endif

#endif /* LLCRYPTO_EC_CURVE_H */
