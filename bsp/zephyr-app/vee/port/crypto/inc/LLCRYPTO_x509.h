/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#ifndef LLCRYPTO_X509_H
#define LLCRYPTO_X509_H

#include <stddef.h>
#include <stdint.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

/**
 * @defgroup llcrypto_x509_verify_results Results of llcrypto_x509_verify_signature
 *
 * Port-local codes, deliberately distinct from the LLAPI ones: each native maps them to its own
 * LLAPI contract (the LLCRYPTO_X509_*_EXCEPTION return codes of LLCRYPTO_X509_CERT_impl.h, or the
 * LLCRYPTO_ERROR / LLCRYPTO_ERROR_OOM return codes of LLCRYPTO_CERT_PATH_VALIDATOR_impl.h). Keeping
 * them local lets one helper serve both natives and report an out-of-memory as a plain return code.
 * @{
 */
/** The signature is valid. */
#define LLCRYPTO_X509_VERIFY_OK                (0)
/** The signature does not match, or the PSA verification failed for any other unexpected reason. */
#define LLCRYPTO_X509_VERIFY_BAD_SIGNATURE     (-1)
/** The signature value is malformed. */
#define LLCRYPTO_X509_VERIFY_BAD_ENCODING      (-2)
/** The signature algorithm or its digest is not available in this build. */
#define LLCRYPTO_X509_VERIFY_NO_SUCH_ALGORITHM (-3)
/** The key attributes cannot be read, so the key is unusable. */
#define LLCRYPTO_X509_VERIFY_INVALID_KEY       (-4)
/** The verification ran out of memory. */
#define LLCRYPTO_X509_VERIFY_OUT_OF_MEMORY     (-5)
/** @} */

/**
 * @brief Parse a DER- or PEM-encoded X.509 certificate into an already-initialized context.
 *
 * {@code mbedtls_x509_crt_parse} only attempts its PEM branch when the buffer is a NUL-terminated
 * string whose terminator is counted in the length. The Java layer does not reliably include that
 * terminator, so this helper tries DER first (exact length, terminator-agnostic) and, on failure,
 * retries PEM from a NUL-terminated copy. Use it instead of calling {@code mbedtls_x509_crt_parse}
 * directly on caller-supplied certificate bytes.
 *
 * @param crt An initialized {@code mbedtls_x509_crt} to parse into.
 * @param cert_data Buffer holding the certificate, DER or PEM encoded.
 * @param cert_data_length Length of {@code cert_data}.
 * @return {@code LLCRYPTO_MBEDTLS_SUCCESS} (0) on success, or a negative mbedtls error code.
 */
int32_t llcrypto_x509_crt_parse(mbedtls_x509_crt *crt, const uint8_t *cert_data, size_t cert_data_length);

/**
 * @brief Verify the signature of a parsed certificate against a PSA public key.
 *
 * @param[in] crt A parsed certificate.
 * @param[in] issuer_key A PSA key with {@code PSA_KEY_USAGE_VERIFY_MESSAGE} usage, typically the issuer's public key.
 * @return {@code LLCRYPTO_X509_VERIFY_OK} when the signature is valid, otherwise one of the other
 *         @ref llcrypto_x509_verify_results codes.
 */
int32_t llcrypto_x509_verify_signature(const mbedtls_x509_crt *crt, psa_key_id_t issuer_key);

#endif // LLCRYPTO_X509_H
