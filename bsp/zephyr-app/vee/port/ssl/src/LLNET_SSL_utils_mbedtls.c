/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

#include <mbedtls/build_info.h>
#include <mbedtls/platform.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/x509_crt.h>
#include "LLNET_Common.h"
#include "LLNET_SSL_mbedtls_configuration.h"
#include "LLNET_SSL_utils_mbedtls.h"
#include "LLNET_SSL_ERRORS.h"
#include "async_select.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

VEEPORT_LOGGER_DECLARE_MODULE(LLNET_SSL_LOG_MODULE)

#ifdef __cplusplus
extern "C" {
#endif

/* ---- constants ---- */

/**
 * @brief null terminated character
 */
#define NULL_CHARACTER '\0'

/* ---- external functions ---- */
/**
 * @brief External function used to retrieve currentTime (defined in LLMJVM)
 */
extern int64_t LLMJVM_IMPL_getCurrentTime__Z(uint8_t system);

/*
 * Used to check whether the previous call of the underlying read/write bio was succeeded.
 * As mbedtls_ssl_read()/mbedtls_ssl_write() may cause several underlying bio read/write call,
 * we need to check if the previous call of the underlying read/write bio was succeeded.
 * In the case where the previous bio call succeeded, the absolute timeout of the next bio call will be updated to
 * currentTime + relativeTimeout.
 */
static bool previous_bio_call_succeeded = false;

/* ---- private functions ---- */
static uint8_t * microej_get_str_from_array(uint8_t *array, uint32_t offset, uint32_t *len) {
	uint8_t *p_str;

	p_str = (uint8_t *)array + offset;

	/* Check if array is null terminated */
	if (NULL_CHARACTER != p_str[*len - 1]) {
		int32_t trueLength = SNI_getArrayLength(array) - offset;

		/* len should be one byte longer to store the null terminated key */
		(*len)++;

		/* Check if there is enough space in the byte array to insert a null character */
		if (trueLength > *len) {
			p_str[*len - 1] = NULL_CHARACTER;
		} else {
			/*
			 * Duplicate the array but with a null terminated character,
			 * /!\ this new string must be freed
			 */

			p_str = mbedtls_calloc(1, *len);

			if (NULL == p_str) {
				return NULL;
			}

			memcpy((void *)p_str, (void *)(array + offset), *len - 1);
			p_str[*len - 1] = NULL_CHARACTER;
		}
	}

	return (uint8_t *)p_str;
}

/* ---- Specific C Time function ---- */

time_t LLNET_SSL_utils_custom_mbedtls_time(time_t *time) {
	time_t currentTime = (time_t)(LLMJVM_IMPL_getCurrentTime__Z(0) / 1000);

	if (NULL != time) {
		*time = currentTime;
	}

	return currentTime;
}

/* ---- Specific net layer connection functions ---- */

int LLNET_SSL_utils_mbedtls_recv(void *ctx, unsigned char *buf, size_t len) {
	VEEPORT_LOGGER_LOG_DBG("ctx=%d, length=%d", (int)ctx, (int)len);
	int recv_bytes = -1;

	int fd = *((int *)ctx);

	if (fd < 0) {
		return(MBEDTLS_ERR_NET_INVALID_CONTEXT);
	}

	recv_bytes = llnet_recv(fd, buf, len, 0);
	int netError = llnet_errno(fd);

	VEEPORT_LOGGER_LOG_DBG("RECV (ctx=%d, length=%d) bytes read=%d, netError=%d", (int)ctx, (int)len, (int)recv_bytes,
	                       (int)netError);

	if (recv_bytes < 0) {
		if (EPIPE == netError || ECONNRESET == netError) {
			return(MBEDTLS_ERR_NET_CONN_RESET);
		}

		if ((EINTR == netError) || (EAGAIN == netError) || (EWOULDBLOCK == netError)) {
			return(MBEDTLS_ERR_SSL_WANT_READ);
		}

		return(MBEDTLS_ERR_NET_RECV_FAILED);
	}
	previous_bio_call_succeeded = true;
	return recv_bytes;
}

int LLNET_SSL_utils_mbedtls_send(void *ctx, const unsigned char *buf, size_t len) {
	VEEPORT_LOGGER_LOG_DBG("ctx=%d, length=%d", (int)ctx, (int)len);
	int size_sent = -1;

	int fd = *((int *)ctx);
	if (fd < 0) {
		return(MBEDTLS_ERR_NET_INVALID_CONTEXT);
	}

	size_sent = llnet_send(fd, buf, len, 0);
	int netError = llnet_errno(fd);

	VEEPORT_LOGGER_LOG_DBG("SEND (ctx=%d, length=%d) bytes sent=%d netError=%d", (int)ctx, (int)len, (int)size_sent,
	                       (int)netError);

	if (size_sent < 0) {
		if (netError == EPIPE || netError == ECONNRESET) {
			return(MBEDTLS_ERR_NET_CONN_RESET);
		}

		if ((EINTR == netError) || (EAGAIN == netError) || (EWOULDBLOCK == netError)) {
			return(MBEDTLS_ERR_SSL_WANT_WRITE);
		}

		return(MBEDTLS_ERR_NET_SEND_FAILED);
	}
	previous_bio_call_succeeded = true;
	return size_sent;
}

/* ---- mbedtls custom function for error printing ---- */
#if defined(CONFIG_MBEDTLS_DEBUG)
void LLNET_SSL_utils_print_mbedtls_error(const char *name, int err) {
	char buf[128];
	mbedtls_strerror(err, buf, sizeof (buf));
	VEEPORT_LOGGER_LOG_DBG("%s() failed: -0x%04x (%d): %s", name, -err, err, buf);
}

#endif // defined(CONFIG_MBEDTLS_DEBUG)

int LLNET_SSL_utils_mbedtls_x509_crt_parse(mbedtls_x509_crt *cert, uint8_t *array, uint32_t offset, uint32_t len) {
	VEEPORT_LOGGER_LOG_DBG("");
	uint8_t *strCert = microej_get_str_from_array(array, offset, &len);

	if (NULL == strCert) {
		return J_MEMORY_ERROR;
	}

	int ret = mbedtls_x509_crt_parse(cert, strCert, (uint32_t)len);

	// Free strCert if it has been allocated by microej_get_str_from_array
	if (strCert != (array + offset)) {
		mbedtls_free(strCert);
	}

	return LLNET_SSL_TranslateReturnCode(ret);
}

void LLNET_SSL_utils_mbedtls_on_bio_start(void) {
	//mbedtls_ssl_read(), mbedtls_ssl_write() or mbedtls_ssl_handshake() is going to be called.
	//This means that a new series of bio read/write calls will be started
	//reset the variable used to check the previous bio call
	previous_bio_call_succeeded = false;
}

int64_t LLNET_SSL_utils_mbedtls_update_next_bio_timeout(int64_t absoluteJavaStartTime, int64_t absoluteTimeout,
                                                        int32_t relativeTimeout, uint32_t *delta32) {
	if (0 != absoluteTimeout && previous_bio_call_succeeded) {
		//The previous underlying BIO read/write call succeeded, then the absolute timeout should be updated for the
		// next BIO read/write call.
		//The new absolute timeout will be currentTime + relativeTimeout
		//compute the new absolute read timeout
		int64_t currentTime = LLNET_current_time_ms();
		absoluteTimeout = currentTime + relativeTimeout;
		//compute the time difference between the start time of the native call from Java and the current time.
		int64_t delta64 = currentTime - absoluteJavaStartTime;
		// Saturate delta on uinsigned 32-bit
		*delta32 = (uint32_t)(delta64 > UINT32_MAX? UINT32_MAX : delta64);
	}
	return absoluteTimeout;
}

void LLNET_SSL_utils_mbedtls_handle_IO_error(mbedtls_ssl_context *ssl_context, int32_t mbledtls_error,
                                             bool fromHandShake, int32_t fd, int64_t absolute_timeout_ms,
                                             SNI_callback callback, void *callback_suspend_arg) {
	uint8_t check_timeout = MBEDTLS_ERR_SSL_WANT_READ == mbledtls_error && absolute_timeout_ms != 0;

	//check if timeout occurs (only on read)
	if (check_timeout && absolute_timeout_ms < LLNET_current_time_ms()) {
		//timeout occurs
		VEEPORT_ERRORS_throw_caught_exception(J_SOCKET_TIMEOUT);
		return;
	}

	if (MBEDTLS_ERR_SSL_WANT_READ == mbledtls_error) {
		async_select(fd, SELECT_READ, absolute_timeout_ms, callback, callback_suspend_arg);
		return;
	}
	if (MBEDTLS_ERR_SSL_WANT_WRITE == mbledtls_error) {
		async_select(fd, SELECT_WRITE, absolute_timeout_ms, callback, callback_suspend_arg);
		return;
	}
	LLNET_SSL_DEBUG_MBEDTLS_TRACE("mbedtls operation", mbledtls_error);

	if (fromHandShake) {
		/* SSL Context unusable */
		mbedtls_ssl_session_reset(ssl_context);
	}
	LLNET_SSL_throw_mbedtls_error(mbledtls_error, "MbedTLS ssl operation failed");
}

void LLNET_SSL_throw_caught_exception(int32_t error_code, const char *message) {
	if (J_UNKNOWN_ERROR == error_code) {
		VEEPORT_ERRORS_throw_caught_exception_message(error_code, "%s", message);
	} else {
		VEEPORT_LOGGER_LOG_DBG("%s (%d)", message, (int)error_code);
		VEEPORT_ERRORS_throw_caught_exception(error_code);
	}
}

void LLNET_SSL_throw_mbedtls_error(int32_t mbedtls_error, const char *message) {
	int32_t error_code = LLNET_SSL_TranslateReturnCode(mbedtls_error);
	if (J_UNKNOWN_ERROR == error_code) {
		VEEPORT_ERRORS_throw_caught_exception_message(error_code, "%s: %d", message, (int)mbedtls_error);
	} else {
		VEEPORT_LOGGER_LOG_DBG("%s: %d", message, (int)mbedtls_error);
		VEEPORT_ERRORS_throw_caught_exception(error_code);
	}
}

#ifdef __cplusplus
}
#endif
