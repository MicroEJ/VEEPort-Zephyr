/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

#include "LLSEC_configuration.h"
#include "LLSEC_KEY_AGREEMENT_impl.h"

#include <stdint.h>

// cppcheck-suppress misra-c2012-8.7; external linkage is required as this function is part of the API
int32_t LLSEC_KEY_AGREEMENT_IMPL_get_algorithm_id(uint8_t *algorithm_name) {
	(void)(algorithm_name);

	LLSEC_ASSERT(SNI_OK == SNI_throwNativeException(LLSEC_ERROR, "Not implemented"));

	return SNI_IGNORED_RETURNED_VALUE;
}

// cppcheck-suppress misra-c2012-8.7; external linkage is required as this function is part of the API
int32_t LLSEC_KEY_AGREEMENT_IMPL_get_max_size(int32_t algorithm, int32_t local_private_key, int32_t remote_public_key) {
	(void)(algorithm);
	(void)(local_private_key);
	(void)(remote_public_key);

	LLSEC_ASSERT(SNI_OK == SNI_throwNativeException(LLSEC_ERROR, "Not implemented"));

	return SNI_IGNORED_RETURNED_VALUE;
}

// cppcheck-suppress misra-c2012-8.7; external linkage is required as this function is part of the API
int32_t LLSEC_KEY_AGREEMENT_IMPL_generate_secret(int32_t algorithm, int32_t local_private_key,
                                                 int32_t remote_public_key, uint8_t *secret_buffer) {
	(void)(algorithm);
	(void)(local_private_key);
	(void)(remote_public_key);
	(void)(secret_buffer);

	LLSEC_ASSERT(SNI_OK == SNI_throwNativeException(LLSEC_ERROR, "Not implemented"));

	return SNI_IGNORED_RETURNED_VALUE;
}
