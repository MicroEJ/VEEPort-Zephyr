/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#ifndef LLNET_SSL_VERIFY_CALLBACK
#define LLNET_SSL_VERIFY_CALLBACK

#include <stdint.h>
#include <mbedtls/build_info.h>
#include <mbedtls/x509_crt.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Verify Callback context
 */
typedef struct {
	mbedtls_x509_crt *trust_store; /* head of the trusted certificate chain of the ssl context (never NULL) */
	uint8_t isUnTrustCA;
}cert_verify_ctx;

/*
 * Certificate verification callback
 */
int LLNET_SSL_VERIFY_verifyCallback(void *data, mbedtls_x509_crt *crt, int depth, uint32_t *flags);

#ifdef __cplusplus
}
#endif

#endif //LLNET_SSL_VERIFY_CALLBACK
