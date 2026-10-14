/*
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */
#include <LLSERIAL_CONNECTION_impl.h>
#include <LLSERIAL_utils.h>
#include <string.h>

VEEPORT_LOGGER_DECLARE_MODULE(LLSERIAL_LOG_MODULE)

// -----------------------------------------------------------------------------
// Internal function definitions
// -----------------------------------------------------------------------------

static serial_ctx_t *throw_if_not_open(int32_t connection_id) {
	serial_ctx_t *serial = (serial_ctx_t *)connection_id;

	if (NULL == serial) {
		VEEPORT_LOGGER_LOG_DBG(LLSERIAL_CONNECTION_INVALID_PORT);
		VEEPORT_ERRORS_throw_runtime_exception(RUNTIME_EXCEPTION_INVALID_ARGUMENT);
		serial = NULL;
	} else if (false == serial->is_open) {
		VEEPORT_ERRORS_throw_caught_exception_message(LLSERIAL_IOEXCEPTION_GENERIC,
		                                              LLSERIAL_CONNECTION_CONNECTION_CLOSED);
		serial = NULL;
	} else {
		// serial is valid and open
	}

	return serial;
}

// -----------------------------------------------------------------------------
// LLSERIAL_CONNECTION_impl.h functions
// -----------------------------------------------------------------------------

/**
 * Initializes the native stack.
 */
void LLSERIAL_CONNECTION_IMPL_init(void) {
	VEEPORT_LOGGER_LOG_DBG("");

	int32_t result = serial_init();
	if (LLSERIAL_RETCODE_ERROR == result) {
		VEEPORT_ERRORS_throw_caught_exception_message(LLSERIAL_IOEXCEPTION_GENERIC, LLSERIAL_CONNECTION_INTERNAL_ERROR);
	}
}

int32_t LLSERIAL_CONNECTION_IMPL_open(uint8_t *port_name) {
	VEEPORT_LOGGER_LOG_DBG("port_name = %s", port_name);

	int32_t connection_id = serial_open(port_name);
	if (LLSERIAL_RETCODE_ERROR != connection_id) {
		LLSERIAL_UNUSED_RETURN(SNI_registerResource((void *)connection_id,
		                                            (SNI_closeFunction)LLSERIAL_CONNECTION_IMPL_close, NULL));
	}

	VEEPORT_LOGGER_LOG_DBG("connection_id = %d", connection_id);
	return connection_id;
}

void LLSERIAL_CONNECTION_IMPL_configure(int32_t connection_id, int32_t baudrate, int32_t databits, int32_t parity,
                                        int32_t stopbits) {
	VEEPORT_LOGGER_LOG_DBG("connection_id = %d, baudrate = %d, databits = %d, stopbits = %d, parity = %d",
	                       connection_id, baudrate, databits, stopbits, parity);

	serial_ctx_t *serial = (serial_ctx_t *)connection_id;
	if (NULL != serial) {
		int32_t result = serial_configure(serial, baudrate, databits, stopbits, parity);
		if (LLSERIAL_RETCODE_ERROR == result) {
			VEEPORT_LOGGER_LOG_ERR("configuration error %d", result);
		}
	} else {
		VEEPORT_LOGGER_LOG_DBG(LLSERIAL_CONNECTION_INVALID_PORT);
		VEEPORT_ERRORS_throw_runtime_exception(RUNTIME_EXCEPTION_INVALID_ARGUMENT);
	}
}

int32_t LLSERIAL_CONNECTION_IMPL_read(int32_t connection_id, uint8_t *b, int32_t off, int32_t len) {
	VEEPORT_LOGGER_LOG_DBG("connection_id = %d, off = %d, len = %d", connection_id, off, len);
	int32_t result = LLSERIAL_RETCODE_OK;

	if (len > 0) {
		serial_ctx_t *serial = throw_if_not_open(connection_id);
		if (NULL != serial) {
			result = serial_read(serial, &b[off], len);
			if (0 == result) {
				serial->rx.java_thread_id = SNI_getCurrentJavaThreadID();
				LLSERIAL_SNI_SUSPEND_WITH_CALLBACK(0, (SNI_callback)LLSERIAL_CONNECTION_IMPL_read, NULL);
			} else if (LLSERIAL_RETCODE_ERROR == result) {
				VEEPORT_ERRORS_throw_caught_exception_message(LLSERIAL_IOEXCEPTION_GENERIC,
				                                              LLSERIAL_CONNECTION_RX_ERROR);
				result = SNI_IGNORED_RETURNED_VALUE;
			}
		}
	}

	VEEPORT_LOGGER_LOG_DBG("return value = %d", result);
	return result;
}

void LLSERIAL_CONNECTION_IMPL_write(int32_t connection_id, uint8_t *b, int32_t off, int32_t len) {
	VEEPORT_LOGGER_LOG_DBG("connection_id = %d, off = %d, len = %d", connection_id, off, len);
	int32_t result = LLSERIAL_RETCODE_OK;

	serial_ctx_t *serial = throw_if_not_open(connection_id);
	if (NULL != serial) {
		int32_t ret = serial->tx.ret;
		if (ret == LLSERIAL_RETCODE_ERROR) {
			serial->tx.ret = 0;

			VEEPORT_ERRORS_throw_caught_exception_message(LLSERIAL_IOEXCEPTION_GENERIC, LLSERIAL_CONNECTION_TX_ERROR);
		} else if (ret < len) {
			result = SNI_IGNORED_RETURNED_VALUE;
			uint32_t remaining_length = len - ret;

			remaining_length = (remaining_length >
			                    sizeof(serial->tx_buffer)) ? sizeof(serial->tx_buffer) : remaining_length;
			int32_t new_off = off + ret;

			result = SNI_retrieveArrayElements((jbyte *)b, new_off, remaining_length, (int8_t *)&serial->tx_buffer[0],
			                                   remaining_length, (int8_t **)&serial->tx.data,
			                                   (uint32_t *)&serial->tx.length, true);
			if (SNI_OK != result) {
				VEEPORT_LOGGER_LOG_ERR("Failed to retrieve SNI array elements of addr %p and offset %d (error %d)",
				                       (void *)b, new_off, result);
				VEEPORT_ERRORS_throw_caught_exception_message(LLSERIAL_IOEXCEPTION_GENERIC,
				                                              LLSERIAL_CONNECTION_INTERNAL_ERROR);
				result = SNI_IGNORED_RETURNED_VALUE;
			} else {
				serial->tx.java_thread_id = SNI_getCurrentJavaThreadID();
				LLSERIAL_SNI_SUSPEND_WITH_CALLBACK(0, (SNI_callback)LLSERIAL_CONNECTION_IMPL_write, NULL);
				result = serial_write(serial, serial->tx.data, serial->tx.length);
				if (LLSERIAL_RETCODE_ERROR == result) {
					VEEPORT_ERRORS_throw_caught_exception_message(LLSERIAL_IOEXCEPTION_GENERIC,
					                                              LLSERIAL_CONNECTION_INTERNAL_ERROR);
				}
			}
		} else {
			serial->tx.ret = 0;
		}

		VEEPORT_LOGGER_LOG_DBG("write length = %d", serial->tx.ret);
	}
}

int32_t LLSERIAL_CONNECTION_IMPL_available(int32_t connection_id) {
	int32_t result = LLSERIAL_RETCODE_OK;

	serial_ctx_t *serial = throw_if_not_open(connection_id);
	if (NULL != serial) {
		result = serial_available(serial);
		if (0 > result) {
			VEEPORT_ERRORS_throw_caught_exception_message(LLSERIAL_IOEXCEPTION_GENERIC,
			                                              LLSERIAL_CONNECTION_INTERNAL_ERROR);
		}
	}

	VEEPORT_LOGGER_LOG_DBG("return value = %d", result);
	return result;
}

void LLSERIAL_CONNECTION_IMPL_flush(int32_t connection_id) {
	VEEPORT_LOGGER_LOG_DBG("connection_id = %d", connection_id);

	serial_ctx_t *serial = throw_if_not_open(connection_id);
	LLSERIAL_UNUSED_PARAM(serial);
	// Serial port is flushed immediately when write() is called, nothing to do.
}

void LLSERIAL_CONNECTION_IMPL_close(int32_t connection_id) {
	VEEPORT_LOGGER_LOG_DBG("connection_id = %d", connection_id);

	serial_ctx_t *serial = (serial_ctx_t *)connection_id;
	if (NULL == serial) {
		VEEPORT_LOGGER_LOG_DBG(LLSERIAL_CONNECTION_INVALID_PORT);
		VEEPORT_ERRORS_throw_runtime_exception(RUNTIME_EXCEPTION_INVALID_ARGUMENT);
		return;
	}

	if (false == serial->is_open) {
		return;
	}

	const struct device *uart = (const struct device *)serial->handle;
	int err;

	if (serial->close_in_progress) {
		// SNI Callback: UART_RX_DISABLED received, finalize close
		goto finalize_close;
	}

	// First call to close method: start close sequence
	LLSERIAL_UNUSED_RETURN(SNI_unregisterResource((void *)connection_id,
	                                              (SNI_closeFunction)LLSERIAL_CONNECTION_IMPL_close));

	serial->close_in_progress = true;
	serial->close_java_thread_id = SNI_getCurrentJavaThreadID();

	err = uart_rx_disable(uart);
	if (0 == err) {
		int32_t sni_ret = SNI_suspendCurrentJavaThreadWithCallback(0, (SNI_callback)LLSERIAL_CONNECTION_IMPL_close,
		                                                           NULL);
		if (SNI_OK == sni_ret) {
			return;     // Suspended: re-entry will finalize when UART_RX_DISABLED fires
		}
		// SNI suspend failed (e.g. GC context): fall through to immediate close
		VEEPORT_LOGGER_LOG_DBG("SNI suspend failed %d, closing immediately", sni_ret);
	} else if (-EFAULT != err) {     // Ignore -EFAULT: RX already disabled
		VEEPORT_LOGGER_LOG_ERR("Serial uart_rx_disable error %d (handle %p)", err,
		                       uart);
		VEEPORT_ERRORS_throw_caught_exception_message(LLSERIAL_IOEXCEPTION_GENERIC, LLSERIAL_CONNECTION_INTERNAL_ERROR);
	}

	// Immediate close: RX already disabled (-EFAULT), error, or SNI suspend failed

finalize_close:
	serial->close_in_progress = false;
	serial->close_java_thread_id = -1;
	err = uart_tx_abort(uart);
	if ((0 > err) && (-EFAULT != err)) {     // Ignore -EFAULT: no active transmission
		VEEPORT_LOGGER_LOG_ERR("Serial uart_tx_abort error %d (handle %p)", err, uart);
		VEEPORT_ERRORS_throw_caught_exception_message(LLSERIAL_IOEXCEPTION_GENERIC, LLSERIAL_CONNECTION_INTERNAL_ERROR);
	}
	serial->is_open = false;
	SNI_resumeJavaThread(serial->tx.java_thread_id);
	SNI_resumeJavaThread(serial->rx.java_thread_id);
}

int64_t LLSERIAL_CONNECTION_IMPL_getCloseFunction(int32_t connectionId) {
	LLSERIAL_UNUSED_PARAM(connectionId);
	return (int64_t)(intptr_t)LLSERIAL_CONNECTION_IMPL_close;
}
