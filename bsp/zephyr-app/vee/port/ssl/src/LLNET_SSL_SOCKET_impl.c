/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

#include <mbedtls/build_info.h>
#include <mbedtls/platform.h>
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <psa/crypto.h>
#include "LLNET_Common.h"
#include "LLNET_CHANNEL_impl.h"
#include "LLNET_SSL_mbedtls_configuration.h"
#include "LLNET_SSL_mbedtls_context.h"
#include "LLNET_SSL_ERRORS.h"
#include "LLNET_SSL_utils_mbedtls.h"
#include "LLNET_SSL_SOCKET_impl.h"
#include "LLNET_SSL_CONSTANTS.h"
#include <stdio.h>

VEEPORT_LOGGER_DEFINE_MODULE(LLNET_SSL_LOG_MODULE)

#ifdef __cplusplus
extern "C" {
#endif

/* static functions */
static void LLNET_SSL_SOCKET_initialHandShake_callback(int32_t sslID, int32_t fd, int64_t absoluteJavaStartTime,
                                                       int32_t relativeTimeout);
static void LLNET_SSL_SOCKET_initialHandShake(int32_t sslID, int32_t fd, int64_t absoluteJavaStartTime,
                                              int64_t absoluteTimeout, int32_t relativeTimeout);
static int32_t LLNET_SSL_SOCKET_read_callback(int32_t sslID, int32_t fd, int8_t *buf, int32_t off, int32_t len,
                                              int64_t absoluteJavaStartTime, int32_t relativeTimeout);
static int32_t LLNET_SSL_SOCKET_read(int32_t sslID, int32_t fd, int8_t *buf, int32_t off, int32_t len,
                                     int64_t absoluteJavaStartTime, int64_t absoluteTimeout, int32_t relativeTimeout,
                                     SNI_callback callback);
static int32_t LLNET_SSL_SOCKET_write_callback(int32_t sslID, int32_t fd, int8_t *buf, int32_t off, int32_t len,
                                               int64_t absoluteJavaStartTime, int32_t relativeTimeout);
static int32_t LLNET_SSL_SOCKET_write(int32_t sslID, int32_t fd, int8_t *buf, int32_t off, int32_t len,
                                      int64_t absoluteJavaStartTime, int64_t absoluteTimeout, int32_t relativeTimeout);
static void LLNET_SSL_SOCKET_close_callback(int32_t sslID, int32_t fd, uint8_t autoclose, int64_t absoluteJavaStartTime,
                                            int32_t relativeTimeout);

/**
 * Returns the Mbed TLS connection state behind a socket ID, or NULL for an invalid ID.
 */
static mbedtls_ssl_context * llnet_ssl_get_ssl_context(int32_t sslID) {
	llnet_ssl_socket *sock = (llnet_ssl_socket *)(sslID);
	return (NULL == sock) ? NULL : &sock->ssl;
}

static void LLNET_SSL_SOCKET_initialHandShake_callback(int32_t sslID, int32_t fd, int64_t absoluteJavaStartTime,
                                                       int32_t relativeTimeout) {
	VEEPORT_LOGGER_LOG_DBG("ssl=%d, fd=%d", (int)sslID, (int)fd);
	uint32_t delta32 = 0;
	//get the time diff between the start time of the LL call and the time from which the current absolute timeout was
	// computed.
	SNI_getCallbackArgs((void **)&delta32, NULL);
	//reconstruct the absolute timeout
	int64_t absoluteTimeout = (0 == relativeTimeout) ? 0 : absoluteJavaStartTime + delta32 + relativeTimeout;
	LLNET_SSL_SOCKET_initialHandShake(sslID, fd, absoluteJavaStartTime, absoluteTimeout, relativeTimeout);
}

static void LLNET_SSL_SOCKET_initialHandShake(int32_t sslID, int32_t fd, int64_t absoluteJavaStartTime,
                                              int64_t absoluteTimeout, int32_t relativeTimeout) {
	VEEPORT_LOGGER_LOG_DBG("ssl=%d, fd=%d", (int)sslID, (int)fd);

	mbedtls_ssl_context *ssl_ctx = llnet_ssl_get_ssl_context(sslID);
	if (NULL == ssl_ctx) {
		VEEPORT_ERRORS_throw_caught_exception(J_BAD_FUNC_ARG);
		return;
	}
	LLNET_SSL_utils_mbedtls_on_bio_start();
	int ret = mbedtls_ssl_handshake(ssl_ctx);

	if (0 != ret) {
		//error
		uint32_t delta32 = 0;
		absoluteTimeout = LLNET_SSL_utils_mbedtls_update_next_bio_timeout(absoluteJavaStartTime, absoluteTimeout,
		                                                                  relativeTimeout, &delta32);
		LLNET_SSL_utils_mbedtls_handle_IO_error(ssl_ctx, ret, true, fd, absoluteTimeout,
		                                        (SNI_callback)LLNET_SSL_SOCKET_initialHandShake_callback,
		                                        (void *)delta32);
	}

	VEEPORT_LOGGER_LOG_DBG("HandShake (ssl=%d, fd=%d) ret=%d", (int)sslID, (int)fd, (int)ret);
}

static int32_t LLNET_SSL_SOCKET_read_callback(int32_t sslID, int32_t fd, int8_t *buf, int32_t off, int32_t len,
                                              int64_t absoluteJavaStartTime, int32_t relativeTimeout) {
	VEEPORT_LOGGER_LOG_DBG("ssl=%d, fd=%d, offset=%d, length=%d", (int)sslID, (int)fd, (int)off, (int)len);
	uint32_t delta32 = 0;
	//get the time diff between the start time of the LL call and the time from which the current absolute timeout was
	// computed.
	SNI_getCallbackArgs((void **)&delta32, NULL);
	//reconstruct the absolute timeout
	int64_t absoluteTimeout = (0 == relativeTimeout) ? 0 : absoluteJavaStartTime + delta32 + relativeTimeout;
	return LLNET_SSL_SOCKET_read(sslID, fd, buf, off, len, absoluteJavaStartTime, absoluteTimeout, relativeTimeout,
	                             (SNI_callback)LLNET_SSL_SOCKET_read_callback);
}

static int32_t LLNET_SSL_SOCKET_read(int32_t sslID, int32_t fd, int8_t *buf, int32_t off, int32_t len,
                                     int64_t absoluteJavaStartTime, int64_t absoluteTimeout, int32_t relativeTimeout,
                                     SNI_callback callback) {
	VEEPORT_LOGGER_LOG_DBG("ssl=%d, fd=%d, offset=%d, length=%d", (int)sslID, (int)fd, (int)off, (int)len);

	mbedtls_ssl_context *ssl = llnet_ssl_get_ssl_context(sslID);
	LLNET_SSL_utils_mbedtls_on_bio_start();
	int32_t ret = mbedtls_ssl_read(ssl, (unsigned char *)buf + off, len);

	if (0 < ret) {
		//success
		return ret;
	}

	if (MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY == ret || 0 == ret) {
		//end-of-file
		return 0;
	}

	uint32_t delta32 = 0;
	absoluteTimeout = LLNET_SSL_utils_mbedtls_update_next_bio_timeout(absoluteJavaStartTime, absoluteTimeout,
	                                                                  relativeTimeout, &delta32);
	LLNET_SSL_utils_mbedtls_handle_IO_error(ssl, ret, false, fd, absoluteTimeout, callback, (void *)delta32);
	return 0;
}

static int32_t LLNET_SSL_SOCKET_write_callback(int32_t sslID, int32_t fd, int8_t *buf, int32_t off, int32_t len,
                                               int64_t absoluteJavaStartTime, int32_t relativeTimeout) {
	VEEPORT_LOGGER_LOG_DBG("ssl=%d, fd=%d, offset=%d, length=%d", (int)sslID, (int)fd, (int)off, (int)len);
	uint32_t delta32 = 0;
	//get the time diff between the start time of the LL call and the time from which the current absolute timeout was
	// computed.
	SNI_getCallbackArgs((void **)&delta32, NULL);
	//reconstruct the absolute timeout
	int64_t absoluteTimeout = (0 == relativeTimeout) ? 0 : absoluteJavaStartTime + delta32 + relativeTimeout;
	return LLNET_SSL_SOCKET_write(sslID, fd, buf, off, len, absoluteJavaStartTime, absoluteTimeout, relativeTimeout);
}

static int32_t LLNET_SSL_SOCKET_write(int32_t sslID, int32_t fd, int8_t *buf, int32_t off, int32_t len,
                                      int64_t absoluteJavaStartTime, int64_t absoluteTimeout, int32_t relativeTimeout) {
	VEEPORT_LOGGER_LOG_DBG("ssl=%d, fd=%d, offset=%d, length=%d", (int)sslID, (int)fd, (int)off, (int)len);

	mbedtls_ssl_context *ssl = llnet_ssl_get_ssl_context(sslID);
	LLNET_SSL_utils_mbedtls_on_bio_start();
	int ret = mbedtls_ssl_write(ssl, (const unsigned char *)buf + off, len);

	if (0 < ret) {
		//success
		return ret;
	}

	if (0 == ret) {
		//should not happen
		//0 byte written
		VEEPORT_ERRORS_throw_caught_exception_message(J_EUNKNOWN, "0 byte written");
		return 0;
	}

	//error
	uint32_t delta32 = 0;
	absoluteTimeout = LLNET_SSL_utils_mbedtls_update_next_bio_timeout(absoluteJavaStartTime, absoluteTimeout,
	                                                                  relativeTimeout, &delta32);
	LLNET_SSL_utils_mbedtls_handle_IO_error(ssl, ret, false, fd, absoluteTimeout,
	                                        (SNI_callback)LLNET_SSL_SOCKET_write_callback, (void *)delta32);

	return 0;
}

static void LLNET_SSL_SOCKET_close_callback(int32_t sslID, int32_t fd, uint8_t autoclose, int64_t absoluteJavaStartTime,
                                            int32_t relativeTimeout) {
	int8_t buffer[1];
	int32_t res;
	//get the time diff between the start time of the LL call and the time from which the current absolute timeout was
	// computed.
	uint32_t delta32 = 0;
	SNI_getCallbackArgs((void **)&delta32, NULL);
	//reconstruct the absolute timeout
	int64_t absoluteTimeout = (0 == relativeTimeout) ? 0 : absoluteJavaStartTime + delta32 + relativeTimeout;
	do{
		res = LLNET_SSL_SOCKET_read(sslID, fd, buffer, 0, 1, absoluteJavaStartTime, absoluteTimeout, relativeTimeout,
		                            (SNI_callback)LLNET_SSL_SOCKET_close_callback);
		absoluteTimeout = (0 == relativeTimeout) ? 0 : LLNET_current_time_ms() + relativeTimeout;
	}while (res > 0);
}

void LLNET_SSL_SOCKET_IMPL_initialize(void) {
	VEEPORT_LOGGER_LOG_DBG("");

	/* Since Mbed TLS 4 every random number, key and handshake goes through PSA Crypto, which must be
	 * initialized first. psa_crypto_init() is idempotent, so sharing it with the Crypto port is fine. */
	psa_status_t status = psa_crypto_init();
	if (PSA_SUCCESS != status) {
		VEEPORT_LOGGER_LOG_DBG("psa_crypto_init failed: %d", (int)status);
		VEEPORT_ERRORS_throw_caught_exception(J_RNG_FAILURE_ERROR);
		return;
	}
}

int32_t LLNET_SSL_SOCKET_IMPL_create(int32_t contextID, int32_t fd, uint8_t *hostname, int32_t hostnameLength,
                                     bool autoclose, uint8_t useClientMode, uint8_t needClientAuth) {
	VEEPORT_LOGGER_LOG_DBG("context=%d, fd=%d, autoclose=%d, useClientMode=%d, needClientAuth=%d", (int)contextID,
	                       (int)fd, autoclose, useClientMode, needClientAuth);
	(void)hostnameLength;
	llnet_ssl_context *ctx = (llnet_ssl_context *)(contextID);

	if (NULL == ctx) {
		VEEPORT_ERRORS_throw_caught_exception(J_BAD_FUNC_ARG);
		return SNI_IGNORED_RETURNED_VALUE;
	}

	llnet_ssl_socket *sock = (llnet_ssl_socket *)mbedtls_calloc(1, sizeof(llnet_ssl_socket));
	if (NULL == sock) {
		VEEPORT_ERRORS_throw_caught_exception(J_MEMORY_ERROR);
		return SNI_IGNORED_RETURNED_VALUE;
	}

	/* It is possible to disable authentication by passing
	 * MBEDTLS_SSL_VERIFY_NONE in the call to mbedtls_ssl_conf_authmode()
	 */
	if ((0 != useClientMode) || (0 != needClientAuth)) {
		mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
	} else {
		mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_NONE);
	}

	mbedtls_ssl_init(&sock->ssl);
	sock->fd = fd;

	/* Always called, even with a NULL hostname: Mbed TLS refuses to verify a server certificate when the
	 * hostname was never set, but accepts an explicit "no hostname". */
	int ret = mbedtls_ssl_set_hostname(&sock->ssl, (const char *)hostname);
	if (0 != ret) {
		mbedtls_ssl_free(&sock->ssl);
		mbedtls_free((void *)sock);
		LLNET_SSL_DEBUG_MBEDTLS_TRACE("mbedtls_ssl_set_hostname", ret);
		LLNET_SSL_throw_mbedtls_error(ret, "MbedTLS SSL hostname setup failed");
		return SNI_IGNORED_RETURNED_VALUE;
	}

	ret = mbedtls_ssl_setup(&sock->ssl, &ctx->conf);
	if (0 != ret) {
		mbedtls_ssl_free(&sock->ssl);
		mbedtls_free((void *)sock);
		LLNET_SSL_DEBUG_MBEDTLS_TRACE("mbedtls_ssl_setup", ret);
		LLNET_SSL_throw_mbedtls_error(ret, "MbedTLS SSL context setup failed");
		return SNI_IGNORED_RETURNED_VALUE;
	}

	mbedtls_ssl_set_bio(&sock->ssl, (void *)&sock->fd, LLNET_SSL_utils_mbedtls_send, LLNET_SSL_utils_mbedtls_recv,
	                    NULL);

	return (int32_t)sock;
}

void LLNET_SSL_SOCKET_IMPL_shutdown(int32_t sslID, int32_t fd, bool autoclose, int64_t absoluteJavaStartTime,
                                    int32_t relativeTimeout) {
	VEEPORT_LOGGER_LOG_DBG("ssl=%d, fd=%d, autoclose=%d", (int)sslID, (int)fd, autoclose);

	mbedtls_ssl_context *ssl_ctx = llnet_ssl_get_ssl_context(sslID);
	if (NULL == ssl_ctx) {
		VEEPORT_ERRORS_throw_caught_exception(J_BAD_FUNC_ARG);
		return;
	}

	//send close notify
	int ret = mbedtls_ssl_close_notify(ssl_ctx);
	if (ret < 0) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
		    ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			LLNET_SSL_DEBUG_MBEDTLS_TRACE("mbedtls_ssl_close_notify", ret);
		}
	}

	if (!autoclose) {
		//the close of the underlying socket is not requested
		//read close_notify alert to clear input stream
		int8_t buffer[1];
		int32_t res;
		int64_t absoluteTimeout = (0 == relativeTimeout) ? 0 : absoluteJavaStartTime + relativeTimeout;
		do{
			res = LLNET_SSL_SOCKET_read(sslID, fd, buffer, 0, 1, absoluteJavaStartTime, absoluteTimeout,
			                            relativeTimeout, (SNI_callback)LLNET_SSL_SOCKET_close_callback);
			absoluteTimeout = (0 == relativeTimeout) ? 0 : LLNET_current_time_ms() + relativeTimeout;
		}while (res > 0);
	}
	VEEPORT_LOGGER_LOG_DBG("ssl=%d, fd=%d, autoclose=%d: free SSL", (int)sslID, (int)fd, (int)autoclose);
}

void LLNET_SSL_SOCKET_IMPL_freeSSL(int32_t sslID) {
	VEEPORT_LOGGER_LOG_DBG("ssl=%d", (int)sslID);

	llnet_ssl_socket *sock = (llnet_ssl_socket *)(sslID);
	if (NULL == sock) {
		VEEPORT_ERRORS_throw_caught_exception(J_BAD_FUNC_ARG);
		return;
	}

	/* The BIO context is the fd embedded in the socket: freeing the socket frees it. */
	mbedtls_ssl_free(&sock->ssl);
	mbedtls_free((void *)sock);
}

void LLNET_SSL_SOCKET_IMPL_initialClientHandShake(int32_t sslID, int32_t fd, int64_t absoluteJavaStartTime,
                                                  int32_t relativeTimeout) {
	VEEPORT_LOGGER_LOG_DBG("ssl=%d, fd=%d", (int)sslID, (int)fd);
	int64_t absoluteTimeout = (0 == relativeTimeout) ? 0 : absoluteJavaStartTime + relativeTimeout;
	LLNET_SSL_SOCKET_initialHandShake(sslID, fd, absoluteJavaStartTime, absoluteTimeout, relativeTimeout);
}

void LLNET_SSL_SOCKET_IMPL_initialServerHandShake(int32_t sslID, int32_t fd, int64_t absoluteJavaStartTime,
                                                  int32_t relativeTimeout) {
	VEEPORT_LOGGER_LOG_DBG("ssl=%d, fd=%d", (int)sslID, (int)fd);
	int64_t absoluteTimeout = (0 == relativeTimeout) ? 0 : absoluteJavaStartTime + relativeTimeout;
	LLNET_SSL_SOCKET_initialHandShake(sslID, fd, absoluteJavaStartTime, absoluteTimeout, relativeTimeout);
}

int32_t LLNET_SSL_SOCKET_IMPL_read(int32_t sslID, int32_t fd, int8_t *buf, int32_t off, int32_t len,
                                   int64_t absoluteJavaStartTime, int32_t relativeTimeout) {
	VEEPORT_LOGGER_LOG_DBG("ssl=%d, fd=%d, offset=%d, length=%d", (int)sslID, (int)fd, (int)off, (int)len);

	mbedtls_ssl_context *ssl_ctx = llnet_ssl_get_ssl_context(sslID);
	if (NULL == ssl_ctx) {
		VEEPORT_ERRORS_throw_caught_exception(J_BAD_FUNC_ARG);
		return SNI_IGNORED_RETURNED_VALUE;
	}
	int64_t absoluteTimeout = (0 == relativeTimeout) ? 0 : absoluteJavaStartTime + relativeTimeout;
	return LLNET_SSL_SOCKET_read(sslID, fd, buf, off, len, absoluteJavaStartTime, absoluteTimeout, relativeTimeout,
	                             (SNI_callback)LLNET_SSL_SOCKET_read_callback);
}

int32_t LLNET_SSL_SOCKET_IMPL_write(int32_t sslID, int32_t fd, int8_t *buf, int32_t off, int32_t len,
                                    int64_t absoluteJavaStartTime, int32_t relativeTimeout) {
	VEEPORT_LOGGER_LOG_DBG("ssl=%d, fd=%d, offset=%d, length=%d", (int)sslID, (int)fd, (int)off, (int)len);
	mbedtls_ssl_context *ssl_ctx = llnet_ssl_get_ssl_context(sslID);
	if (NULL == ssl_ctx) {
		VEEPORT_ERRORS_throw_caught_exception(J_BAD_FUNC_ARG);
		return SNI_IGNORED_RETURNED_VALUE;
	}
	int64_t absoluteTimeout = (0 == relativeTimeout) ? 0 : absoluteJavaStartTime + relativeTimeout;
	return LLNET_SSL_SOCKET_write(sslID, fd, buf, off, len, absoluteJavaStartTime, absoluteTimeout, relativeTimeout);
}

int32_t LLNET_SSL_SOCKET_IMPL_available(int32_t sslID) {
	VEEPORT_LOGGER_LOG_DBG("ssl=%d", (int)sslID);

	mbedtls_ssl_context *ssl_ctx = llnet_ssl_get_ssl_context(sslID);
	if (NULL == ssl_ctx) {
		VEEPORT_ERRORS_throw_caught_exception(J_BAD_FUNC_ARG);
		return SNI_IGNORED_RETURNED_VALUE;
	}

	return mbedtls_ssl_get_bytes_avail(ssl_ctx);
}

#ifdef __cplusplus
}
#endif
