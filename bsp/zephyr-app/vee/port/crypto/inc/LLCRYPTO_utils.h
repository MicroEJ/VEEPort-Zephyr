/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#ifndef LLCRYPTO_UTILS_H
#define LLCRYPTO_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <psa/crypto_types.h>

#include "sni.h"

/**
 * @brief Unregisters an SNI native resource then closes it.
 *
 * On unregister failure (the resource is not in the SNI table — it was already closed or the VM
 * auto-closed it) a NativeException is thrown and @p close is NOT invoked, because the resource is
 * no longer owned by this path and freeing it would be a double-free. On success @p close is
 * invoked exactly once.
 *
 * This factors out the resource teardown boilerplate shared verbatim by every LLCRYPTO *_IMPL_close
 * native (DIGEST, MAC, CIPHER, RSA_CIPHER, AES_KEY_WRAP, RANDOM).
 *
 * @param[in] resource the native resource handle previously registered with SNI_registerResource.
 * @param[in] close    the close function registered alongside @p resource.
 */
void llcrypto_unregister_and_close(void *resource, SNI_closeFunction close);

/**
 * @brief Checks that a (offset, length) pair addresses a valid sub-range of an SNI array.
 *
 * Use this for the common case where the caller supplies both the start offset and the number of
 * bytes accessed. All three arguments are signed so a negative caller value is rejected rather than
 * wrapping to a huge size_t downstream.
 *
 * @param[in] array_length the length of the SNI array (from microej_sni_get_array_length).
 * @param[in] offset       the start offset into the array.
 * @param[in] length       the number of bytes accessed from @p offset.
 * @return true iff @p offset >= 0, @p length >= 0 and [offset, offset + length) fits within @p array_length.
 */
bool llcrypto_bounds_ok(int32_t array_length, int32_t offset, int32_t length);

/**
 * @brief Checks that an offset lands within an SNI array, for the case where the accessed length is
 *        derived from the array itself (array_length - offset) rather than supplied by the caller.
 *
 * @param[in] array_length the length of the SNI array (from microej_sni_get_array_length).
 * @param[in] offset       the offset into the array.
 * @return true iff @p offset >= 0 and @p offset <= @p array_length.
 */
bool llcrypto_offset_ok(int32_t array_length, int32_t offset);

/**
 * @brief Maps a failed PSA status to the LLAPI error code for the SNI layer.
 *
 * PSA_ERROR_INSUFFICIENT_MEMORY becomes LLCRYPTO_ERROR_OOM, every other status maps to the generic LLCRYPTO_ERROR.
 *
 * @param[in] status the failed PSA status.
 * @return LLCRYPTO_ERROR_OOM or LLCRYPTO_ERROR.
 */
int32_t llcrypto_sni_error_code(psa_status_t status);

/**
 * @brief Size in bytes of the DER header (tag and length octets) that opens @p der.
 *
 * Reads only the length octets (X.690 8.1.3): a short form is one octet, a long form is one octet
 * announcing how many length octets follow. Call it on a buffer that a preceding
 * {@code mbedtls_asn1_get_tag()} has already validated: this helper does not check that the header
 * is well formed beyond the buffer size, it only measures it so that callers can compute element
 * boundaries with indexes instead of pointer arithmetic.
 *
 * @param[in] der        the DER-encoded element.
 * @param[in] der_length the number of bytes available in @p der.
 * @return the header size in bytes, or 0 when @p der_length is too short to hold a header.
 */
size_t llcrypto_asn1_header_length(const uint8_t *der, size_t der_length);

/**
 * @brief Throws the NativeException of a failed crypto operation.
 *
 * The message reaches Java only with the generic LLCRYPTO_ERROR code. With a specific code, the code
 * says what failed and the message goes to a debug log.
 *
 * @param[in] error_code LLCRYPTO_ERROR or one of the specific LLCRYPTO_ERROR_* codes.
 * @param[in] message    what failed.
 */
void llcrypto_throw_error(int32_t error_code, const char *message);

/**
 * @brief Throws the NativeException of a failed PSA call, with the code llcrypto_sni_error_code() gives.
 *
 * With the generic code, the message also carries the PSA status.
 *
 * @param[in] status  the failed PSA status.
 * @param[in] message what failed.
 */
void llcrypto_throw_psa_error(psa_status_t status, const char *message);

#ifdef __cplusplus
}
#endif

#endif // LLCRYPTO_UTILS_H
