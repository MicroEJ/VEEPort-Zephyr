/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

#ifndef LLNET_SSL_MBEDTLS_CONTEXT_H
#define LLNET_SSL_MBEDTLS_CONTEXT_H

#include <stdbool.h>
#include <stdint.h>
#include <mbedtls/build_info.h>
#include <mbedtls/ssl.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include "LLNET_SSL_verifyCallback.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief State behind an LLNET_SSL context ID.
 *
 * Mbed TLS 4 keeps the fields of mbedtls_ssl_config private, so the objects the configuration points to
 * (trust store, own certificate chain, private key) are owned here, next to the
 * configuration, instead of being read back from it. Everything is embedded so that the pointers handed to
 * Mbed TLS stay valid for the whole life of the context: clearing a store frees and re-initializes the
 * object in place, it never moves it.
 */
typedef struct {
	mbedtls_ssl_config conf;              /* Mbed TLS configuration shared by the sockets of this context */
	cert_verify_ctx verify_ctx;           /* argument of the certificate verification callback */
	mbedtls_x509_crt trust_store;         /* trusted certificates; empty until the first one is added */
	mbedtls_x509_crt own_cert;            /* certificate chain of the private key; empty until set */
	mbedtls_pk_context own_key;           /* private key; empty until set */
	bool trust_store_installed;           /* trust_store has been handed to conf */
	bool own_cert_parsed;                 /* own_cert holds a certificate */
	bool own_key_parsed;                  /* own_key holds a key */
	bool own_cert_installed;              /* own_cert and own_key have been handed to conf */
} llnet_ssl_context;

/**
 * @brief State behind an LLNET_SSL socket ID.
 *
 * The socket descriptor is stored next to the Mbed TLS context so that it can serve as the BIO context
 * without a separate allocation.
 */
typedef struct {
	mbedtls_ssl_context ssl; /* Mbed TLS connection state */
	int fd;                  /* underlying socket, passed to the BIO send/receive callbacks */
} llnet_ssl_socket;

#ifdef __cplusplus
}
#endif

#endif //LLNET_SSL_MBEDTLS_CONTEXT_H
