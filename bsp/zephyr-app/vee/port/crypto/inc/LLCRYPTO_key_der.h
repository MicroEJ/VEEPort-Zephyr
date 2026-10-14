/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#ifndef LLCRYPTO_KEY_DER_H
#define LLCRYPTO_KEY_DER_H

#include <stdint.h>
#include <psa/crypto.h>
#include <mbedtls/pk.h>

/**
 * @brief Export a PSA private key as a PKCS#8 PrivateKeyInfo DER structure, front-aligned.
 *
 * PSA has no DER-structure export, so the raw key material is obtained with {@code psa_export_key()}
 * and wrapped here: for RSA the export is already a PKCS#1 RSAPrivateKey and is embedded as-is; for EC
 * the exported scalar is wrapped in a minimal {@code ECPrivateKey}. The privateKeyAlgorithm is written from
 * the key type and size: rsaEncryption, or id-ecPublicKey with the named-curve OID of the curve table. The
 * encoded structure is written from the end of {@code output} then moved to the front.
 *
 * @param psa_id PSA key identifier of an exportable key-pair handle.
 * @param[out] output Output buffer; the encoded structure is moved to its front on success.
 * @param output_length Capacity of {@code output} in bytes.
 * @return Length of the DER structure on success, or {@code LLCRYPTO_ERROR} on failure.
 */
int32_t llcrypto_psa_export_pkcs8(psa_key_id_t psa_id, uint8_t *output, size_t output_length);

/**
 * @brief Import a PKCS#8 PrivateKeyInfo DER structure into a volatile PSA key.
 *
 * The PrivateKeyInfo is unwrapped with the ASN.1 reader and the key material is passed to
 * {@code psa_import_key()}, so that the PSA status of the import is reported as is. RSA (rsaEncryption) and,
 * when EC key types are enabled, EC (id-ecPublicKey on a named curve enabled in PSA) keys are accepted. The
 * key is exportable only, the policy of a generated key.
 *
 * @param der        the DER-encoded PrivateKeyInfo.
 * @param der_length length of {@code der}, in bytes.
 * @param[out] id    the imported key on success, {@code PSA_KEY_ID_NULL} on failure.
 * @return {@code LLCRYPTO_SUCCESS}, {@code LLCRYPTO_ERROR_OOM} when PSA has no free key slot or memory left,
 *         or {@code LLCRYPTO_ERROR} when the key is malformed or not supported.
 */
int32_t llcrypto_psa_import_pkcs8(uint8_t *der, size_t der_length, psa_key_id_t *id);

/**
 * @brief Write the SubjectPublicKeyInfo DER of a parsed mbedtls public key, front-aligned.
 *
 * mbedtls already produces the full X.509 SPKI envelope directly from the PK context, so a parsed key
 * (MBEDTLS_PK / X509 backend) does not need the PSA import/export round-trip. The writer fills the
 * buffer from its end; the result is moved to the front to match the LLAPI contract.
 *
 * @param pk Parsed mbedtls public key context.
 * @param[out] output Output buffer; the encoded structure is moved to its front on success.
 * @param output_length Capacity of {@code output} in bytes.
 * @return Length of the DER structure on success, or {@code LLCRYPTO_ERROR} on failure.
 */
int32_t llcrypto_pk_write_spki(const mbedtls_pk_context *pk, uint8_t *output, int32_t output_length);

/**
 * @brief Export a PSA key as an X.509 SubjectPublicKeyInfo DER structure.
 *
 * The public material of the PSA key is copied into a transient {@code mbedtls_pk_context} with
 * {@code mbedtls_pk_copy_public_from_psa()}, then serialized through the same
 * {@code mbedtls_pk_write_pubkey_der()} writer used for parsed keys. This unifies all three SPKI
 * backends behind one writer instead of a hand-rolled ASN.1 encoder. The copy accepts key pairs and
 * public keys alike and needs no export permission on the PSA key.
 *
 * @param psa_id PSA key identifier of an RSA or EC key pair or public key.
 * @param[out] output Output buffer; the encoded structure is moved to its front on success.
 * @param output_length Capacity of {@code output} in bytes.
 * @return Length of the DER structure on success, or {@code LLCRYPTO_ERROR} on failure.
 */
int32_t llcrypto_psa_export_spki(psa_key_id_t psa_id, uint8_t *output, int32_t output_length);

#endif /* LLCRYPTO_KEY_DER_H */
