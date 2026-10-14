/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

#ifndef LLNET_SSL_UTILS_MBEDTLS
#define LLNET_SSL_UTILS_MBEDTLS

#include <mbedtls/build_info.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/ssl.h>
#include "LLNET_SSL_CONSTANTS.h"
#include "LLNET_SSL_mbedtls_configuration.h"
#include "LLNET_SSL_ERRORS.h"
#include "sni.h"
#include <time.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The verbosity of mbedtls follows the verbosity of this layer: its handshake traces are only
 * useful next to the traces of the calls that drive it. Reporting an mbedtls error is not gated,
 * because it only happens on a failure path and the logger drops the message on its own when the
 * level does not call for it.
 */
#if VEEPORT_LOGGER_MODULE_LEVEL(SSL) >= VEEPORT_LOGGER_LEVEL_DBG
#define MBEDTLS_DEBUG_LEVEL 2
#endif

#if defined(CONFIG_MBEDTLS_DEBUG)
#define LLNET_SSL_DEBUG_MBEDTLS_TRACE(name, err) LLNET_SSL_utils_print_mbedtls_error((name), (err))
#else
#define LLNET_SSL_DEBUG_MBEDTLS_TRACE(name, err)
#endif // defined(CONFIG_MBEDTLS_DEBUG)

/* ---- Specific C Time function ---- */
/*
 * Time function to be used to get current JVM application time for mbedtls.
 * In your mbedtls configuration file, you will need to set the macro MBEDTLS_PLATFORM_TIME_MACRO with this time
 * function.
 *
 *
 * Example: Add the following macro definition in your mbedtls configuration file:
 * 		#include <time.h>
 * 		extern time_t LLNET_SSL_utils_custom_mbedtls_time(time_t *time);
 * 		#define MBEDTLS_PLATFORM_TIME_MACRO   LLNET_SSL_utils_custom_mbedtls_time
 *
 * @Warning: the mbedtls configuration file we are talking about here is NOT this CCO configuration file
 * LLNET_SSL_mbedtls_configuration.h.
 * By default, the mbedtls configuration file is mbedtls/config.h and the user can override it
 * by defining the macro MBEDTLS_CONFIG_FILE to use its own configuration file.
 *
 * @param[out] time pointer to time_t struct that will be updated with the current time in seconds
 * @return the current time in seconds
 */
time_t LLNET_SSL_utils_custom_mbedtls_time(time_t *time);

/* ---- Specific net layer connection functions ---- */

/*
 * Net receive layer adaptation function for mbedtls.
 *
 * @param[in] ctx Net context (pointer to the socket fd)
 * @param[out] buf buffer for receiving data
 * @param[in] len length to receive
 *
 * @return the number of bytes received if successful or an mbedtls negative error code
 *
 * @note this function returns MBEDTLS_ERR_SSL_WANT_READ if the read operation would block (no available data to read).
 */
int LLNET_SSL_utils_mbedtls_recv(void *ctx, unsigned char *buf, size_t len);

/*
 * Net send layer adaptation function for mbedtls.
 *
 * @param[in] ctx Net context (pointer to the socket fd)
 * @param[in] buf buffer of data to send
 * @param[in] len length to send
 *
 * @return the number of bytes sent if successful or an mbedtls negative error code
 *
 * @note this function returns MBEDTLS_ERR_SSL_WANT_WRITE if the send operation would block.
 */
int LLNET_SSL_utils_mbedtls_send(void *ctx, const unsigned char *buf, size_t len);

/**
 * Helper for pretty-printing mbedtls error codes
 *
 * param[in] function_name the name of the function in which the error occured
 * param[in] mbedtls_error the mbedtls error code
 */
void LLNET_SSL_utils_print_mbedtls_error(const char *function_name, int mbedtls_error);

/* ---- Parsing certificate helper ---- */

/**
 * Parse PEM-encoded Certificate.
 * PEM certificate content should be null-terminated otherwise mbedtls_x509_crt_parse() method call will fail.
 * This method wrap mbedtls_x509_crt_parse() by adding a terminating null byte ('\0') to the certificate data
 * if there is none before calling mbedtls_x509_crt_parse().
 *
 * @param[in] cert the chain to which to add the parsed certificate.
 * @param[in] array the buffer holding the certificate data.
 * @param[in] offset the offset in the buffer at which the certificate data started.
 * @param[in] len the certificate data length.
 *
 * @return 0 on success or negative value on error.
 *
 * @see LLNET_SSL_ERRORS.h.
 */
int LLNET_SSL_utils_mbedtls_x509_crt_parse(mbedtls_x509_crt *cert, uint8_t *array, uint32_t offset, uint32_t len);

/*
 * As mbedtls_ssl_read(), mbedtls_ssl_write() or mbedtls_ssl_handshake()
 * may cause several underlying bio read/write call,
 * this function is called just before mbedtls_ssl_read()/mbedtls_ssl_write()/mbedtls_ssl_handshake()
 * to notify that a new series of underlying bio read/write will be started.
 */
void LLNET_SSL_utils_mbedtls_on_bio_start(void);

/*
 * Updates the absolute timeout for the next underlying bio read/write.
 *
 * If the previous bio read/write was not succeeded, the current absolute timeout <code>absoluteTimeout</code>
 * is returned and will be used for the next bio call.
 * A new absolute timeout is returned only if the given <code>absoluteTimeout</code> is not null (!= 0)
 * and the previous bio read/write was succeeded. This new absolute timeout is computed by adding the
 *<code>relativeTimeout</code> to the current system time <code>LLMJVM_IMPL_getCurrentTime(1)</code>.
 *
 * @param[in] absoluteJavaStartTime the absolute start time of the read/write/handshake call from Java. This time is
 * computed from the system time returned by <code>LLMJVM_IMPL_getCurrentTime(1)</code>.
 * @param[in] absoluteTimeout the current absolute timeout in milliseconds. This time is computed from the system time
 * returned by <code>LLMJVM_IMPL_getCurrentTime(1)</code>.
 * @param[in] relativeTimeout the relative timeout in milliseconds or 0 if no timeout.
 * @param[out] delta32 the time difference between <code>absoluteJavaStartTime</code> and the current time when the
 * timeout is updated.
 *
 * @return the new absolute timeout value or the current one <code>absoluteTimeout</code> if there is no update.
 */
int64_t LLNET_SSL_utils_mbedtls_update_next_bio_timeout(int64_t absoluteJavaStartTime, int64_t absoluteTimeout,
                                                        int32_t relativeTimeout, uint32_t *delta32);

/**
 * Handles the mbedtls error code of a non-blocking I/O operation.
 * This function is called only when a non-blocking mbedtls_ssl_handshake(), mbedtls_ssl_read() or mbedtls_ssl_write()
 * results in an error.
 * It checks first the given absolute timeout and throws an SNI NativeIOException if the timeout is reached.
 *
 * <code>absolute_timeout_ms</code> is an absolute time in milliseconds computed from the system time returned by
 * <code>LLMJVM_IMPL_getCurrentTime(1)</code>. A timeout of zero is interpreted as an infinite timeout.
 *
 * If the error code is <code>MBEDTLS_ERR_SSL_WANT_READ</code> or <code>MBEDTLS_ERR_SSL_WANT_WRITE</code>,
 * then an <code>async_select()</code> request is performed on the given <code>operation</code>; otherwise an SNI
 * NativeIOException is thrown.
 *
 * @param[in] ssl_context the ssl context
 * @param[in] mbledtls_error the mbedtls error code to be handled.
 * @param[in] fromHanshake tells whether the error occurred during a handshake
 * @param[in] fd the the file descriptor on which the error has occurred
 * @param[in] absolute_timeout_ms the absolute timeout in milliseconds or 0 if no timeout.
 * @param[in] callback the async_select SNI callback function.
 * @param[in] callback_suspend_arg the async_select SNI suspend callback argument.
 */
void LLNET_SSL_utils_mbedtls_handle_IO_error(mbedtls_ssl_context *ssl_context, int32_t mbledtls_error,
                                             bool fromHanshake, int32_t fd, int64_t absolute_timeout_ms,
                                             SNI_callback callback, void *callback_suspend_arg);

/*
 * Translates mbedtls error code to MicroEJ SSL error code
 * @param[in] mbedtls_error : Error code from mbedtls
 *
 * @return the corresponding MicroEJ SSL error code.
 */
int32_t LLNET_SSL_TranslateReturnCode(int32_t mbedtls_error);

/**
 * @brief Throws the NativeIOException of a failed SSL operation.
 *
 * The message reaches Java only with the generic code J_UNKNOWN_ERROR. With a specific code, the code
 * says what failed and the message goes to a debug log.
 *
 * @param[in] error_code one of the codes of LLNET_SSL_ERRORS.h.
 * @param[in] message    what failed.
 */
void LLNET_SSL_throw_caught_exception(int32_t error_code, const char *message);

/**
 * @brief Throws the NativeIOException of a failed Mbed TLS call, with the code
 * LLNET_SSL_TranslateReturnCode() gives. With the generic code, the message also carries the Mbed TLS error.
 *
 * @param[in] mbedtls_error the error returned by the Mbed TLS call.
 * @param[in] message       what failed.
 */
void LLNET_SSL_throw_mbedtls_error(int32_t mbedtls_error, const char *message);

#ifdef __cplusplus
}
#endif

#endif //LLNET_SSL_UTILS_MBEDTLS
