/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <LLCRYPTO_CONSTANTS.h>
#include <psa/crypto.h>

#include "LLCRYPTO_utils.h"
#include "LLCRYPTO_configuration.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

void llcrypto_unregister_and_close(void *resource, SNI_closeFunction close) {
	if (SNI_OK != SNI_unregisterResource(resource, close)) {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "Can't unregister SNI native resource");
	} else {
		close(resource);
	}
}

bool llcrypto_bounds_ok(int32_t array_length, int32_t offset, int32_t length) {
	/* length >= 0 is checked before the (array_length - length) subtraction, so the subtraction
	 * cannot be fed a negative length; offset >= 0 keeps the final comparison well-defined. */
	return (offset >= 0) && (length >= 0) && (offset <= (array_length - length));
}

bool llcrypto_offset_ok(int32_t array_length, int32_t offset) {
	return (offset >= 0) && (offset <= array_length);
}

int32_t llcrypto_sni_error_code(psa_status_t status) {
	return (PSA_ERROR_INSUFFICIENT_MEMORY == status) ? LLCRYPTO_ERROR_OOM : LLCRYPTO_ERROR;
}

/* X.690 8.1.3.5: in the long form the first length octet has bit 8 set and its low bits count the
 * following length octets. */
#define LLCRYPTO_ASN1_LONG_FORM_FLAG    (0x80u)
#define LLCRYPTO_ASN1_LONG_FORM_COUNT   (0x7Fu)
/* Tag octet plus the first length octet. */
#define LLCRYPTO_ASN1_MIN_HEADER_LENGTH (2u)

size_t llcrypto_asn1_header_length(const uint8_t *der, size_t der_length) {
	size_t header_length = 0;

	if (der_length >= LLCRYPTO_ASN1_MIN_HEADER_LENGTH) {
		uint8_t first_length_octet = der[1];
		header_length = LLCRYPTO_ASN1_MIN_HEADER_LENGTH;
		if ((first_length_octet & LLCRYPTO_ASN1_LONG_FORM_FLAG) != 0u) {
			header_length += first_length_octet & LLCRYPTO_ASN1_LONG_FORM_COUNT;
		}
	}

	return header_length;
}

void llcrypto_throw_error(int32_t error_code, const char *message) {
	if (LLCRYPTO_ERROR == error_code) {
		VEEPORT_ERRORS_throw_runtime_exception_message(error_code, "%s", message);
	} else {
		VEEPORT_LOGGER_LOG_DBG("%s (%d)", message, (int)error_code);
		VEEPORT_ERRORS_throw_runtime_exception(error_code);
	}
}

void llcrypto_throw_psa_error(psa_status_t status, const char *message) {
	int32_t error_code = llcrypto_sni_error_code(status);
	if (LLCRYPTO_ERROR == error_code) {
		VEEPORT_ERRORS_throw_runtime_exception_message(error_code, "%s: %d", message, (int)status);
	} else {
		VEEPORT_LOGGER_LOG_DBG("%s: %d", message, (int)status);
		VEEPORT_ERRORS_throw_runtime_exception(error_code);
	}
}
