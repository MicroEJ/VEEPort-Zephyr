/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#ifndef LLCRYPTO_KEY_PSA_H
#define LLCRYPTO_KEY_PSA_H

#include <stdint.h>
#include <psa/crypto.h>
#include <mbedtls/pk.h>

#include "LLCRYPTO_key.h"

/**
 * @brief Convert a Java SecretKeyFactory algorithm name into a PSA key type.
 *
 * Maps the Managed Code algorithm string (see
 * https://docs.oracle.com/javase/8/docs/technotes/guides/security/StandardNames.html) to its PSA key
 * type. Only {@code AES} is supported: TF-PSA-Crypto 1.x removed DES, so {@code DESede} is rejected.
 *
 * @param algorithm Null-terminated ASCII algorithm name; may be NULL.
 * @return The PSA key type, or {@code PSA_KEY_TYPE_NONE} when the algorithm is unsupported or NULL.
 */
psa_key_type_t llcrypto_psa_type_from_java_algorithm(uint8_t *algorithm);

/**
 * @brief Import a parsed mbedtls private/public key into a volatile PSA key.
 *
 * Derives the PSA attributes from the PK context with {@code mbedtls_pk_get_psa_attributes()}, forces a
 * volatile lifetime, and imports the key with {@code mbedtls_pk_import_into_psa()}.
 *
 * {@code mbedtls_pk_get_psa_attributes()} derives one algorithm policy from the usage alone (PKCS#1 v1.5
 * for an RSA encrypt/decrypt usage, ECDSA/ECDH with any hash for an EC key). A caller that needs another
 * algorithm on the same key, such as RSA OAEP or RSA PSS, passes it so that the imported key is permitted
 * to run it.
 *
 * @param pk Parsed mbedtls key context.
 * @param usage Requested PSA usage flags for the imported key.
 * @param algorithm Requested PSA algorithm policy, or {@code PSA_ALG_NONE} to keep the one
 *        {@code mbedtls_pk_get_psa_attributes()} derives from the usage.
 * @param[out] id The imported volatile PSA key identifier, or {@code PSA_KEY_ID_NULL} on failure.
 * @return {@code LLCRYPTO_SUCCESS}, {@code LLCRYPTO_ERROR_OOM} when PSA has no free key slot or memory
 *         left, or {@code LLCRYPTO_ERROR} on any other failure.
 */
int32_t llcrypto_psa_from_mbedtls_pk(const mbedtls_pk_context *pk, psa_key_usage_t usage,
                                     psa_algorithm_t algorithm, psa_key_id_t *id);

/**
 * @brief Import a raw symmetric secret key into a volatile PSA key.
 *
 * @param secret_key Secret key descriptor (buffer, length and PSA key type).
 * @param usage Requested PSA usage flags for the imported key.
 * @param algorithm Requested PSA algorithm policy for the imported key.
 * @param[out] id The imported volatile PSA key identifier, or {@code PSA_KEY_ID_NULL} on failure.
 * @return {@code LLCRYPTO_SUCCESS}, {@code LLCRYPTO_ERROR_OOM} when PSA has no free key slot or memory
 *         left, or {@code LLCRYPTO_ERROR} on any other failure.
 */
int32_t llcrypto_psa_from_secret_key(struct llcrypto_secret_key secret_key, psa_key_usage_t usage,
                                     psa_algorithm_t algorithm, psa_key_id_t *id);

/**
 * @brief Re-import an exportable PSA key as a new volatile key with the requested usage and algorithm.
 *
 * When a PSA key is not provisioned with the usage/algorithm a caller needs but is exportable, its raw
 * material is exported and re-imported under a fresh volatile key carrying the requested policy while
 * keeping the original key type and bit length. The exported material is zeroized from the stack before
 * returning (no ASLR on Cortex-M33/Zephyr).
 *
 * @param src Source PSA key identifier; must carry {@code PSA_KEY_USAGE_EXPORT}.
 * @param usage Requested PSA usage flags for the new key.
 * @param algorithm Requested PSA algorithm policy for the new key.
 * @param[out] id The new volatile PSA key identifier, or {@code PSA_KEY_ID_NULL} on failure.
 * @return {@code LLCRYPTO_SUCCESS}, {@code LLCRYPTO_ERROR_OOM} when PSA has no free key slot or memory
 *         left, or {@code LLCRYPTO_ERROR} on any other failure.
 */
int32_t llcrypto_psa_reimport_with_attributes(psa_key_id_t src, psa_key_usage_t usage,
                                              psa_algorithm_t algorithm, psa_key_id_t *id);

#endif /* LLCRYPTO_KEY_PSA_H */
