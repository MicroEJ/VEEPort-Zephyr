/*
 * Copyright 2021-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#include <stdint.h>
#include <stddef.h>
#include <psa/crypto.h>
#include <LLCRYPTO_RANDOM_impl.h>
#include <LLCRYPTO_CONSTANTS.h>
#include <sni.h>

#include "LLCRYPTO_configuration.h"
#include "LLCRYPTO_utils.h"
#include "sni_extras.h"

VEEPORT_LOGGER_DECLARE_MODULE(LLCRYPTO_LOG_MODULE)

/*
 * SecureRandom is backed by the PSA DRBG (psa_generate_random), which is seeded from the
 * platform entropy source of each board (e.g. the CRACEN TRNG on nRF54L15, the SoC RNG
 * peripheral on the i.MX RT1170 / MCX N947). This drives the behavior of the two seed LLAPIs:
 *   - set_seed is a no-op with an always-on warning: PSA exposes no reseed/add-entropy call,
 *     and Java's SecureRandom.setSeed() contract makes the seed supplement (never replace)
 *     the already well-seeded DRBG, so dropping it is spec-compliant.
 *   - generate_seed fills the buffer from the DRBG, the same source as next_bytes. This is
 *     the sanctioned PSA usage (mbedtls docs/psa-transition.md: "If you need random data for
 *     some other purposes, call psa_generate_random"), and seeding one generator from another
 *     CSPRNG's output is standard practice (SP 800-90A chained DRBGs). On boards whose
 *     entropy driver is itself a hardware DRBG (i.MX RT1170 CAAM, nRF54L15 CRACEN) the PSA
 *     path lands on the very same driver call anyway. It must not be a no-op, or
 *     SecureRandom.generateSeed() would return the caller's zero-filled array.
 */

static void llcrypto_random_close(void *native_id) {
	LLCRYPTO_UNUSED_PARAM(native_id);
	VEEPORT_LOGGER_LOG_DBG("");
	// No per-instance native resource to release: the DRBG is global PSA state.
}

int32_t LLCRYPTO_RANDOM_IMPL_init(void) {
	VEEPORT_LOGGER_LOG_DBG("");
	// native_id is a monotonic "fake" handle: pre-incremented before each use so a
	// freshly registered SNI resource never gets id 0 (reserved as the NULL/error sentinel).
	static int32_t native_id = 1;
	int32_t return_code = LLCRYPTO_ERROR;
	// Never return the same "fake" ID twice
	native_id++;
	// The initialization is done by psa_crypto_init() in LLCRYPTO_IMPL_initialize(void)
	/* cppcheck-suppress misra-c2012-11.6 ; native_id is a synthetic int handle, not a real object pointer */
	if (SNI_OK == SNI_registerResource((void *)native_id, llcrypto_random_close, NULL)) {
		return_code = native_id;
	} else {
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_SNI_REGISTER_FAILED);
		return_code = LLCRYPTO_ERROR;
	}
	return return_code;
}

void LLCRYPTO_RANDOM_IMPL_close(int32_t native_id) {
	VEEPORT_LOGGER_LOG_DBG("");
	/* cppcheck-suppress misra-c2012-11.6 ; native_id is a synthetic int handle, not a real object pointer */
	llcrypto_unregister_and_close((void *)native_id, llcrypto_random_close);
}

void LLCRYPTO_RANDOM_IMPL_next_bytes(int32_t native_id, uint8_t *rnd, int32_t size) {
	LLCRYPTO_UNUSED_PARAM(native_id);
	VEEPORT_LOGGER_LOG_DBG("");

	int32_t rnd_array_length = microej_sni_get_array_length(rnd);
	if ((size < 0) || (size > rnd_array_length)) {
		VEEPORT_LOGGER_LOG_DBG("invalid size %d (array %d)", size, rnd_array_length);
		VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, LLCRYPTO_MSG_INVALID_LENGTH);
	} else {
		psa_status_t status = psa_generate_random(rnd, size);
		if (PSA_SUCCESS != status) {
			VEEPORT_LOGGER_LOG_DBG("psa_generate_random failed! (Error: %d)", status);
			VEEPORT_ERRORS_throw_runtime_exception_message(LLCRYPTO_ERROR, "psa_generate_random() failed");
		}
	}
}

void LLCRYPTO_RANDOM_IMPL_set_seed(int32_t native_id, uint8_t *seed, int32_t size) {
	LLCRYPTO_UNUSED_PARAM(native_id);
	LLCRYPTO_UNUSED_PARAM(seed);
	LLCRYPTO_UNUSED_PARAM(size);
	VEEPORT_LOGGER_LOG_DBG("");
	/* No-op: PSA has no reseed API and the seed only supplements the DRBG (see file header). */
	VEEPORT_LOGGER_LOG_WRN("seed ignored");
}

void LLCRYPTO_RANDOM_IMPL_generate_seed(int32_t native_id, uint8_t *seed, int32_t size) {
	VEEPORT_LOGGER_LOG_DBG("");
	/*
	 * A generated seed is fresh CSPRNG output — the same bounded fill as next_bytes. Valid seed
	 * material for any consumer (see the rationale in the file header).
	 */
	LLCRYPTO_RANDOM_IMPL_next_bytes(native_id, seed, size);
}

int32_t LLCRYPTO_RANDOM_IMPL_get_close_id(void) {
	VEEPORT_LOGGER_LOG_DBG("");
	return (intptr_t)llcrypto_random_close;
}
