/*
 * Copyright 2021-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <LLCRYPTO_KEY_FACTORY_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_key.h"
#include "LLCRYPTO_ec_curve.h"
#include "sni_extras.h"

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)

/* SEC1 uncompressed-point prefix byte: PSA exposes no macro for the wire tag. */
#define ECC_UNCOMPRESSED_POINT_TAG (0x04U)

/* Right-align a coordinate produced by Java BigInteger.toByteArray() (two's-complement, big-endian,
 * variable length: it may carry a leading 0x00 sign byte and may drop leading zero bytes) into a
 * fixed field_bytes big-endian field element. Leading zero bytes are skipped, so a positive value
 * with a sign byte and a short value are both handled. Returns false if the magnitude does not fit
 * field_bytes. */
static bool llcrypto_ec_normalize_coordinate(const uint8_t *src, int32_t src_length, uint8_t *dst,
                                             size_t field_bytes) {
	bool success = false;
	if ((NULL != src) && (src_length >= 0)) {
		const uint8_t *p = src;
		size_t len = (size_t)src_length;
		/* Strip leading zero bytes: the two's-complement sign byte from BigInteger.toByteArray()
		 * plus any non-significant high zeros. */
		while ((len > 0u) && (0u == *p)) {
			p++;
			len--;
		}
		if (len <= field_bytes) {
			size_t pad = field_bytes - len;
			(void)memset(dst, 0, pad);
			(void)memcpy(&dst[pad], p, len);
			success = true;
		}
	}
	return success;
}

#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

int32_t LLCRYPTO_KEY_FACTORY_IMPL_get_algorithm_description(uint8_t *algorithm_name) {
	LLCRYPTO_UNUSED_PARAM(algorithm_name);
	VEEPORT_LOGGER_LOG_DBG("");

	/* The algorithm_id returned by this function is not used by any downstream
	 * Key Factory method. Any non-LLCRYPTO_ERROR value signals to the Java layer
	 * that this implementation is available. */
	return LLCRYPTO_SUCCESS;
}

int32_t LLCRYPTO_KEY_FACTORY_IMPL_get_public_key_data(int32_t algorithm_id, uint8_t *format_name, uint8_t *encoded_key,
                                                      int32_t encoded_key_length) {
	LLCRYPTO_UNUSED_PARAM(algorithm_id);
	LLCRYPTO_UNUSED_PARAM(format_name);
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();

	struct llcrypto_key *key = NULL;

	/* Defense-in-depth: bound encoded_key_length against the array length before mbedtls_pk_parse reads it. */
	if ((encoded_key_length < 0) || (encoded_key_length > microej_sni_get_array_length(encoded_key))) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_LENGTH);
	} else {
		key = llcrypto_key_from_x509_public_key(encoded_key, encoded_key_length);
		if (NULL == key) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_PARSE_FAILED);
		}
	}

	LLCRYPTO_PROFILE_END();
	return (intptr_t)key;
}

int32_t LLCRYPTO_KEY_FACTORY_IMPL_get_private_key_data(int32_t algorithm_id, uint8_t *format_name, uint8_t *encoded_key,
                                                       int32_t encoded_key_length) {
	LLCRYPTO_UNUSED_PARAM(algorithm_id);
	LLCRYPTO_UNUSED_PARAM(format_name);
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();

	struct llcrypto_key *key = NULL;
	int32_t return_code = LLCRYPTO_SUCCESS;

	/* Defense-in-depth: bound encoded_key_length against the array length before the PKCS#8 parser reads it. */
	if ((encoded_key_length < 0) || (encoded_key_length > microej_sni_get_array_length(encoded_key))) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_LENGTH);
	} else {
		int32_t key_rc = llcrypto_key_from_pkcs8_private_key(encoded_key, encoded_key_length, &key);
		if (LLCRYPTO_ERROR_OOM == key_rc) {
			/* Returned, not thrown, so that the Crypto library retries after a GC. */
			return_code = LLCRYPTO_ERROR_OOM;
		} else if (LLCRYPTO_SUCCESS != key_rc) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_PARSE_FAILED);
		} else {
			/* key parsed */
		}
	}

	if (LLCRYPTO_SUCCESS == return_code) {
		return_code = (intptr_t)key;
	}

	LLCRYPTO_PROFILE_END();
	return return_code;
}

int32_t LLCRYPTO_KEY_FACTORY_IMPL_get_private_key_close_id(int32_t algorithm_id) {
	LLCRYPTO_UNUSED_PARAM(algorithm_id);
	VEEPORT_LOGGER_LOG_DBG("");

	return (intptr_t)llcrypto_key_free;
}

int32_t LLCRYPTO_KEY_FACTORY_IMPL_get_public_key_close_id(int32_t algorithm_id) {
	LLCRYPTO_UNUSED_PARAM(algorithm_id);
	VEEPORT_LOGGER_LOG_DBG("");

	return (intptr_t)llcrypto_key_free;
}

int32_t LLCRYPTO_KEY_FACTORY_IMPL_get_ec_public_key_from_raw(uint8_t *curveName, uint8_t *x, uint8_t *y) {
	VEEPORT_LOGGER_LOG_DBG("");
	LLCRYPTO_PROFILE_START();
	struct llcrypto_key *key = NULL;
	int32_t return_code = LLCRYPTO_SUCCESS;

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
	/* The field-element byte size and the exported uncompressed-point size both derive from the curve size
	 * via the PSA helpers. */
	const llcrypto_ec_curve *curve = llcrypto_ec_curve_from_name((const char *)curveName);

	if (NULL == curve) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_ALGORITHM_NOT_SUPPORTED);
	} else {
		size_t field_bytes = PSA_BITS_TO_BYTES(curve->bits);
		/* Size the buffer to the vendor worst case, not PSA_KEY_EXPORT_ECC_PUBLIC_KEY_MAX_SIZE(curve->bits):
		 * curve->bits is only known at runtime, so the tighter bound would make point a variable-length
		 * array, forbidden by MISRA C:2012 Rule 18.8. */
		uint8_t point[PSA_KEY_EXPORT_ECC_PUBLIC_KEY_MAX_SIZE(PSA_VENDOR_ECC_MAX_CURVE_BITS)];
		point[0] = ECC_UNCOMPRESSED_POINT_TAG;

		/* Lay out the uncompressed point as 0x04 || X || Y, each coordinate right-aligned to the field
		 * width. A coordinate that does not fit the field is an invalid parameter. */
		bool laid_out = llcrypto_ec_normalize_coordinate(x, microej_sni_get_array_length(x), &point[1], field_bytes);
		if (laid_out) {
			laid_out = llcrypto_ec_normalize_coordinate(y, microej_sni_get_array_length(y),
			                                            &point[1U + field_bytes], field_bytes);
		}

		if (!laid_out) {
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR,
			                                               "EC coordinate does not fit the curve field");
		} else {
			size_t point_length = 1U + (2U * field_bytes);
			/* Import the point as a PSA public key; PSA rejects a point that is not on the curve. */
			int32_t key_rc = llcrypto_key_from_ec_public_key(curve->family, curve->bits, point, point_length, &key);
			if (LLCRYPTO_ERROR_OOM == key_rc) {
				/* Returned, not thrown, so that the Crypto library retries after a GC. */
				return_code = LLCRYPTO_ERROR_OOM;
			} else if (LLCRYPTO_SUCCESS != key_rc) {
				VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "Failed to build EC public key");
			} else {
				/* key built */
			}
		}
	}
#else
	LLCRYPTO_UNUSED_PARAM(curveName);
	LLCRYPTO_UNUSED_PARAM(x);
	LLCRYPTO_UNUSED_PARAM(y);
	VEEPORT_LOGGER_LOG_DBG("disabled: PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY not set");
	VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_ALGORITHM_NOT_SUPPORTED);
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

	if (LLCRYPTO_SUCCESS == return_code) {
		return_code = (intptr_t)key;
	}

	LLCRYPTO_PROFILE_END();
	return return_code;
}
