/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

/* mbedTLS include files. */
#include <mbedtls/build_info.h>
#include <mbedtls/debug.h>
#include <mbedtls/ssl.h>
#include <mbedtls/error.h>
#include <mbedtls/platform.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#if MBEDTLS_VERSION_MAJOR == 3
#include <mbedtls/psa_util.h>
#endif

/* MicroEJ include files. */
#include "LLNET_SSL_mbedtls_configuration.h"
#include "LLNET_SSL_mbedtls_context.h"
#include "LLNET_SSL_utils_mbedtls.h"
#include "LLNET_SSL_verifyCallback.h"
#include "LLNET_SSL_CONTEXT_impl.h"
#include "LLNET_SSL_CONSTANTS.h"
#include "LLNET_SSL_ERRORS.h"

#include <stdbool.h>
#include <string.h>

VEEPORT_LOGGER_DECLARE_MODULE(LLNET_SSL_LOG_MODULE)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sanity check between the expected version of the configuration and the actual version of
 * the configuration.
 * If an error is raised here, it means that a new version of the CCO has been installed and
 * the configuration LLNET_SSL_mbedtls_configuration.h must be updated based on the one provided
 * by the new CCO version.
 */
#if LLNET_SSL_MBEDTLS_CONFIGURATION_VERSION != 2
	#error \
	"Version of the configuration file LLNET_SSL_mbedtls_configuration.h is not compatible with this implementation."
#endif

/* ----------- Definitions  -----------*/
#define PEM_END "-----END CERTIFICATE-----"

/* ----------- Private API  -----------*/
#if MBEDTLS_DEBUG_LEVEL > 0
/**
 * Debug callback for mbed TLS
 * Just prints on the USB serial port
 */
void microej_mbedtls_debug(void *ctx, int level, const char *file, int line,
                           const char *str) {
	const char *p, *basename;
	(void)ctx;

	/* Extract basename from file */
	for (p = basename = file; *p != '\0'; p++) {
		if (*p == '/' || *p == '\\') {
			basename = p + 1;
		}
	}

	VEEPORT_LOGGER_LOG_DBG("%s:%04d: |%d| %s", basename, line, level, str);
}

#endif

/**
 * Parses a certificate (PEM or DER) and appends it to the given chain.
 *
 * @return J_SSL_NO_ERROR on success, or a J_* error code.
 */
static int32_t llnet_ssl_parse_certificate(mbedtls_x509_crt *chain, uint8_t *cert, int32_t len, int32_t format) {
	int32_t ret = J_SSL_NO_ERROR;
	if (CERT_DER_FORMAT == format) {
		int rc = mbedtls_x509_crt_parse_der(chain, (const unsigned char *)cert, (size_t)len);
		ret = LLNET_SSL_TranslateReturnCode(rc);
	} else {
		ret = LLNET_SSL_utils_mbedtls_x509_crt_parse(chain, cert, 0, (uint32_t)len);
	}
	return ret;
}

/**
 * Hands the own certificate chain and private key to Mbed TLS once both have been parsed.
 * Mbed TLS keeps a pointer to the objects, which live in the context, so this is done only once: a later
 * setCertificate/setPrivateKey re-parses the material in place and Mbed TLS sees the new content.
 */
static void llnet_ssl_install_own_cert(llnet_ssl_context *ctx) {
	if (ctx->own_cert_parsed && ctx->own_key_parsed && !ctx->own_cert_installed) {
		int rc = mbedtls_ssl_conf_own_cert(&ctx->conf, &ctx->own_cert, &ctx->own_key);
		if (0 != rc) {
			LLNET_SSL_DEBUG_MBEDTLS_TRACE("mbedtls_ssl_conf_own_cert", rc);
			LLNET_SSL_throw_mbedtls_error(rc, "Cannot install the private key");
			return;
		}
		ctx->own_cert_installed = true;
	}
}

/* ----------- API  -----------*/

int32_t LLNET_SSL_CONTEXT_IMPL_createContext(int32_t protocol, uint8_t isClientContext) {
	VEEPORT_LOGGER_LOG_DBG("protocol=%d, isClientContext=%d", protocol, isClientContext);

	/* Mbed TLS 4 only knows TLS 1.2 and TLS 1.3; each is compiled in by its own Kconfig option. The context is
	 * pinned to the requested version, as the SSL Foundation Library expects. */
	mbedtls_ssl_protocol_version version;
	switch (protocol) {
#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
	case TLSv1_2_PROTOCOL:
		version = MBEDTLS_SSL_VERSION_TLS1_2;
		break;
#endif
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
	case TLSv1_3_PROTOCOL:
		version = MBEDTLS_SSL_VERSION_TLS1_3;
		break;
#endif
	default:
		VEEPORT_ERRORS_throw_caught_exception(J_VERSION_ERROR);
		return SNI_IGNORED_RETURNED_VALUE;
	}

	llnet_ssl_context *ctx = (llnet_ssl_context *)mbedtls_calloc(1, sizeof(llnet_ssl_context));
	if (NULL == ctx) {
		VEEPORT_ERRORS_throw_caught_exception(J_MEMORY_ERROR);
		return SNI_IGNORED_RETURNED_VALUE;
	}

	mbedtls_ssl_config_init(&ctx->conf);
	mbedtls_x509_crt_init(&ctx->trust_store);
	mbedtls_x509_crt_init(&ctx->own_cert);
	mbedtls_pk_init(&ctx->own_key);

	int endpoint = (0 != isClientContext) ? MBEDTLS_SSL_IS_CLIENT : MBEDTLS_SSL_IS_SERVER;
	int ret = mbedtls_ssl_config_defaults(&ctx->conf, endpoint, MBEDTLS_SSL_TRANSPORT_STREAM,
	                                      MBEDTLS_SSL_PRESET_DEFAULT);
	if (0 != ret) {
		LLNET_SSL_DEBUG_MBEDTLS_TRACE("mbedtls_ssl_config_defaults", ret);
		mbedtls_ssl_config_free(&ctx->conf);
		mbedtls_free(ctx);
		LLNET_SSL_throw_mbedtls_error(ret, "MbedTLS config defaults failed");
		return SNI_IGNORED_RETURNED_VALUE;
	}

	mbedtls_ssl_conf_min_tls_version(&ctx->conf, version);
	mbedtls_ssl_conf_max_tls_version(&ctx->conf, version);

#if MBEDTLS_VERSION_MAJOR == 3
	/* Mbed TLS 3 still needs the (PSA) random generator handed to the configuration; Mbed TLS 4 uses it implicitly. */
	mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_psa_get_random, MBEDTLS_PSA_RANDOM_STATE);
#endif

	/* Certificate verification callback: it looks the peer certificates up in the trust store. */
	ctx->verify_ctx.trust_store = &ctx->trust_store;
	ctx->verify_ctx.isUnTrustCA = 0;
	mbedtls_ssl_conf_verify(&ctx->conf, LLNET_SSL_VERIFY_verifyCallback, (void *)&ctx->verify_ctx);

#if MBEDTLS_DEBUG_LEVEL > 0
	mbedtls_ssl_conf_dbg(&ctx->conf, microej_mbedtls_debug, NULL);
#if defined(MBEDTLS_DEBUG_C)
	mbedtls_debug_set_threshold(MBEDTLS_DEBUG_LEVEL);
#endif
#endif

	VEEPORT_LOGGER_LOG_DBG("method=%d: return ctx=%p", protocol, ctx);
	return (int32_t)ctx;
}

void LLNET_SSL_CONTEXT_IMPL_addTrustedCertificate(int32_t contextID, uint8_t *cert, int32_t len, int32_t format) {
	VEEPORT_LOGGER_LOG_DBG("context=%d", contextID);

	int32_t ret = J_SSL_NO_ERROR;
	llnet_ssl_context *ctx = (llnet_ssl_context *)(contextID);

	/* Check parameters */
	if (NULL == ctx || NULL == cert || 0 == len ||
	    ((CERT_DER_FORMAT != format) && (CERT_PEM_FORMAT != format))) {
		VEEPORT_ERRORS_throw_caught_exception(J_BAD_FUNC_ARG);
		return;
	}

	if (CERT_DER_FORMAT == format) {
		ret = llnet_ssl_parse_certificate(&ctx->trust_store, cert, len, format);
	} else {
		/* Find end of first certificate. */
		char *first_cert_end_ptr = strstr((char *)cert, PEM_END);
		if (NULL == first_cert_end_ptr) {
			ret = J_BAD_FUNC_ARG;
		} else {
			first_cert_end_ptr += sizeof(PEM_END);
			int first_cert_len = first_cert_end_ptr - (char *)cert;
			ret = LLNET_SSL_utils_mbedtls_x509_crt_parse(&ctx->trust_store, cert, 0, (uint32_t)first_cert_len);
		}
	}

	if (J_SSL_NO_ERROR != ret) {
		/* A failed parse leaves the chain as it was: the certificates already trusted stay valid. */
		LLNET_SSL_throw_caught_exception(ret, "Certificate parsing error");
		return;
	}

	if (!ctx->trust_store_installed) {
		mbedtls_ssl_conf_ca_chain(&ctx->conf, &ctx->trust_store, NULL);
		ctx->trust_store_installed = true;
	}
}

void LLNET_SSL_CONTEXT_IMPL_clearTrustStore(int32_t contextID) {
	VEEPORT_LOGGER_LOG_DBG("context=%d", contextID);
	llnet_ssl_context *ctx = (llnet_ssl_context *)(contextID);

	if (NULL != ctx) {
		mbedtls_ssl_conf_ca_chain(&ctx->conf, NULL, NULL);
		ctx->trust_store_installed = false;
		mbedtls_x509_crt_free(&ctx->trust_store);
		mbedtls_x509_crt_init(&ctx->trust_store);
	}
}

void LLNET_SSL_CONTEXT_IMPL_clearKeyStore(int32_t contextID) {
	VEEPORT_LOGGER_LOG_DBG("context=%d", contextID);
	llnet_ssl_context *ctx = (llnet_ssl_context *)(contextID);

	/* Free the private key and its certificate chain in place: Mbed TLS keeps pointing at the (now empty)
	 * objects, which a later setCertificate/setPrivateKey refills. */
	if (NULL != ctx) {
		mbedtls_pk_free(&ctx->own_key);
		mbedtls_pk_init(&ctx->own_key);
		ctx->own_key_parsed = false;
		mbedtls_x509_crt_free(&ctx->own_cert);
		mbedtls_x509_crt_init(&ctx->own_cert);
		ctx->own_cert_parsed = false;
	}
}

void LLNET_SSL_CONTEXT_IMPL_setCertificate(int32_t contextID, uint8_t *cert, int32_t len, int32_t format) {
	VEEPORT_LOGGER_LOG_DBG("context=%d", contextID);
	llnet_ssl_context *ctx = (llnet_ssl_context *)(contextID);

	/* Check parameters */
	if (NULL == ctx || NULL == cert || 0 == len ||
	    ((CERT_DER_FORMAT != format) && (CERT_PEM_FORMAT != format))) {
		VEEPORT_ERRORS_throw_caught_exception(J_BAD_FUNC_ARG);
		return;
	}

	/* The LLNET_SSL_* interface supports a single credential: replace the existing chain, in place. */
	mbedtls_x509_crt_free(&ctx->own_cert);
	mbedtls_x509_crt_init(&ctx->own_cert);
	ctx->own_cert_parsed = false;

	int32_t ret = llnet_ssl_parse_certificate(&ctx->own_cert, cert, len, format);
	if (J_SSL_NO_ERROR != ret) {
		mbedtls_x509_crt_free(&ctx->own_cert);
		mbedtls_x509_crt_init(&ctx->own_cert);
		LLNET_SSL_throw_caught_exception(ret, "Certificate parsing error");
		return;
	}
	ctx->own_cert_parsed = true;

	llnet_ssl_install_own_cert(ctx);
}

void LLNET_SSL_CONTEXT_IMPL_setPrivateKey(int32_t contextID, uint8_t *privateKey, int32_t len, uint8_t *password,
                                          int32_t passwordLen) {
	VEEPORT_LOGGER_LOG_DBG("context=%d", contextID);
	llnet_ssl_context *ctx = (llnet_ssl_context *)(contextID);

	/* Check parameters */
	if (NULL == ctx || NULL == privateKey || 0 >= len || NULL == password || 0 >= passwordLen) {
		VEEPORT_ERRORS_throw_caught_exception(J_BAD_FUNC_ARG);
		return;
	}

	/* The LLNET_SSL_* interface supports a single credential: replace the existing key, in place. */
	mbedtls_pk_free(&ctx->own_key);
	mbedtls_pk_init(&ctx->own_key);
	ctx->own_key_parsed = false;

	/* Since Mbed TLS 4 the parser takes its randomness from PSA implicitly; Mbed TLS 3 takes it as arguments. */
#if MBEDTLS_VERSION_MAJOR == 3
	int ret = mbedtls_pk_parse_key(&ctx->own_key, (const unsigned char *)privateKey, (size_t)len,
	                               (const unsigned char *)password, (size_t)passwordLen,
	                               mbedtls_psa_get_random, MBEDTLS_PSA_RANDOM_STATE);
#else
	int ret = mbedtls_pk_parse_key(&ctx->own_key, (const unsigned char *)privateKey, (size_t)len,
	                               (const unsigned char *)password, (size_t)passwordLen);
#endif
	if (0 != ret) {
		LLNET_SSL_DEBUG_MBEDTLS_TRACE("mbedtls_pk_parse_key", ret);
		mbedtls_pk_free(&ctx->own_key);
		mbedtls_pk_init(&ctx->own_key);
		LLNET_SSL_throw_mbedtls_error(ret, "Private key parsing error");
		return;
	}
	ctx->own_key_parsed = true;

	llnet_ssl_install_own_cert(ctx);
}

int32_t LLNET_SSL_CONTEXT_IMPL_initChainBuffer(int32_t contextID, int32_t nbChainCerts, int32_t chainCertsTotalSize) {
	VEEPORT_LOGGER_LOG_DBG("context=%d, nbChainCerts=%d, chainCertsTotalSize=%d", contextID, nbChainCerts,
	                       chainCertsTotalSize);
	return 0;
}

void LLNET_SSL_CONTEXT_IMPL_addChainCertificate(int32_t contextID, uint8_t *cert, int32_t len, int32_t format,
                                                int32_t chainBufferSize) {
	VEEPORT_LOGGER_LOG_DBG("context=%d", contextID);

	(void)chainBufferSize;
	llnet_ssl_context *ctx = (llnet_ssl_context *)(contextID);

	/* Check parameters */
	if (NULL == ctx || NULL == cert || 0 == len ||
	    ((CERT_DER_FORMAT != format) && (CERT_PEM_FORMAT != format))) {
		VEEPORT_ERRORS_throw_caught_exception(J_BAD_FUNC_ARG);
		return;
	}

	/* The chain hangs below the own certificate, which must have been set first. */
	if (!ctx->own_cert_parsed) {
		VEEPORT_ERRORS_throw_caught_exception(J_BAD_FUNC_ARG);
		return;
	}

	/* Parse the certificate, appending it to the own certificate chain. */
	int32_t ret = llnet_ssl_parse_certificate(&ctx->own_cert, cert, len, format);
	if (J_SSL_NO_ERROR != ret) {
		LLNET_SSL_throw_caught_exception(ret, "Certificate parsing error");
	}
}

void LLNET_SSL_CONTEXT_IMPL_freeContext(int32_t contextID) {
	VEEPORT_LOGGER_LOG_DBG("context=%d", contextID);
	llnet_ssl_context *ctx = (llnet_ssl_context *)(contextID);

	if (NULL != ctx) {
		/* Release the references held by the configuration before freeing the objects behind them. */
		mbedtls_ssl_conf_ca_chain(&ctx->conf, NULL, NULL);
		mbedtls_ssl_conf_verify(&ctx->conf, NULL, NULL);
		mbedtls_ssl_config_free(&ctx->conf);

		mbedtls_x509_crt_free(&ctx->trust_store);
		mbedtls_x509_crt_free(&ctx->own_cert);
		mbedtls_pk_free(&ctx->own_key);

		mbedtls_free((void *)ctx);
	}
}

#ifdef __cplusplus
}
#endif
