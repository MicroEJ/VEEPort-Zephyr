/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#ifndef LLCRYPTO_KEY_H
#define LLCRYPTO_KEY_H

#include <stdint.h>
#include <stdbool.h>
#include <psa/crypto.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>

/**
 * This enum defines the type of data that represents a key.
 */
enum llcrypto_key_backend {
	LLCRYPTO_KEY_BACKEND_PSA, /**< The key is backed by a PSA managed key */
	LLCRYPTO_KEY_BACKEND_MBEDTLS_PK, /**< The key is backed by an MbedTLS PK object, which can be imported into PSA */
	LLCRYPTO_KEY_BACKEND_MBEDTLS_X509, /**< Public key backed by a x509 certificate */
	LLCRYPTO_KEY_BACKEND_SECRET_KEY /**< RAW Secret Key (ex: AES) */
};

struct llcrypto_secret_key {
	uint8_t *buffer; /**< Buffer holding the secret key raw data. Owned by this struct; allocated by
	                  *   {@code llcrypto_key_from_secret_key()}, freed by {@code llcrypto_key_free()}. */
	size_t length;   /**< Length of the buffer, in bytes */
	psa_key_type_t type; /**< PSA key type, ex: {@code PSA_KEY_TYPE_AES} */
};

/**
 * This structure represents a key that can be backed by multiple backends.
 */
struct llcrypto_key {
	enum llcrypto_key_backend backend; /**< Backend that holds the key */
	/* cppcheck-suppress misra-c2012-19.2 ; intentional tagged union discriminated by `backend`; the backends are
	 * mutually exclusive per key, so a struct holding all of them would waste RAM. No type-punning: each member is
	 * read only under its matching `backend` value. */
	union {
		/** PSA identifier valid when backend is LLCRYPTO_KEY_BACKEND_PSA */
		psa_key_id_t psa_id;
		/** MbedTLS asymmetric key context valid when backend is LLCRYPTO_KEY_BACKEND_MBEDTLS_PK */
		mbedtls_pk_context *mbedtls_pk;
		/** MbedTLS certificate context valid when backend is LLCRYPTO_KEY_BACKEND_MBEDTLS_X509 */
		mbedtls_x509_crt *mbedtls_crt;
		/** RAW Secret Key valide when backend is LLCRYPTO_KEY_BACKEND_SECRET_KEY */
		struct llcrypto_secret_key secret_key;
	};
};

/**
 * @brief Release a key and free its heap allocation.
 *
 * Pure teardown: releases the backend resources (mbedTLS contexts, volatile PSA keys), then frees
 * the {@code struct llcrypto_key} allocation. It never touches the SNI resource table: it is the close
 * function registered by {@code llcrypto_key_new}, so the VM invokes it after having removed the
 * registration itself. Internal callers releasing a key that is still registered must call
 * {@code SNI_unregisterResource} first.
 * The pointer is invalid after this call.
 *
 * @invariant This function MUST be called from the core engine task.
 *
 * @param key Key to be freed
 */
void llcrypto_key_free(struct llcrypto_key *key);

/**
 * @brief Instantiate a new key from parsing an encoded public key
 *
 * Parse a public key encoded as a ASN.1 `SubjectPublicKeyInfo`. The type is DER encoded, then optionally Base64 armored
 * inside the PEM header/footer.
 *
 * @invariant This function MUST be called from the core engine task.
 *
 * @param encoded_key Buffer containing the encoded key.
 * @param key_length Length of the buffer pointed to by `encoded_key`. If encoded_key contains a string (PEM encoded),
 * `key_length` must include the terminating NULL byte.
 *
 * @return struct llcrypto_key* A heap allocated key
 */
struct llcrypto_key *llcrypto_key_from_x509_public_key(uint8_t *encoded_key, size_t key_length);

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
/**
 * @brief Instantiate a new EC public key from a curve and an uncompressed point.
 *
 * Imports the SEC1 uncompressed point encoding (`0x04 || X || Y`) into PSA as a
 * {@code PSA_KEY_TYPE_ECC_PUBLIC_KEY} of the given family and size. PSA validates that the point lies
 * on the curve. The key carries the export-only policy of a generated key: it is re-imported with the
 * requested usage and algorithm at use time by {@code llcrypto_key_get_psa_id}.
 *
 * @invariant This function MUST be called from the core engine task.
 *
 * @param family       The PSA elliptic-curve family, e.g. {@code PSA_ECC_FAMILY_SECP_R1}.
 * @param bits         The curve size in bits, e.g. 256 for secp256r1.
 * @param point        The uncompressed point (`0x04 || X || Y`) with field-width coordinates.
 * @param point_length Length of `point`, in bytes.
 * @param[out] key_out  A heap allocated key on success, NULL on failure.
 *
 * @return `LLCRYPTO_SUCCESS`, `LLCRYPTO_ERROR_OOM` when the key cannot be allocated or PSA has no free key
 *         slot or memory left, or `LLCRYPTO_ERROR` on any other failure (such as a point off the curve).
 */
int32_t llcrypto_key_from_ec_public_key(psa_ecc_family_t family, size_t bits, const uint8_t *point,
                                        size_t point_length, struct llcrypto_key **key_out);
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

/**
 * @brief Instantiate a new key from parsing an encoded private key.
 *
 * Parse a private key encoded as a DER PKCS#8 `PrivateKeyInfo`, the encoding of a `PKCS8EncodedKeySpec`, and
 * import it into PSA. The key carries the export-only policy of a generated key: it is re-imported with the
 * requested usage and algorithm at use time by {@code llcrypto_key_get_psa_id}.
 *
 * @invariant This function MUST be called from the core engine task.
 *
 * @param encoded_key Buffer containing the encoded key.
 * @param key_length Length of the buffer pointed to by `encoded_key`.
 *
 * @param[out] key_out A heap allocated key on success, NULL on failure.
 *
 * @return `LLCRYPTO_SUCCESS`, `LLCRYPTO_ERROR_OOM` when the key cannot be allocated or PSA has no free key
 *         slot or memory left for it, or `LLCRYPTO_ERROR` when the encoded key is invalid.
 */
int32_t llcrypto_key_from_pkcs8_private_key(uint8_t *encoded_key, size_t key_length, struct llcrypto_key **key_out);

/**
 * @brief Instanciate a new (public) key from a X509 certificate
 *
 * Parse a X509 DER encoded certificate, optionally Base64 armored inside the PEM header/footer.
 *
 * @invariant This function MUST be called from the core engine task.
 *
 * @param encoded_cert Buffer to the certificate, DER or PEM encoded
 * @param encoded_length Length of the buffer, including the terminating NULL character if the certificate is PEM
 * encoded.
 * @return struct llcrypto_key* A heap allocated key, NULL on failure.
 */
struct llcrypto_key *llcrypto_key_from_x509_certificate(uint8_t *encoded_cert, size_t encoded_length);

/**
 * @brief Instanciate a new secret key from a raw symmetric key.
 *
 * @invariant This function MUST be called from the core engine task.
 *
 * @param secret_key Buffer to the secret key
 * @param key_length Length of the buffer
 * @param algorithm Null-terminated ASCII string representing the key algorithm in its Managed Code definition,
 *                  which corresponds to PSA key type.
 * @return struct llcrypto_key* A heap allocated key, NULL on failure.
 */
struct llcrypto_key *llcrypto_key_from_secret_key(uint8_t *secret_key, size_t key_length, uint8_t *algorithm);

/**
 * @brief Instanciate a new key from a PSA Key ID
 *
 * @invariant This function MUST be called from the core engine task.
 *
 * @param psa_id PSA key identifier to use the key without retrieving its value
 * @return struct llcrypto_key* A heap allocated key, NULL on failure.
 */
struct llcrypto_key *llcrypto_key_from_psa_id(psa_key_id_t psa_id);

/**
 * @brief Get the maximum size in bytes of the exported key material.
 *
 * @param key Key object
 * @return int32_t Size in bytes, or 0 on failure or unsupported key type
 */
int32_t llcrypto_key_get_encoded_max_size(const struct llcrypto_key *key);

/**
 * @brief Get the size in bytes of an asymmetric cryptographic operation output with this key.
 *
 * For an RSA key this is the modulus size in bytes (e.g. 128 for 1024-bit, 256 for 2048-bit); it is
 * derived from the key bit length (PSA backend) or from {@code mbedtls_pk_get_bitlen()} (mbedtls backends).
 *
 * @param key Key object
 * @return int32_t Output size in bytes, or 0 on failure or unsupported key type
 */
int32_t llcrypto_key_get_output_size(const struct llcrypto_key *key);

/**
 * @brief Export the raw key bytes into a caller-supplied buffer.
 *
 * @param key Key object
 * @param output Output buffer
 * @param output_length Size of the output buffer in bytes
 * @return int32_t Number of bytes written on success, 0 on any failure
 */
int32_t llcrypto_key_get_encoded(const struct llcrypto_key *key, uint8_t *output, int32_t output_length);

/**
 * @brief Export a private key as a PKCS#8 `PrivateKeyInfo` DER structure.
 *
 * Dispatches on the key backend: a `LLCRYPTO_KEY_BACKEND_PSA` key is exported with `psa_export_key` and
 * wrapped in the PKCS#8 envelope; a `LLCRYPTO_KEY_BACKEND_MBEDTLS_PK` key is imported into PSA and then
 * exported through the same path (TF-PSA-Crypto exposes no accessor to the raw private scalar of a PK
 * context).
 *
 * @param key Key object
 * @param[out] output Output buffer; must be at least `LLCRYPTO_PRIVATE_KEY_MAX_PKCS8_LEN` bytes
 * @param output_length Capacity of `output` in bytes
 * @return int32_t Number of bytes written on success, `LLCRYPTO_ERROR_OOM` when a
 * `LLCRYPTO_KEY_BACKEND_MBEDTLS_PK` key cannot be imported into PSA for lack of a free key slot or of
 * memory, or `LLCRYPTO_ERROR` on failure / unsupported backend
 */
int32_t llcrypto_key_export_pkcs8(const struct llcrypto_key *key, uint8_t *output, size_t output_length);

/**
 * @brief Export a public key as an X.509 `SubjectPublicKeyInfo` DER structure.
 *
 * Dispatches on the key backend: a `LLCRYPTO_KEY_BACKEND_PSA` key (key pair or public key) has its
 * public material copied into a PK context with `mbedtls_pk_copy_public_from_psa`;
 * `LLCRYPTO_KEY_BACKEND_MBEDTLS_PK` and `LLCRYPTO_KEY_BACKEND_MBEDTLS_X509` keys already hold one. All
 * three are then serialized with `mbedtls_pk_write_pubkey_der`.
 *
 * @param key Key object
 * @param[out] output Output buffer; must be at least `LLCRYPTO_PUBLIC_KEY_MAX_DER_LEN` bytes
 * @param output_length Capacity of `output` in bytes
 * @return int32_t Number of bytes written on success, or `LLCRYPTO_ERROR` on failure / unsupported backend
 */
int32_t llcrypto_key_export_spki(const struct llcrypto_key *key, uint8_t *output, int32_t output_length);

/**
 * @brief A pointer to a function to free a PSA key identifier obtained from `llcrypto_key_get_psa_id()`
 */
typedef psa_status_t (*llcrypto_key_close_with_psa)(psa_key_id_t);

/**
 * @brief Get a PSA identifier from a `struct llcrypto_key` instance
 *
 * If the key type is `LLCRYPTO_KEY_BACKEND_PSA`, and the usage and attributes requested match, this function returns
 * the
 * underlying PSA key identifier, otherwise it is re-imported into PSA and a new PSA key identifier is generated.
 * If the key type is not `LLCRYPTO_KEY_BACKEND_PSA`, the key is imported in PSA with the given usage.
 *
 * @warning The caller MUST call {@code (*key_close)(*id)} exactly once when the PSA key is no
 * longer needed, regardless of the input {@code key} type. This holds even when the function fails:
 * {@code *id} is then {@code PSA_KEY_ID_NULL} and {@code key_close} is set to a no-op that safely accepts
 * it, so callers do not need to special-case the failure path.
 *
 * @param key Key object from which a PSA key identifier must be obtained
 * @param requested_usage Requested usage for the returned PSA key
 * @param requested_algorithm Requested algorithm of the returned PSA key. Passing {@code PSA_ALG_NONE}
 * disables the export/re-import path and is only valid for {@code LLCRYPTO_KEY_BACKEND_MBEDTLS_PK} and
 * {@code LLCRYPTO_KEY_BACKEND_MBEDTLS_X509} keys, where PSA infers the algorithm from the key itself.
 * @param[out] id The PSA key, to be freed with {@code (*key_close)()}, or {@code PSA_KEY_ID_NULL} on failure
 * @param[out] key_close A pointer to a function to be called once the PSA key is no longer needed
 * @return {@code LLCRYPTO_SUCCESS}, {@code LLCRYPTO_ERROR_OOM} when the key must be imported and PSA has
 *         no free key slot or memory left, or {@code LLCRYPTO_ERROR} on any other failure
 */
int32_t llcrypto_key_get_psa_id(struct llcrypto_key *key, psa_key_usage_t requested_usage,
                                psa_algorithm_t requested_algorithm, psa_key_id_t *id,
                                llcrypto_key_close_with_psa *key_close);

/**
 * @brief Return the category of a key.
 *
 * Only supports keys backed by PSA. For other backends the call fails.
 *
 * @param key Key object
 * @return One of {@code LLCRYPTO_KEY_TYPE_PRIVATE} / {@code LLCRYPTO_KEY_TYPE_PUBLIC} / {@code
 * LLCRYPTO_KEY_TYPE_SECRET}
 *         (from {@code LLCRYPTO_CONSTANTS.h}), or {@code LLCRYPTO_ERROR} on failure.
 */
int32_t llcrypto_key_get_type(const struct llcrypto_key *key);

/**
 * @brief Write the algorithm name of a key into a caller-provided buffer.
 *
 * Only supports keys backed by PSA. For other backends the call fails.
 *
 * @param key Key object
 * @param[out] out Output buffer for the null-terminated algorithm string
 * @param out_len Capacity of {@code out} in bytes
 * @return {@code LLCRYPTO_SUCCESS} on success, {@code LLCRYPTO_ERROR} on unsupported / unknown key
 */
int32_t llcrypto_key_get_algorithm_name(const struct llcrypto_key *key, char *out, size_t out_len);

#endif /* LLCRYPTO_KEY_H */
