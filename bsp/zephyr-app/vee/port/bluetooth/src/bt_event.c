/*
 * Copyright 2024-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

/**
 * @file bt_event.c
 * @brief Bluetooth event implementation.
 * @author MicroEJ Developer Team
 * @version 1.0.0
 * @date 2 September 2024
 */

#include "bt_event.h"
#include "bt_helper.h"

VEEPORT_LOGGER_DEFINE_MODULE(bt_event, BT_LOG_MODULE_LEVEL)

#ifdef __cplusplus
extern "C" {
#endif

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

/** @brief Offset of the type byte within an advertising data field. */
#define AD_FIELD_TYPE_OFFSET      (1U)

/** @brief Offset of the first data byte within an advertising data field. */
#define AD_FIELD_DATA_OFFSET      (2U)

/** @brief Scan filter state */
static uint8_t scan_filter_action = (uint8_t)SCAN_FILTER_ACTION_NONE;
static uint8_t scan_filter_type = 0U;
static uint8_t scan_filter_data[BT_GAP_ADV_MAX_ADV_DATA_LEN];
static uint8_t scan_filter_data_size = 0U;

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
	VEEPORT_LOGGER_LOG_DBG("event_info->event->event_type: %d", event_info->event->event_type);
	int32_t result = bt_event_queue_put(event_info);
	if (result != 0) {
		VEEPORT_LOGGER_LOG_WRN("Warning: event dropped due to queue being full");
		bt_event_free(event_info->event);
		return;
	}

	if (suspended_thread_id != SNI_ERROR) {
		SNI_resumeJavaThread(suspended_thread_id);
	} else {
		VEEPORT_LOGGER_LOG_WRN("Warning: event received while no suspended thread");
	}
}

int bt_event_pairing_request_put(int8_t *data) {
	return k_msgq_put(&pairing_req_queue, (void *)data, K_NO_WAIT);
}

void bt_event_pairing_complete(struct bt_conn *conn, bool bonded) {
	VEEPORT_LOGGER_LOG_DBG("Pairing complete! Bonded: %d", bonded);

	ALLOC_EVENT(LLBLUETOOTH_event_gap_pair_completed_t, 0U);
	if (NULL == out) {
		VEEPORT_LOGGER_LOG_ERR("EVENT_GAP_PAIR_COMPLETED cannot be allocated ! ble_events_heap is full");
		return;
	}

	out->event_type = EVENT_GAP_PAIR_COMPLETED;
	out->success = 1;
	out->conn_handle = conn->handle;
	send_event(&out_info);
}

void bt_event_pairing_failed(struct bt_conn *conn, enum bt_security_err reason) {
	VEEPORT_LOGGER_LOG_ERR("Pairing failed for %p, reason: %d", conn, reason);

	ALLOC_EVENT(LLBLUETOOTH_event_gap_pair_completed_t, 0U);
	if (NULL == out) {
		VEEPORT_LOGGER_LOG_ERR("EVENT_GAP_PAIR_COMPLETED cannot be allocated ! ble_events_heap is full");
		return;
	}

	out->event_type = EVENT_GAP_PAIR_COMPLETED;
	out->success = 0;
	out->conn_handle = conn->handle;
	send_event(&out_info);
}

void bt_event_pairing_cancel(struct bt_conn *conn) {
	VEEPORT_LOGGER_LOG_DBG("Pairing cancelled for %p", conn);
}

void bt_event_pairing_confirm(struct bt_conn *conn) {
	VEEPORT_LOGGER_LOG_DBG("Confirm pairing for %p", conn);
	int accept = 0;

	ALLOC_EVENT(LLBLUETOOTH_event_gap_pair_request_t, 0U);
	if (NULL == out) {
		VEEPORT_LOGGER_LOG_ERR("EVENT_GAP_PAIR_REQUEST cannot be allocated ! ble_events_heap is full");
		return;
	}

	out->event_type = EVENT_GAP_PAIR_REQUEST;
	out->conn_handle = conn->handle;
	send_event(&out_info);

	int32_t result = k_msgq_get(&pairing_req_queue, &accept, K_MSEC(REQUEST_TIMEOUT_MS));
	if (0 == result) {
		if (1 == accept) {
			VEEPORT_LOGGER_LOG_DBG("Pairing accepted");
			bt_conn_auth_pairing_confirm(conn);
		} else {
			VEEPORT_LOGGER_LOG_ERR("Pairing not allowed !");
			bt_conn_auth_cancel(conn);
		}
	} else {
		VEEPORT_LOGGER_LOG_ERR("Pairing request timeout - No answer received from the application");
		bt_conn_auth_cancel(conn);
	}
}

void bt_event_connected(struct bt_conn *conn, uint8_t err) {
	VEEPORT_LOGGER_LOG_DBG("");
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		VEEPORT_LOGGER_LOG_ERR("Failed to connect to %s %u", addr, err);

		ALLOC_EVENT(LLBLUETOOTH_event_gap_connect_failed_t, 0U);
		if (NULL == out) {
			VEEPORT_LOGGER_LOG_ERR("EVENT_GAP_CONNECT_FAILED cannot be allocated ! ble_events_heap is full");
			return;
		}

		out->event_type = EVENT_GAP_CONNECT_FAILED;
		out->addr = bt_helper_write_addr(bt_conn_get_dst(conn));

		send_event(&out_info);
		return;
	}

	// Send connected.
	{
		ALLOC_EVENT(LLBLUETOOTH_event_gap_connected_t, 0U);
		if (NULL == out) {
			VEEPORT_LOGGER_LOG_ERR("EVENT_GAP_CONNECTED cannot be allocated ! ble_events_heap is full");
			return;
		}

		out->event_type = EVENT_GAP_CONNECTED;
		out->addr = bt_helper_write_addr(bt_conn_get_dst(conn));
		out->conn_handle = conn->handle;
		send_event(&out_info);
	}

	// Send advertisement completed if the role is peripheral.
	struct bt_conn_info info = { 0 };
	int get_info_result = bt_conn_get_info(conn, &info);
	if ((BT_CONN_ROLE_PERIPHERAL == info.role) && (0 == get_info_result)) {
		ALLOC_EVENT(LLBLUETOOTH_event_gap_advertisement_completed_t, 0U);
		if (NULL == out) {
			VEEPORT_LOGGER_LOG_ERR("EVENT_GAP_ADVERTISEMENT_COMPLETED cannot be allocated");
			return;
		}

		out->event_type = EVENT_GAP_ADVERTISEMENT_COMPLETED;
		send_event(&out_info);
	} else {
		VEEPORT_LOGGER_LOG_DBG("Could not get role info or mode is not peripheral");
	}

	connection = bt_conn_ref(conn);
	last_conn_handle = conn->handle;
	current_conn_handle = conn->handle;
}

void bt_event_disconnected(struct bt_conn *conn, uint8_t reason) {
	VEEPORT_LOGGER_LOG_DBG("disconnection reason: %d", reason);
	if (conn != connection) {
		return;
	}

	ALLOC_EVENT(LLBLUETOOTH_event_gap_disconnected_t, 0U);
	if (NULL == out) {
		VEEPORT_LOGGER_LOG_ERR("EVENT_GAP_DISCONNECTED cannot be allocated ! ble_events_heap is full");
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
	VEEPORT_LOGGER_LOG_DBG("Connection interval = %d ms", interval);
	VEEPORT_LOGGER_LOG_DBG("Connection latency = %d ms", latency);
	VEEPORT_LOGGER_LOG_DBG("Connection timeout = %d ms", timeout);
}

int bt_event_read_request_put(const uint8_t *value, uint16_t len, int8_t err) {
	VEEPORT_LOGGER_LOG_DBG("len=%d, err=%d", len, err);
	read_request_resp_t data;
	memcpy(data.value, value, len);
	data.length = len;
	data.err = err;

	return k_msgq_put(&read_req_queue, (void *)&data, K_NO_WAIT);
}

ssize_t bt_event_read_request(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                              void *buf, uint16_t len, uint16_t offset) {
	VEEPORT_LOGGER_LOG_DBG("conn=%p, attr%p, len=%d, offset=%d", conn, attr, len, offset);
	read_request_resp_t data;

	ALLOC_EVENT(LLBLUETOOTH_event_gatts_read_request_t, 0U);
	if (NULL == out) {
		VEEPORT_LOGGER_LOG_ERR("EVENT_GATTS_READ_REQUEST cannot be allocated ! ble_events_heap is full");
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
			VEEPORT_LOGGER_LOG_ERR("Return data.err=%d", data.err);
			return data.err;
		} else {
			size_t actual_length = MIN(data.length, len); // Read at most requested length.
			memcpy(buf, data.value, actual_length);
			VEEPORT_LOGGER_LOG_DBG("Return actual_length=%d", actual_length);
			return actual_length;
		}
	} else {
		VEEPORT_LOGGER_LOG_ERR("Read request timeout - No answer received from the application");
		return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
	}
}

int bt_event_write_request_put(int8_t *data) {
	VEEPORT_LOGGER_LOG_DBG("data=%d", *data);
	return k_msgq_put(&write_req_queue, (void *)data, K_NO_WAIT);
}

ssize_t bt_event_write_request(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
	int8_t err = 0;
	VEEPORT_LOGGER_LOG_DBG("conn=%p, len=%d, offset=%d, flags=0x%x", conn, len, offset, flags);

	if (flags == BT_GATT_WRITE_FLAG_EXECUTE) {
		ALLOC_EVENT(LLBLUETOOTH_event_gatts_execute_write_request_t, len);
		if (NULL == out) {
			VEEPORT_LOGGER_LOG_ERR("EVENT_GATTS_EXECUTE_WRITE_REQUEST cannot be allocated ! ble_events_heap is full");
			return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
		}
		VEEPORT_LOGGER_LOG_DBG("Sending execute write request");
		out->event_type = EVENT_GATTS_EXECUTE_WRITE_REQUEST;
		out->execute = true;
		out->conn_handle = conn->handle;
		out->attr_handle = attr->handle;
		VEEPORT_LOGGER_LOG_DBG("execute=%u, conn_handle=%u, attr_handle=%u", out->execute, out->conn_handle,
		                       out->attr_handle);
		send_event(&out_info);
	} else {
		ALLOC_EVENT(LLBLUETOOTH_event_gatts_write_request_t, len);
		if (NULL == out) {
			VEEPORT_LOGGER_LOG_ERR("EVENT_GATTS_WRITE_REQUEST cannot be allocated ! ble_events_heap is full");
			return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
		}

		out->event_type = EVENT_GATTS_WRITE_REQUEST;
		out->prepare = flags & BT_GATT_WRITE_FLAG_PREPARE;
		out->conn_handle = conn->handle;
		out->attr_handle = attr->handle;
		out->offset = offset;
		out->value_size = len;
		memcpy(out + 1, buf, len);
		VEEPORT_LOGGER_LOG_DBG("prepare=%u, conn_handle=%u, attr_handle=%u, offset=%u, value_size=%u", out->prepare,
		                       out->conn_handle, out->attr_handle, out->offset, out->value_size);
		send_event(&out_info);

		if (flags & BT_GATT_WRITE_FLAG_CMD) {
			VEEPORT_LOGGER_LOG_DBG("flags & BT_GATT_WRITE_FLAG_CMD, write without response");
			return len;
		}
	}
	int32_t result = k_msgq_get(&write_req_queue, &err, K_MSEC(REQUEST_TIMEOUT_MS));
	if (0 == result) {
		if (err < 0) {
			VEEPORT_LOGGER_LOG_ERR("Cannot send message: err=%d", err);
			return err;
		} else {
			if (flags & BT_GATT_WRITE_FLAG_PREPARE) {
				VEEPORT_LOGGER_LOG_DBG("flags & BT_GATT_WRITE_FLAG_PREPARE");
				len = 0;
			}
		}
	} else {
		VEEPORT_LOGGER_LOG_ERR("Write request timeout - No answer received from the application");
		return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
	}
	return len;
}

void bt_event_indication_cb(struct bt_conn *conn, struct bt_gatt_indicate_params *params, uint8_t err) {
	VEEPORT_LOGGER_LOG_DBG("Indication %s", err != 0U ? "fail" : "success");

	ALLOC_EVENT(LLBLUETOOTH_event_gatts_notification_sent_t, 0U);
	if (NULL == out) {
		VEEPORT_LOGGER_LOG_ERR("EVENT_GATTS_NOTIFICATION_SENT cannot be allocated ! ble_events_heap is full");
		return;
	}

	out->event_type = EVENT_GATTS_NOTIFICATION_SENT;
	out->success = (0 == err) ? 1 : 0;
	out->conn_handle = conn->handle;
	out->attr_handle = params->attr->handle;
	send_event(&out_info);
}

void bt_event_indication_destroy(struct bt_gatt_indicate_params *params) {
	VEEPORT_LOGGER_LOG_DBG("Indication complete");
}

void bt_event_notification_complete(struct bt_conn *conn, void *attr_handle_ptr) {
	VEEPORT_LOGGER_LOG_DBG("");
	uint16_t *attr_handle = (uint16_t *)attr_handle_ptr;

	ALLOC_EVENT(LLBLUETOOTH_event_gatts_notification_sent_t, 0U);
	if (NULL == out) {
		VEEPORT_LOGGER_LOG_ERR("EVENT_GATTS_NOTIFICATION_SENT cannot be allocated ! ble_events_heap is full");
		return;
	}

	out->event_type = EVENT_GATTS_NOTIFICATION_SENT;
	out->success = 1;
	out->conn_handle = conn->handle;
	out->attr_handle = *attr_handle;
	send_event(&out_info);
}

void bt_event_scan_completed(void) {
	ALLOC_EVENT(LLBLUETOOTH_event_gap_scan_completed_t, 0U);
	if (NULL == out) {
		VEEPORT_LOGGER_LOG_ERR("EVENT_GAP_SCAN_COMPLETED cannot be allocated ! ble_events_heap is full");
		return;
	}
	out->event_type = EVENT_GAP_SCAN_COMPLETED;
	send_event(&out_info);
}

void bt_event_set_scan_filter(uint8_t action, uint8_t type, const uint8_t *data, uint8_t data_size) {
	scan_filter_action = action;
	scan_filter_type = type;
	scan_filter_data_size = (data_size < BT_GAP_ADV_MAX_ADV_DATA_LEN) ? data_size : BT_GAP_ADV_MAX_ADV_DATA_LEN;
	if (data_size > 0U) {
		memcpy(scan_filter_data, data, scan_filter_data_size);
	}
}

static bool scan_filter_matches(const struct net_buf_simple *ad) {
	bool matches = false;

	if ((uint8_t)SCAN_FILTER_ACTION_NONE == scan_filter_action) {
		matches = true;
	} else {
		uint16_t i = 0U;
		while ((i < ad->len) && !matches) {
			uint8_t length = ad->data[i];
			/* stop on the zero padding following the last field, or on a truncated field */
			if ((0U == length) || ((i + length) >= ad->len)) {
				break;
			}
			uint8_t ad_type = ad->data[i + AD_FIELD_TYPE_OFFSET];
			uint8_t field_len = (uint8_t)(length - 1U);
			const uint8_t *field_data = &ad->data[i + AD_FIELD_DATA_OFFSET];
			if (ad_type == scan_filter_type) {
				switch (scan_filter_action) {
				case (uint8_t)SCAN_FILTER_ACTION_FIELD_EXISTS:
					matches = true;
					break;
				case (uint8_t)SCAN_FILTER_ACTION_FIELD_EQUALS:
					matches = (field_len == scan_filter_data_size) &&
					          (0 == memcmp(field_data, scan_filter_data, scan_filter_data_size));
					break;
				case (uint8_t)SCAN_FILTER_ACTION_FIELD_STARTS_WITH:
					matches = (field_len >= scan_filter_data_size) &&
					          (0 == memcmp(field_data, scan_filter_data, scan_filter_data_size));
					break;
				default:
					/* unknown action: no match */
					break;
				}
			}
			i = (uint16_t)(i + length + 1U);
		}
	}

	return matches;
}

void bt_event_scan_device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                                struct net_buf_simple *ad) {
	VEEPORT_LOGGER_LOG_DBG("*addr=%p, rssi=%d, type=%d", addr, rssi, type);
	if (scan_filter_matches(ad)) {
		ALLOC_EVENT(LLBLUETOOTH_event_gap_scan_result_t, ad->len);
		if (NULL == out) {
			VEEPORT_LOGGER_LOG_ERR("EVENT_GAP_SCAN_RESULT cannot be allocated ! ble_events_heap is full");
		} else {
			out->event_type = EVENT_GAP_SCAN_RESULT;
			out->addr = bt_helper_write_addr(addr);
			out->rssi = rssi;
			out->adv_type = bt_helper_map_adv_type(type);
			out->adv_data_size = ad->len;
			memcpy(out + 1, ad->data, ad->len);
			send_event(&out_info);
		}
	}
}

void bt_event_search_result(struct bt_conn *conn, uint16_t service_handle,
                            const LLBLUETOOTH_uuid_t *service_uuid,
                            uint8_t num_attributes,
                            const LLBLUETOOTH_gattc_attribute_t *attrs) {
	ALLOC_EVENT(LLBLUETOOTH_event_gattc_discovery_result_t,
	            num_attributes * sizeof(LLBLUETOOTH_gattc_attribute_t));
	if (NULL == out) {
		VEEPORT_LOGGER_LOG_ERR("EVENT_GATTC_DISCOVERY_RESULT cannot be allocated ! ble_events_heap is full");
		return;
	}

	out->event_type = EVENT_GATTC_DISCOVERY_RESULT;
	out->num_attributes = num_attributes;
	out->conn_handle = conn->handle;
	out->service_handle = service_handle;
	out->service_uuid = *service_uuid;
	memcpy(out + 1, attrs, num_attributes * sizeof(LLBLUETOOTH_gattc_attribute_t));
	send_event(&out_info);
}

void bt_event_search_complete(struct bt_conn *conn) {
	ALLOC_EVENT(LLBLUETOOTH_event_gattc_discovery_completed_t, 0U);
	if (NULL == out) {
		VEEPORT_LOGGER_LOG_ERR("EVENT_GATTC_DISCOVERY_COMPLETED cannot be allocated ! ble_events_heap is full");
		return;
	}

	out->event_type = EVENT_GATTC_DISCOVERY_COMPLETED;
	out->conn_handle = conn->handle;
	send_event(&out_info);
}

uint8_t bt_event_read_complete(struct bt_conn *conn, uint8_t att_err,
                               struct bt_gatt_read_params *params, const void *data, uint16_t read_len) {
	/* Accumulate each chunk and let Zephyr drive the Read Long procedure (Read Blob
	 * requests) until it signals completion with a NULL / zero-length chunk. */
	if ((att_err == 0U) && (data != NULL) && (read_len > 0U)) {
		(void)bt_helper_read_accumulate(params, (const uint8_t *)data, read_len);
		return BT_GATT_ITER_CONTINUE;
	}

	/* Completion (or error): deliver the fully reassembled value in one event. */
	uint16_t value_size = 0U;
	const uint8_t *value = bt_helper_read_data(params, &value_size);

	ALLOC_EVENT(LLBLUETOOTH_event_gattc_read_completed_t, value_size);
	if (NULL == out) {
		VEEPORT_LOGGER_LOG_ERR("EVENT_GATTC_READ_COMPLETED cannot be allocated ! ble_events_heap is full");
		bt_helper_release_read_params(params);
		return BT_GATT_ITER_STOP;
	}
	out->event_type = EVENT_GATTC_READ_COMPLETED;
	out->status = att_err;
	out->conn_handle = conn->handle;
	out->attr_handle = params->single.handle;
	out->value_size = value_size;
	(void)memcpy(out + 1, value, value_size);
	bt_helper_release_read_params(params);
	send_event(&out_info);

	return BT_GATT_ITER_STOP;
}

void bt_event_notification_received(struct bt_conn *conn, uint16_t attr_handle,
                                    const void *data, uint16_t length) {
	VEEPORT_LOGGER_LOG_DBG("conn=%p, attr_handle=%d, length=%d", conn, attr_handle, length);
	ALLOC_EVENT(LLBLUETOOTH_event_gattc_notification_received_t, length);
	if (NULL == out) {
		VEEPORT_LOGGER_LOG_ERR("EVENT_GATTC_NOTIFICATION_RECEIVED cannot be allocated ! ble_events_heap is full");
		return;
	}
	out->event_type = EVENT_GATTC_NOTIFICATION_RECEIVED;
	out->_padding = 0U;
	out->conn_handle = conn->handle;
	out->attr_handle = attr_handle;
	out->value_size = length;
	(void)memcpy(out + 1, data, length);
	send_event(&out_info);
}

void bt_event_write_complete(struct bt_conn *conn, uint8_t status, uint16_t attr_handle) {
	ALLOC_EVENT(LLBLUETOOTH_event_gattc_write_completed_t, 0U);
	if (NULL == out) {
		VEEPORT_LOGGER_LOG_ERR("EVENT_GATTC_WRITE_COMPLETED cannot be allocated ! ble_events_heap is full");
		return;
	}
	out->event_type = EVENT_GATTC_WRITE_COMPLETED;
	out->status = status;
	out->conn_handle = conn->handle;
	out->attr_handle = attr_handle;
	VEEPORT_LOGGER_LOG_DBG("status=%u, conn_handle=%u, attr_handle=%u", out->status, out->conn_handle,
	                       out->attr_handle);
	send_event(&out_info);
}

#ifdef __cplusplus
}
#endif
