/*
 * Copyright 2024-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

/**
 * @file bt_event.c
 * @brief Bluetooth event implementation.
 * @author MicroEJ Developer Team
 * @version 1.0.0
 * @date 2 September 2024
 */

#include "bt_event.h"
#include <zephyr/logging/log.h>

#ifdef __cplusplus
extern "C" {
#endif

LOG_MODULE_REGISTER(bt_event);

/** @brief Request queue size (in number of elements). */
#define REQUEST_QUEUE_SIZE        (10U)

/** @brief Request timeout is ms */
#define REQUEST_TIMEOUT_MS        (100U)

/** @brief Bluetooth event queue size (in number of elements). */
#define BT_EVENT_QUEUE_SIZE           (25U)
#define BT_ATT_MTU (CONFIG_BT_L2CAP_TX_MTU)

/* Event queue initialization */
K_MSGQ_DEFINE(bt_event_queue, sizeof(struct bt_event_info_t), BT_EVENT_QUEUE_SIZE, 1);

/** @brief Events heap */
K_HEAP_DEFINE(ble_events_heap,
              BT_EVENT_QUEUE_SIZE * (sizeof(LLBLUETOOTH_event_gattc_discovery_result_t) + BT_ATT_MTU));

/** @brief Allocate event using dynamic memory. */
// cppcheck-suppress misra-c2012-20.7 // type macro parameter is a data type, therefore it cannot be enclosed in
// parentheses.
#define ALLOC_EVENT(type, extra)                                                 \
		uint32_t out_size = sizeof(type) + (extra);                              \
		type *out = (type *)k_heap_alloc(&ble_events_heap, out_size, K_NO_WAIT); \
		bt_event_info_t out_info = { (LLBLUETOOTH_event_t *)out, out_size };

/** @brief Connection handle reference */
static struct bt_conn *connection = NULL;
/** @brief Handle of the last connection */
static uint16_t last_conn_handle;
/** @brief Handle of the current connection */
static uint16_t current_conn_handle;
/** @brief Java thread id when suspended. */
static int32_t suspended_thread_id = SNI_ERROR;

typedef struct read_request_resp_t {
	int8_t err;
	uint8_t value[BT_ATT_MTU];
	uint16_t length;
} read_request_resp_t;

K_MSGQ_DEFINE(read_req_queue, sizeof(read_request_resp_t), REQUEST_QUEUE_SIZE, 1);
K_MSGQ_DEFINE(write_req_queue, sizeof(int8_t), REQUEST_QUEUE_SIZE, 1);
K_MSGQ_DEFINE(pairing_req_queue, sizeof(int8_t), REQUEST_QUEUE_SIZE, 1);

void bt_event_free(LLBLUETOOTH_event_t *event) {
	k_heap_free(&ble_events_heap, event);
}

ssize_t bt_event_queue_get(bt_event_info_t *event_info) {
	return k_msgq_get(&bt_event_queue, event_info, K_NO_WAIT);
}

ssize_t bt_event_queue_put(bt_event_info_t *event_info) {
	return k_msgq_put(&bt_event_queue, event_info, K_NO_WAIT);
}

struct bt_conn * bt_event_get_connection() {
	return connection;
}

uint16_t bt_event_get_current_connection_handle() {
	return current_conn_handle;
}

int32_t bt_event_get_suspended_thread_id() {
	return suspended_thread_id;
}

void bt_event_set_suspended_thread_id(uint16_t thread_id) {
	suspended_thread_id = thread_id;
}

/**
 * @brief Sends a generic event into the queue. If the Java thread is suspended, it will be resumed.
 *
 * @param[in] event_info the event info.
 */
static void send_event(bt_event_info_t *event_info) {
	int32_t result = bt_event_queue_put(event_info);
	if (result != 0) {
		LOG_WRN("Warning: event dropped due to queue being full");
		bt_event_free(event_info->event);
		return;
	}

	if (suspended_thread_id != SNI_ERROR) {
		SNI_resumeJavaThread(suspended_thread_id);
	} else {
		LOG_WRN("Warning: event received while no suspended thread");
	}
}

int bt_event_pairing_request_put(int8_t *data) {
	return k_msgq_put(&pairing_req_queue, (void *)data, K_NO_WAIT);
}

enum bt_security_err bt_event_pairing_accept(struct bt_conn *conn, const struct bt_conn_pairing_feat *const feat) {
	int accept = 0;

	ALLOC_EVENT(LLBLUETOOTH_event_gap_pair_request_t, 0U);
	if (NULL == out) {
		LOG_ERR("EVENT_GAP_PAIR_REQUEST cannot be allocated ! ble_events_heap is full");
		return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
		;
		;
	}

	out->event_type = EVENT_GAP_PAIR_REQUEST;
	out->conn_handle = conn->handle;
	send_event(&out_info);

	int32_t result = k_msgq_get(&pairing_req_queue, &accept, K_MSEC(REQUEST_TIMEOUT_MS));
	if (0 == result) {
		if (1 == accept) {
			LOG_DBG("Pairing accepted");
			return BT_SECURITY_ERR_SUCCESS;
		} else {
			LOG_ERR("Pairing not allowed !");
			return BT_SECURITY_ERR_PAIR_NOT_ALLOWED;
		}
	} else {
		LOG_ERR("Pairing request timeout - No answer received from the application");
		return BT_SECURITY_ERR_UNSPECIFIED;
	}
}

void bt_event_pairing_complete(struct bt_conn *conn, bool bonded) {
	ARG_UNUSED(bonded);
	LOG_DBG("Pairing complete !");

	ALLOC_EVENT(LLBLUETOOTH_event_gap_pair_completed_t, 0U);
	if (NULL == out) {
		LOG_ERR("EVENT_GAP_PAIR_COMPLETED cannot be allocated ! ble_events_heap is full");
		return;
	}

	out->event_type = EVENT_GAP_PAIR_COMPLETED;
	out->success = 1;
	out->conn_handle = conn->handle;
	send_event(&out_info);
}

void bt_event_connected(struct bt_conn *conn, uint8_t err) {
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		LOG_ERR("Failed to connect to %s %u\n\n", addr, err);

		ALLOC_EVENT(LLBLUETOOTH_event_gap_connect_failed_t, 0U);
		if (NULL == out) {
			LOG_ERR("EVENT_GAP_CONNECT_FAILED cannot be allocated ! ble_events_heap is full");
			return;
		}

		out->event_type = EVENT_GAP_CONNECT_FAILED;
		memcpy(out->addr.addr, bt_conn_get_dst(conn)->a.val, sizeof(bt_conn_get_dst(conn)->a.val));
		out->addr.type = bt_conn_get_dst(conn)->type;

		bt_conn_unref(connection);
		connection = NULL;

		send_event(&out_info);
		return;
	}

	ALLOC_EVENT(LLBLUETOOTH_event_gap_connected_t, 0U);
	if (NULL == out) {
		LOG_ERR("EVENT_GAP_CONNECTED cannot be allocated ! ble_events_heap is full");
		return;
	}

	out->event_type = EVENT_GAP_CONNECTED;
	memcpy(out->addr.addr, bt_conn_get_dst(conn)->a.val, sizeof(bt_conn_get_dst(conn)->a.val));
	out->addr.type = bt_conn_get_dst(conn)->type;
	out->conn_handle = conn->handle;
	send_event(&out_info);

	connection = bt_conn_ref(conn);
	last_conn_handle = conn->handle;
	current_conn_handle = conn->handle;
}

void bt_event_disconnected(struct bt_conn *conn, uint8_t reason) {
	if (conn != connection) {
		return;
	}

	ALLOC_EVENT(LLBLUETOOTH_event_gap_disconnected_t, 0U);
	if (NULL == out) {
		LOG_ERR("EVENT_GAP_DISCONNECTED cannot be allocated ! ble_events_heap is full");
		return;
	}

	out->event_type = EVENT_GAP_DISCONNECTED;
	out->conn_handle = last_conn_handle;
	send_event(&out_info);

	bt_conn_unref(connection);
	current_conn_handle = 0;
	connection = NULL;
}

void bt_event_le_param_updated(struct bt_conn *conn, uint16_t interval,
                               uint16_t latency, uint16_t timeout) {
	LOG_DBG("Connection interval = %d ms", interval);
	LOG_DBG("Connection latency = %d ms", latency);
	LOG_DBG("Connection timeout = %d ms", timeout);
}

int bt_event_read_request_put(const uint8_t *value, uint16_t len, int8_t err) {
	read_request_resp_t data;
	memcpy(data.value, value, len);
	data.length = len;
	data.err = err;

	return k_msgq_put(&read_req_queue, (void *)&data, K_NO_WAIT);
}

ssize_t bt_event_read_request(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                              void *buf, uint16_t len, uint16_t offset) {
	read_request_resp_t data;

	ALLOC_EVENT(LLBLUETOOTH_event_gatts_read_request_t, 0U);
	if (NULL == out) {
		LOG_ERR("EVENT_GATTS_READ_REQUEST cannot be allocated ! ble_events_heap is full");
		return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
	}

	out->event_type = EVENT_GATTS_READ_REQUEST;
	out->conn_handle = conn->handle;
	out->attr_handle = attr->handle;
	out->offset = offset;
	send_event(&out_info);

	int32_t result = k_msgq_get(&read_req_queue, &data, K_MSEC(REQUEST_TIMEOUT_MS));
	if (0 == result) {
		if (data.err < 0) {
			return data.err;
		} else {
			memcpy(buf, data.value, data.length);
			return data.length;
		}
	} else {
		LOG_ERR("Read request timeout - No answer received from the application");
		return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
	}
}

int bt_event_write_request_put(int8_t *data) {
	return k_msgq_put(&write_req_queue, (void *)data, K_NO_WAIT);
}

ssize_t bt_event_write_request(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
	int8_t err = 0;

	ALLOC_EVENT(LLBLUETOOTH_event_gatts_write_request_t, len);
	if (NULL == out) {
		LOG_ERR("EVENT_GATTS_WRITE_REQUEST cannot be allocated ! ble_events_heap is full");
		return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
		;
	}

	out->event_type = EVENT_GATTS_WRITE_REQUEST;
	out->prepare = flags & 0x1;
	out->conn_handle = conn->handle;
	out->attr_handle = attr->handle;
	out->offset = offset;
	out->value_size = len;
	memcpy(out + 1, buf, len);
	send_event(&out_info);

	if (flags == BT_GATT_WRITE_FLAG_CMD) {
		return len;
	}

	int32_t result = k_msgq_get(&write_req_queue, &err, K_MSEC(REQUEST_TIMEOUT_MS));
	if (0 == result) {
		if (err < 0) {
			return err;
		} else {
			return len;
		}
	} else {
		LOG_ERR("Write request timeout - No answer received from the application");
		return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
	}
}

void bt_event_indication_cb(struct bt_conn *conn, struct bt_gatt_indicate_params *params, uint8_t err) {
	LOG_DBG("Indication %s\n", err != 0U ? "fail" : "success");

	ALLOC_EVENT(LLBLUETOOTH_event_gatts_notification_sent_t, 0U);
	if (NULL == out) {
		LOG_ERR("EVENT_GATTS_NOTIFICATION_SENT cannot be allocated ! ble_events_heap is full");
		return;
	}

	out->event_type = EVENT_GATTS_NOTIFICATION_SENT;
	out->success = (0 == err) ? 1 : 0;
	out->conn_handle = conn->handle;
	out->attr_handle = params->attr->handle;
	send_event(&out_info);
}

void bt_event_indication_destroy(struct bt_gatt_indicate_params *params) {
	LOG_DBG("Indication complete");
}

#ifdef __cplusplus
}
#endif
