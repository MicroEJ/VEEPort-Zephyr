/*
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */
#ifndef LLSERIAL_UTILS_H
#define LLSERIAL_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <LLSERIAL_configuration.h>
#include <zephyr/drivers/uart.h>
#include <sni.h>
#include <stdbool.h>
#include <stdio.h> // cppcheck-suppress [misra-c2012-21.6] Include only in debug
#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Structure to store operations (RX/TX) ring buffers
 */
typedef struct {
	struct ring_buf rb;
	uint8_t buffer[CONFIG_MICROEJ_SERIAL_RX_RING_BUFFER_SIZE];
}  ring_buf_t;

/**
 * @brief Structure to store operations (RX/TX) parameters and results
 */
typedef struct {
	int32_t java_thread_id;
	uint8_t *data;
	int32_t length;
	int32_t ret;
} serial_operation_ctx_t;

/**
 * @brief Structure to store ABLA context
 */
typedef struct {
	// Open operations
	uint32_t handle;
	const uint8_t *name;
	bool is_open;
	bool close_in_progress;
	int32_t close_java_thread_id;
	// Read operations
	serial_operation_ctx_t rx;
	uint8_t(*rx_buffer)[CONFIG_MICROEJ_SERIAL_RX_BUFFER_SIZE];
	uint16_t rx_buffer_id;
	ring_buf_t rx_ring_buffer;
	// Write operations
	serial_operation_ctx_t tx;
	uint8_t *tx_buffer;
	const struct device *async_adapter;
} serial_ctx_t;

/**
 * @brief Backend implementations
 */
int32_t serial_init();
int32_t serial_open(uint8_t *port_name);
int32_t serial_configure(serial_ctx_t *serial, int32_t baudrate, int32_t databits, int32_t stopbits, int32_t parity);
int32_t serial_write(serial_ctx_t *serial, uint8_t *buff, int32_t size);
int32_t serial_read(serial_ctx_t *serial, uint8_t *buff, int32_t size);
int32_t serial_available(serial_ctx_t *serial);

/**
 * @brief macro used to mark unused function parameters for MISRA compliance
 */
#define LLSERIAL_UNUSED_PARAM(x)                   ((void)(x))

/**
 * @brief macro used to mark function calls for which the return value is ignored (MISRA compliance)
 */
#define LLSERIAL_UNUSED_RETURN(x)                  (void)(x)

/**
 * @brief throw native IO Exception with ignored return value (MISRA compliance)
 */
#define LLSERIAL_THROW_NATIVE_IO_EXCEPTION(...)    ((void)SNI_throwNativeIOException(__VA_ARGS__))

/**
 * @brief throw native Exception with ignored return value (MISRA compliance)
 */
#define LLSERIAL_THROW_NATIVE_EXCEPTION(...)       ((void)SNI_throwNativeException(__VA_ARGS__))

#define LLSERIAL_SNI_SUSPEND_WITH_CALLBACK(a, b, c) {                            \
			do {                                                                 \
				int32_t ret = SNI_suspendCurrentJavaThreadWithCallback(a, b, c); \
				__ASSERT(SNI_OK == ret, "SNI suspend returned %d", ret);         \
			} while (0);                                                         \
}

#define LLSERIAL_RETCODE_ERROR                     (-1)
#define LLSERIAL_RETCODE_OK                        (0)

#ifdef __cplusplus
}
#endif
#endif // LLSERIAL_UTILS_H
