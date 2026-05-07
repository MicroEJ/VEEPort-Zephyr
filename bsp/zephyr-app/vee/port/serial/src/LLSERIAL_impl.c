/*
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */
#include <LLSERIAL_CONNECTION_impl.h>
#include <LLSERIAL_utils.h>

#ifdef CONFIG_UART_ASYNC_ADAPTER

#include <uart_async_adapter.h>
/* Create UART async adapter instance */
#ifdef CONFIG_MICROEJ_SERIAL_ENABLE_PORT_0
UART_ASYNC_ADAPTER_INST_DEFINE(async_adapter_0); // uart0
#endif // CONFIG_MICROEJ_SERIAL_ENABLE_PORT_0
#ifdef CONFIG_MICROEJ_SERIAL_ENABLE_PORT_1
UART_ASYNC_ADAPTER_INST_DEFINE(async_adapter_1); // uart1
#endif // CONFIG_MICROEJ_SERIAL_ENABLE_PORT_1

#endif // CONFIG_UART_ASYNC_ADAPTER

/*
 * DMA buffers must reside in non-cached memory on platforms that use a DMA-backed UART
 * (e.g. i.MXRT1170 CM7). The __nocache attribute places them in the .nocache section
 * when CONFIG_MICROEJ_SERIAL_NOCACHE_BUFFERS=y; on other platforms the attribute expands to nothing.
 */
#ifdef CONFIG_MICROEJ_SERIAL_NOCACHE_BUFFERS
#define LLSERIAL_NOCACHE __nocache
#else
#define LLSERIAL_NOCACHE
#endif

#ifdef CONFIG_MICROEJ_SERIAL_ENABLE_PORT_0
static LLSERIAL_NOCACHE uint8_t g_rx_dma_buf_0[CONFIG_MICROEJ_SERIAL_RX_BUFFER_NUMBER][
	CONFIG_MICROEJ_SERIAL_RX_BUFFER_SIZE];
static LLSERIAL_NOCACHE uint8_t g_tx_dma_buf_0[CONFIG_MICROEJ_SERIAL_TX_BUFFER_SIZE];
#endif // CONFIG_MICROEJ_SERIAL_ENABLE_PORT_0
#ifdef CONFIG_MICROEJ_SERIAL_ENABLE_PORT_1
static LLSERIAL_NOCACHE uint8_t g_rx_dma_buf_1[CONFIG_MICROEJ_SERIAL_RX_BUFFER_NUMBER][
	CONFIG_MICROEJ_SERIAL_RX_BUFFER_SIZE];
static LLSERIAL_NOCACHE uint8_t g_tx_dma_buf_1[CONFIG_MICROEJ_SERIAL_TX_BUFFER_SIZE];
#endif // CONFIG_MICROEJ_SERIAL_ENABLE_PORT_1

static serial_ctx_t g_serial_uart_ctx[] = {
#ifdef CONFIG_MICROEJ_SERIAL_ENABLE_PORT_0
	{ .name = (const uint8_t *)"uart0",
#ifdef CONFIG_UART_ASYNC_ADAPTER
	  .async_adapter = async_adapter_0,
#endif
	  .handle = (uint32_t)DEVICE_DT_GET(DT_ALIAS(microej_serial0)),
	  .is_open = false,
	  .close_in_progress = false,
	  .close_java_thread_id = -1,
	  .rx_buffer = g_rx_dma_buf_0,
	  .tx_buffer = g_tx_dma_buf_0,
	  .rx = { .java_thread_id = -1 },
	  .tx = { .java_thread_id = -1 } },
#endif // CONFIG_MICROEJ_SERIAL_ENABLE_PORT_0
#ifdef CONFIG_MICROEJ_SERIAL_ENABLE_PORT_1
	{ .name = (const uint8_t *)"uart1",
#ifdef CONFIG_UART_ASYNC_ADAPTER
	  .async_adapter = async_adapter_1,
#endif
	  .handle = (uint32_t)DEVICE_DT_GET(DT_ALIAS(microej_serial1)),
	  .is_open = false,
	  .close_in_progress = false,
	  .close_java_thread_id = -1,
	  .rx_buffer = g_rx_dma_buf_1,
	  .tx_buffer = g_tx_dma_buf_1,
	  .rx = { .java_thread_id = -1 },
	  .tx = { .java_thread_id = -1 } },
#endif // CONFIG_MICROEJ_SERIAL_ENABLE_PORT_1
};

static uint8_t * get_next_rx_buff(serial_ctx_t *serial) {
	uint8_t *rx_buff_addr = &serial->rx_buffer[serial->rx_buffer_id][0];
	serial->rx_buffer_id++;
	if (serial->rx_buffer_id >= CONFIG_MICROEJ_SERIAL_RX_BUFFER_NUMBER) {
		serial->rx_buffer_id = 0;
	}
	return rx_buff_addr;
}

static void resume_java_thread(int32_t *thread_id) {
	int32_t id = *thread_id;
	if (0 <= id) {
		*thread_id = SNI_ERROR;
		if (SNI_OK != SNI_resumeJavaThread(id)) {
			LLSERIAL_ERROR_TRACE("[%s:%u] SNI_resumeJavaThread error\n", __FUNCTION__, __LINE__);
		}
	}
}

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data) {
	serial_ctx_t *serial = (serial_ctx_t *)user_data;
	const struct device *uart = (const struct device *)serial->handle;

	if (serial->is_open) {
		switch (evt->type) {
		case UART_TX_DONE:
		{
			serial->tx.ret += evt->data.tx.len;
			resume_java_thread(&serial->tx.java_thread_id);
			break;
		}
		case UART_TX_ABORTED:
		{
			LLSERIAL_DEBUG_TRACE("[%s:%u] TX aborted %d bytes sent (handle %p)\n", __FUNCTION__, __LINE__,
			                     evt->data.tx.len, uart);
			serial->tx.ret = LLSERIAL_RETCODE_ERROR;
			resume_java_thread(&serial->tx.java_thread_id);
			break;
		}
		case UART_RX_RDY:
		{
			int32_t length = evt->data.rx.len;
			const uint8_t *data = &evt->data.rx.buf[evt->data.rx.offset];

			int ret = ring_buf_put(&serial->rx_ring_buffer.rb, data, length);
			if (ret != length) {
				LLSERIAL_DEBUG_TRACE("[%s:%u] RX ring buffer not enough room, partial copy (added %d, expected %d)\n",
				                     __FUNCTION__, __LINE__, ret, length);
				serial->rx.ret = LLSERIAL_RETCODE_ERROR;
			}
			resume_java_thread(&serial->rx.java_thread_id);
			break;
		}
		case UART_RX_BUF_REQUEST:
		{
			int err = uart_rx_buf_rsp(uart, get_next_rx_buff(serial), CONFIG_MICROEJ_SERIAL_RX_BUFFER_SIZE);
			if (0 > err) {
				LLSERIAL_DEBUG_TRACE("[%s:%u] uart_rx_buf_rsp error %d (handle %p)\n", __FUNCTION__, __LINE__, err,
				                     uart);
				serial->rx.ret = LLSERIAL_RETCODE_ERROR;
			}
			break;
		}
		case UART_RX_DISABLED:
		{
			resume_java_thread(&serial->rx.java_thread_id);
			if (true == serial->close_in_progress) {
				resume_java_thread(&serial->close_java_thread_id);
			}
			break;
		}
		case UART_RX_STOPPED:
		{
			LLSERIAL_DEBUG_TRACE("[%s:%u] RX error %d (handle %p)\n", __FUNCTION__, __LINE__, evt->data.rx_stop.reason,
			                     uart);
			serial->rx.ret = LLSERIAL_RETCODE_ERROR;
			break;
		}
		default:
		{
			break;     // do nothing
		}
		}
	}
}

/**
 * @brief Function to initialise ABLA
 */
int32_t serial_init() {
	int32_t result = LLSERIAL_RETCODE_OK;

	for (int32_t connection_counter = 0; connection_counter < (sizeof(g_serial_uart_ctx) / sizeof(serial_ctx_t));
	     connection_counter++) {
		serial_ctx_t *serial = &g_serial_uart_ctx[connection_counter];
		const struct device *uart = (const struct device *)serial->handle;

		if (!device_is_ready(uart)) {
			result = LLSERIAL_RETCODE_ERROR;
			LLSERIAL_ERROR_TRACE("[%s:%u] Serial device %s is not ready (handle %p)\n", __FUNCTION__, __LINE__,
			                     serial->name, uart);
		}

		if (result == LLSERIAL_RETCODE_OK) {
			ring_buf_init(&serial->rx_ring_buffer.rb, sizeof(serial->rx_ring_buffer.buffer),
			              serial->rx_ring_buffer.buffer);
#ifdef CONFIG_UART_ASYNC_ADAPTER
			uart_async_adapter_init(serial->async_adapter, uart);
			serial->handle = (uint32_t)serial->async_adapter;
#endif //CONFIG_UART_ASYNC_ADAPTER
			LLSERIAL_DEBUG_TRACE("[%s:%d] Serial init done (handle %p)\n", __FUNCTION__, __LINE__,
			                     (void *)serial->handle);
		} else {
			break;
		}
	}

	return result;
}

/**
 * @brief Function to open serial port
 */
int32_t serial_open(uint8_t *port_name) {
	int32_t result = LLSERIAL_RETCODE_OK;
	int32_t connection_id = LLSERIAL_RETCODE_ERROR;

	if (NULL != port_name) {
		for (int32_t connection_counter = 0; connection_counter < (sizeof(g_serial_uart_ctx) / sizeof(serial_ctx_t));
		     connection_counter++) {
			if (0 == strcmp((const char *)g_serial_uart_ctx[connection_counter].name, (const char *)port_name)) {
				connection_id = connection_counter;
				break;
			}
		}
	}

	if (LLSERIAL_RETCODE_ERROR != connection_id) {
		serial_ctx_t *serial = &g_serial_uart_ctx[connection_id];
		if (false == serial->is_open) {
			const struct device *uart = (const struct device *)serial->handle;
			int err = uart_callback_set(uart, uart_cb, serial);
			if (0 > err) {
				LLSERIAL_ERROR_TRACE("[%s:%u] uart_callback_set error %d (handle %p)\n", __FUNCTION__, __LINE__, err,
				                     uart);
				result = LLSERIAL_RETCODE_ERROR;
			} else {
				// Trigger first read operation and returns immediately.
				serial->rx_buffer_id = 0;
				serial->rx.ret = LLSERIAL_RETCODE_OK;
				serial->tx.ret = LLSERIAL_RETCODE_OK;
				serial->close_in_progress = false;
				serial->close_java_thread_id = -1;
				serial->is_open = true;

				err = uart_rx_enable(uart, get_next_rx_buff(serial), CONFIG_MICROEJ_SERIAL_RX_BUFFER_SIZE,
				                     CONFIG_MICROEJ_SERIAL_READ_TIMEOUT);
				if ((0 > err) && (-EBUSY != err)) { // Allow to enable during read
					LLSERIAL_ERROR_TRACE("[%s:%u] uart_rx_enable error %d (handle %p)\n", __FUNCTION__, __LINE__, err,
					                     uart);

					serial->is_open = false;
					result = LLSERIAL_RETCODE_ERROR;
				}
			}
			if ((0 < uart) && (LLSERIAL_RETCODE_ERROR != result)) {
				result = (int32_t)serial;
				LLSERIAL_DEBUG_TRACE(
					"[%s:%u] Successfully open new file descriptor %p for connection ID %d, device name %s \n",
					__FUNCTION__, __LINE__, (void *)serial->handle, connection_id, serial->name);
			} else {
				LLSERIAL_THROW_NATIVE_IO_EXCEPTION(LLSERIAL_IOEXCEPTION_GENERIC,
				                                   LLSERIAL_CONNECTION_CONFIGURATION_ERROR);
			}
		} else {
			LLSERIAL_THROW_NATIVE_IO_EXCEPTION(LLSERIAL_IOEXCEPTION_GENERIC, LLSERIAL_CONNECTION_PORT_ALREADY_USED);
		}
	} else {
		LLSERIAL_THROW_NATIVE_IO_EXCEPTION(LLSERIAL_IOEXCEPTION_GENERIC, LLSERIAL_CONNECTION_INVALID_PORT);
	}

	return result;
}

/**
 * @brief Function to configure serial port
 */
int32_t serial_configure(serial_ctx_t *serial, int32_t baudrate, int32_t databits, int32_t stopbits, int32_t parity) {
	int32_t result = LLSERIAL_RETCODE_OK;
	const struct device *uart = (const struct device *)serial->handle;
	struct uart_config uart_cfg = { .baudrate = baudrate, .flow_ctrl = UART_CFG_FLOW_CTRL_NONE };

	switch (parity) {
	case LLSERIAL_CONNECTION_PARITY_ODD:
		uart_cfg.parity = UART_CFG_PARITY_ODD;
		break;
	case LLSERIAL_CONNECTION_PARITY_EVEN:
		uart_cfg.parity = UART_CFG_PARITY_EVEN;
		break;
	case LLSERIAL_CONNECTION_PARITY_NONE:
		uart_cfg.parity = UART_CFG_PARITY_NONE;
		break;
	default:
		LLSERIAL_THROW_NATIVE_IO_EXCEPTION(LLSERIAL_IOEXCEPTION_UNSUPPORTED_CONFIG, LLSERIAL_CONNECTION_INVALID_PARITY);
		break;
	}
	switch (stopbits) {
	case LLSERIAL_CONNECTION_STOPBITS_1:
		uart_cfg.stop_bits = UART_CFG_STOP_BITS_1;
		break;
	case LLSERIAL_CONNECTION_STOPBITS_2:
		uart_cfg.stop_bits = UART_CFG_STOP_BITS_2;
		break;
	case LLSERIAL_CONNECTION_STOPBITS_1_5:
		uart_cfg.stop_bits = UART_CFG_STOP_BITS_1_5;
		break;
	default:
		LLSERIAL_THROW_NATIVE_IO_EXCEPTION(LLSERIAL_IOEXCEPTION_UNSUPPORTED_CONFIG,
		                                   LLSERIAL_CONNECTION_INVALID_STOPBITS);
		break;
	}
	switch (databits) {
	case LLSERIAL_CONNECTION_DATABITS_5:
		uart_cfg.data_bits = UART_CFG_DATA_BITS_5;
		break;
	case LLSERIAL_CONNECTION_DATABITS_6:
		uart_cfg.data_bits = UART_CFG_DATA_BITS_6;
		break;
	case LLSERIAL_CONNECTION_DATABITS_7:
		uart_cfg.data_bits = UART_CFG_DATA_BITS_7;
		break;
	case LLSERIAL_CONNECTION_DATABITS_8:
		uart_cfg.data_bits = UART_CFG_DATA_BITS_8;
		break;
	case LLSERIAL_CONNECTION_DATABITS_9:
		uart_cfg.data_bits = UART_CFG_DATA_BITS_9;
		break;
	default:
		LLSERIAL_THROW_NATIVE_IO_EXCEPTION(LLSERIAL_IOEXCEPTION_UNSUPPORTED_CONFIG,
		                                   LLSERIAL_CONNECTION_INVALID_DATABITS);
		break;
	}

	int err = uart_configure(uart, &uart_cfg);
	if (0 > err) {
		LLSERIAL_THROW_NATIVE_IO_EXCEPTION(LLSERIAL_IOEXCEPTION_UNSUPPORTED_CONFIG,
		                                   LLSERIAL_CONNECTION_CONFIGURATION_ERROR);
		result = LLSERIAL_RETCODE_ERROR;
	}

#ifdef CONFIG_UART_NXP_LPUART_ASYNC_API_SUPPORT
	/*
	 * On NXP LPUART, uart_configure calls LPUART_Deinit then LPUART_Init, which leaves
	 * the EDMA channel in a stale "busy" state. Stop it cleanly then restart DMA RX.
	 */
	if (serial->is_open && (LLSERIAL_RETCODE_OK == result)) {
		uart_rx_disable(uart);
		serial->is_open = true; // Restore: port remains open, only DMA was stopped
		ring_buf_reset(&serial->rx_ring_buffer.rb);
		serial->rx_buffer_id = 0;
		serial->rx.ret = LLSERIAL_RETCODE_OK;
		err = uart_rx_enable(uart, get_next_rx_buff(serial), CONFIG_MICROEJ_SERIAL_RX_BUFFER_SIZE,
		                     CONFIG_MICROEJ_SERIAL_READ_TIMEOUT);
		if ((0 > err) && (-EBUSY != err)) {
			LLSERIAL_ERROR_TRACE("[%s:%u] uart_rx_enable error %d after configure (handle %p)\n", __FUNCTION__,
			                     __LINE__, err, uart);
			result = LLSERIAL_RETCODE_ERROR;
		}
	}
#endif /* CONFIG_UART_NXP_LPUART_ASYNC_API_SUPPORT */

	return result;
}

/**
 * @brief Function to read data in serial port
 */
int32_t serial_read(serial_ctx_t *serial, uint8_t *buff, int32_t size) {
	int32_t result = LLSERIAL_RETCODE_OK;

	if (serial->rx.ret == LLSERIAL_RETCODE_OK) {
		result = ring_buf_get(&serial->rx_ring_buffer.rb, buff, size);
	} else {
		LLSERIAL_ERROR_TRACE("[%s:%u] Serial read error %d (handle %p)\n", __FUNCTION__, __LINE__, serial->rx.ret,
		                     (void *)serial->handle);
		serial->rx.ret = LLSERIAL_RETCODE_OK;
		result = uart_rx_enable((const struct device *)serial->handle, get_next_rx_buff(serial),
		                        CONFIG_MICROEJ_SERIAL_RX_BUFFER_SIZE, CONFIG_MICROEJ_SERIAL_READ_TIMEOUT);
		if (0 > result) {
			LLSERIAL_ERROR_TRACE("[%s:%u] uart_rx_enable error %d (handle %p)\n", __FUNCTION__, __LINE__, result,
			                     (const struct device *)serial->handle);
			serial->rx.ret = LLSERIAL_RETCODE_ERROR;
		}
		result = LLSERIAL_RETCODE_ERROR;
	}

	return result;
}

/**
 * @brief Function to write data in serial port
 */
int32_t serial_write(serial_ctx_t *serial, uint8_t *buff, int32_t size) {
	int32_t result = LLSERIAL_RETCODE_OK;
	const struct device *uart = (const struct device *)serial->handle;

#ifdef CONFIG_MICROEJ_SERIAL_NOCACHE_BUFFERS

	int32_t tx_len = MIN(size, (int32_t)CONFIG_MICROEJ_SERIAL_TX_BUFFER_SIZE);

	/* Copy into the per-port nocache DMA buffer before handing it to the UART driver. */
	memcpy(serial->tx_buffer, buff, tx_len);

	int err = uart_tx(uart, serial->tx_buffer, tx_len, SYS_FOREVER_US); // Trigger write operation and returns
	                                                                    // immediately.

#else

	int err = uart_tx(uart, buff, size, SYS_FOREVER_US); // Trigger write operation and returns immediately.

#endif /* CONFIG_MICROEJ_SERIAL_NOCACHE_BUFFERS */

	if (0 > err) {
		LLSERIAL_ERROR_TRACE("[%s:%u] Serial uart_tx error %d (handle %p)\n", __FUNCTION__, __LINE__, err, uart);
		result = LLSERIAL_RETCODE_ERROR;
	}

	return result;
}

/**
 * @brief Function to get available data in serial port
 */
int32_t serial_available(serial_ctx_t *serial) {
	return ring_buf_size_get(&serial->rx_ring_buffer.rb);
}
