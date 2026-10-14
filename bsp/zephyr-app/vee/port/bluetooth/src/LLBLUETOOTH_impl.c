/*
 * Copyright 2024-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

/**
 * @file LLBLUETOOTH_impl.c
 * @brief LLBLUETOOTH implementation over Zephyr stack.
 * @author MicroEJ Developer Team
 * @version 1.0.0
 * @date 14 August 2024
 */

#include <stdio.h>
#include <string.h>
#include "LLBLUETOOTH_impl.h"
#include "bt_event.h"
#include "bt_helper.h"
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/settings/settings.h>

VEEPORT_LOGGER_DEFINE_MODULE(LLBLUETOOTH_impl, BT_LOG_MODULE_LEVEL)

/** @brief Maximum number of "datas" in an advertising packets */
#define MAX_BT_DATA_PER_ADV_PACKET          5

/** @brief Maximum number of GATT services allowed in the heap */
#define CFG_BLE_NUM_GATT_SERVICES           8
#define CFG_BLE_NUM_GATT_ATTRIBUTES         68

/** @brief Advertising parameters */
#define CFG_ADV_PARAMETERS      BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN, \
												BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL)

K_HEAP_DEFINE(services_heap, 4 * 1024);   /* the heap size corresponding at the worst case with a margin
                                           * 8*sizeof(struct bt_gatt_service) + 68*(sizeof(struct bt_gatt_attr) +
                                           * 2*sizeof(struct bt_uuid_128)) = 3768
                                           * This case will never appear as 1 characteristic takes 2 attributes and only
                                           * one UUID128 */

/** @brief Services stack */
static struct bt_gatt_service services[CFG_BLE_NUM_GATT_SERVICES];
/** @brief Services counter, shouldn't be greater than CFG_BLE_NUM_GATT_SERVICES */
static uint8_t services_count = 0;
/** @brief Attributes counter, shouldn't be greater than CFG_BLE_NUM_GATT_ATTRIBUTES */
static uint8_t attributes_count = 0;
/** @brief Whether LE scanning is currently active (drives the scan-completed event). */
static bool scanning = false;
/** @brief Whether the connection/authentication callbacks have been registered.
 *  bt_disable() does not clear registered callbacks, so they must be registered exactly once
 *  and not re-registered on a subsequent enable(). */
static bool callbacks_registered = false;

static struct bt_conn_cb conn_callbacks = {
	.connected = bt_event_connected,
	.disconnected = bt_event_disconnected,
	.le_param_updated = bt_event_le_param_updated
};

static struct bt_conn_auth_cb le_auth_cb = {
	.pairing_confirm = bt_event_pairing_confirm,
	.cancel = bt_event_pairing_cancel
};

static struct bt_conn_auth_info_cb bt_conn_auth_info = {
	.pairing_complete = bt_event_pairing_complete,
	.pairing_failed = bt_event_pairing_failed
};

/**
 * @brief Retrieves the ongoing connection, checking that it matches the given connection handle.
 *
 * This port tracks a single connection at a time, so every LLBLUETOOTH function taking a connection
 * handle must reject a handle that does not designate the ongoing connection. The error is logged
 * here, so callers only have to test the returned pointer.
 *
 * @param[in] conn_handle the connection handle to match.
 * @param[in] operation the operation name, used to prefix the error logs.
 *
 * @return the ongoing connection, or NULL if no connection is ongoing or if the handle does not match.
 */
static struct bt_conn * get_matching_connection(uint16_t conn_handle, const char *operation) {
	struct bt_conn *conn = bt_event_get_connection();

	if (NULL == conn) {
		VEEPORT_LOGGER_LOG_ERR("%s failed: no connection ongoing", operation);
	} else if (conn_handle != bt_event_get_current_connection_handle()) {
		VEEPORT_LOGGER_LOG_ERR("%s failed: connection handle invalid", operation);
		conn = NULL;
	} else {
		/* the ongoing connection matches the given handle */
	}

	return conn;
}

uint8_t LLBLUETOOTH_IMPL_enable(void) {
	VEEPORT_LOGGER_LOG_DBG("");
	int err = bt_enable(NULL);
	if (0 != err) {
		VEEPORT_LOGGER_LOG_ERR("Bluetooth failed to be enabled (err %d)", err);
		return 0;
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	if (IS_ENABLED(CONFIG_MICROEJ_BLUETOOTH_UNPAIR_ON_ENABLE)) {
		bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
	}

	/* Callbacks survive bt_disable()/bt_enable() and must be registered exactly once — Zephyr's
	 * bt_disable() documentation explicitly forbids re-registering them on re-enable. */
	if (!callbacks_registered) {
		/* Register authentication callbacks */
		err = bt_conn_auth_cb_register(&le_auth_cb);
		if (0 != err) {
			VEEPORT_LOGGER_LOG_ERR("Failed to register authentication callbacks (err %d)", err);
			return 0;
		}

		/* Register authentication info callbacks */
		err = bt_conn_auth_info_cb_register(&bt_conn_auth_info);
		if (0 != err) {
			VEEPORT_LOGGER_LOG_ERR("Failed to register authentication info callbacks (err %d)", err);
			return 0;
		}

		/* Register connection callbacks */
		bt_conn_cb_register(&conn_callbacks);

		callbacks_registered = true;
	}

	return 1;
}

void LLBLUETOOTH_IMPL_disable(void) {
	VEEPORT_LOGGER_LOG_DBG("");
	int err = 0;

	/* Stop any active scanning/advertising before tearing down the controller. */
	if (scanning) {
		err = bt_le_scan_stop();
		if (0 != err) {
			VEEPORT_LOGGER_LOG_ERR("Error during scan stop (err %d)", err);
		}
		scanning = false;
	}
	err = bt_le_adv_stop();
	if (0 > err) {
		VEEPORT_LOGGER_LOG_ERR("Error during advertising stop (err %d)", err);
	}

	/* Unregister and free the GATT services registered via addService(). Without this, each
	 * enable()/addService() cycle stacks a new copy of the service in the GATT database, so the
	 * peer discovers a stale instance and attribute handles resolve to outdated objects. */
	for (uint8_t i = 0U; i < services_count; i++) {
		int svc_err = bt_gatt_service_unregister(&services[i]);
		if (0 != svc_err) {
			VEEPORT_LOGGER_LOG_ERR("Failed to unregister GATT service %u (err %d)", i, svc_err);
		}
		bt_helper_free_service(&services[i]);
	}
	services_count = 0U;
	attributes_count = 0U;

	err = bt_disable();
	if (0 != err) {
		VEEPORT_LOGGER_LOG_ERR("Bluetooth failed to be disabled (err %d)", err);
	}

	/* Release the per-session GATT client state once the controller is down: bt_disable()
	 * disconnects every link, which completes the in-flight ATT requests with -ECONNRESET and lets
	 * their callbacks release their own slot. Clearing the pools before that hands out slots whose
	 * parameters the host still references. */
	bt_helper_reset();
}

/**
 * @brief Gets the next Bluetooth event from the event queue in a non-blocking way.
 *
 * @param[in] llevent        the buffer where to store the LLBLUETOOTH event.
 * @param[in] max_event_size the maximum event size that can be stored in the event buffer.
 *
 * @return event size in bytes, 0 if no event can be retrieved.
 */
static int32_t poll_event(LLBLUETOOTH_event_t *llevent, uint32_t max_event_size) {
	while (1) {
		// get event
		bt_event_info_t event_info;
		int32_t result = bt_event_queue_get(&event_info);
		if (result != 0) {
			return 0;
		}

		if (event_info.size > max_event_size) {
			// discard this event and get an other one
			VEEPORT_LOGGER_LOG_WRN("Warning: buffer it not big enough to hold the event");
			bt_event_free(event_info.event);
		} else if (event_info.size == 0U) {
			// discard this event and get an other one
			bt_event_free(event_info.event);
		} else {
			// return this event
			memcpy(llevent, event_info.event, event_info.size);
			bt_event_free(event_info.event);
			return event_info.size;
		}
	}
}

uint32_t LLBLUETOOTH_IMPL_waitEvent(LLBLUETOOTH_event_t *event, uint32_t max_event_size) {
	VEEPORT_LOGGER_LOG_DBG("event=%p, max_event_size=%d", event, max_event_size);
	if (SNI_ERROR == bt_event_get_suspended_thread_id()) {
		bt_event_set_suspended_thread_id(SNI_getCurrentJavaThreadID());
	}

	int32_t event_size = poll_event(event, max_event_size);
	if (event_size > 0) {
		return event_size;
	}

	SNI_suspendCurrentJavaThread(0);
	return 0;
}

uint8_t LLBLUETOOTH_IMPL_startScanning(uint8_t filter_action, uint8_t filter_type, const uint8_t *filter_data,
                                       uint32_t filter_data_size) {
	uint8_t result = 0;
	VEEPORT_LOGGER_LOG_DBG("filter_action=%d, filter_type=%d, filter_data=%p, filter_data_size=%d", filter_action,
	                       filter_type, (void *)filter_data, filter_data_size);
	bt_event_set_scan_filter(filter_action, filter_type, filter_data, (uint8_t)filter_data_size);

	/* Use active scanning and disable duplicate filtering to handle any
	 * devices that might update their advertising data at runtime. */
	struct bt_le_scan_param scan_param = {
		.type = BT_LE_SCAN_TYPE_ACTIVE,
		.interval = BT_GAP_SCAN_SLOW_INTERVAL_1,
		.window = BT_GAP_SCAN_SLOW_WINDOW_1,
	};

	int err = bt_le_scan_start(&scan_param, bt_event_scan_device_found);
	if (err) {
		VEEPORT_LOGGER_LOG_ERR("Scanning failed to start (err %d)", err);
	} else {
		scanning = true;
		VEEPORT_LOGGER_LOG_INF("Scanning successfully started");
		result = 1;
	}

	return result;
}

uint8_t LLBLUETOOTH_IMPL_stopScanning(void) {
	uint8_t result = 0;

	VEEPORT_LOGGER_LOG_DBG("");

	int err = bt_le_scan_stop();
	if (err) {
		VEEPORT_LOGGER_LOG_ERR("Scanning failed to stop (err %d)", err);
	} else if (scanning) {
		scanning = false;
		bt_event_scan_completed();
		result = 1;
	}

	return result;
}

uint8_t LLBLUETOOTH_IMPL_startAdvertising(const uint8_t *adv_data, uint32_t adv_data_size) {
	uint8_t ret = 1;
	uint8_t nb_elements = 0;
	struct bt_data ad_data[MAX_BT_DATA_PER_ADV_PACKET];
	VEEPORT_LOGGER_LOG_DBG("adv_data=%p, adv_data_size=%d", (void *)adv_data, adv_data_size);

	for (uint8_t i = 0; i < adv_data_size;) {
		ad_data[nb_elements] = (struct bt_data) {
			.data_len = adv_data[i] - 1,
			.type = adv_data[i + 1],
			.data = &adv_data[i + 2]
		};
		if (BT_DATA_NAME_COMPLETE == ad_data[nb_elements].type) {
			char name[ad_data[nb_elements].data_len + 1];
			memcpy(name, ad_data[nb_elements].data, ad_data[nb_elements].data_len);
			name[ad_data[nb_elements].data_len] = '\0';
			bt_set_name(name);
		}
		i += ad_data[nb_elements].data_len + 2;
		nb_elements += 1;

		if (nb_elements > MAX_BT_DATA_PER_ADV_PACKET) {
			VEEPORT_LOGGER_LOG_ERR("Bad advertising packet format !");
			return 0;
		}
	}

	int err = bt_le_adv_start(CFG_ADV_PARAMETERS, ad_data, nb_elements, NULL, 0);
	if (0 != err) {
		VEEPORT_LOGGER_LOG_ERR("Advertising failed to start (err %d)", err);
		ret = 0;
	}

	return ret;
}

uint8_t LLBLUETOOTH_IMPL_stopAdvertising(void) {
	uint8_t result = 0;
	VEEPORT_LOGGER_LOG_DBG("");
	int err = bt_le_adv_stop();
	if (err != 0) {
		VEEPORT_LOGGER_LOG_ERR("Stop LE advertising failed (err %d)", err);
	} else {
		result = 1;
	}
	return result;
}

uint8_t LLBLUETOOTH_IMPL_connect(const LLBLUETOOTH_address_t *addr) {
	uint8_t result = 0;

	BT_HELPER_LOG_ADDR(addr->addr);

	/* This port tracks a single connection at a time: connecting to a second peer would overwrite
	 * the ongoing one, leaving its handle unusable and its reference leaked. */
	if (NULL != bt_event_get_connection()) {
		VEEPORT_LOGGER_LOG_ERR("Connect failed: a connection is already ongoing");
	} else {
		bool scan_stopped_ok = true;

		/* Stop scan before initiating connection. Scanning ending here is reported to the
		 * application as a scan-completed event, mirroring an explicit stopScanning(). */
		if (scanning) {
			int stop_err = bt_le_scan_stop();
			if (stop_err != 0) {
				VEEPORT_LOGGER_LOG_ERR("Stop LE scan failed (err %d)", stop_err);
				scan_stopped_ok = false;
			} else {
				scanning = false;
				bt_event_scan_completed();
			}
		}

		if (scan_stopped_ok) {
			bt_addr_le_t zephyr_addr = bt_helper_read_addr(addr);
			struct bt_conn *conn = NULL;
			int err = bt_conn_le_create(&zephyr_addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &conn);
			if (err != 0) {
				VEEPORT_LOGGER_LOG_ERR("Failed to connect (err %d)", err);
			} else {
				/* Release our reference — bt_event_connected will store its own via bt_conn_ref */
				bt_conn_unref(conn);
				result = 1;
			}
		}
	}

	return result;
}

uint8_t LLBLUETOOTH_IMPL_disconnect(uint16_t conn_handle) {
	uint8_t result = 0;

	VEEPORT_LOGGER_LOG_INF("LLBLUETOOTH_IMPL_disconnect");

	struct bt_conn *conn = get_matching_connection(conn_handle, "Disconnect");

	if (NULL != conn) {
		int err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		if (0 != err) {
			VEEPORT_LOGGER_LOG_ERR("Failed to disconnect (err %d)", err);
		} else {
			result = 1;
		}
	}

	return result;
}

uint8_t LLBLUETOOTH_IMPL_sendPairRequest(uint16_t conn_handle) {
	uint8_t result = 0;

	VEEPORT_LOGGER_LOG_DBG("conn_handle=%d", conn_handle);

	struct bt_conn *conn = get_matching_connection(conn_handle, "Send pair request");

	if (NULL != conn) {
		/* Initiate pairing as central. Completion is delivered asynchronously through the
		 * pairing_complete / pairing_failed callbacks. MITM is added automatically when the
		 * local IO capabilities allow it (CONFIG_BT_SMP_ENFORCE_MITM). */
		int err = bt_conn_set_security(conn, BT_SECURITY_L2);
		if (0 != err) {
			VEEPORT_LOGGER_LOG_ERR("Failed to initiate pairing (err %d)", err);
		} else {
			result = 1;
		}
	}

	return result;
}

uint8_t LLBLUETOOTH_IMPL_sendPairResponse(uint16_t conn_handle, uint8_t accept) {
	uint8_t ret = 1;
	VEEPORT_LOGGER_LOG_DBG("conn_handle=%d, accept=%d", conn_handle, accept);

	if (NULL == get_matching_connection(conn_handle, "Send pair response")) {
		return 0;
	}

	int32_t result = bt_event_pairing_request_put(&accept);
	if (0 != result) {
		VEEPORT_LOGGER_LOG_ERR("Send pair response failed : k_msgq_put error %d", result);
		ret = 0;
	}

	return ret;
}

uint8_t LLBLUETOOTH_IMPL_sendPasskeyResponse(uint16_t conn_handle, uint8_t accept, uint32_t passkey) {
	VEEPORT_LOGGER_LOG_DBG("conn_handle=%d, accept=%d, passkey=%d", conn_handle, accept, passkey);
	return 0;
}

uint8_t LLBLUETOOTH_IMPL_discoverServices(uint16_t conn_handle, const LLBLUETOOTH_uuid_t *uuid) {
	uint8_t result = 0;

	VEEPORT_LOGGER_LOG_INF("LLBLUETOOTH_IMPL_discoverServices uuid=%s", (uuid == NULL) ? "NULL" : "set");

	struct bt_conn *conn = get_matching_connection(conn_handle, "Discover services");

	if (NULL != conn) {
		int err = bt_helper_start_discovery(conn, uuid);
		if (err != 0) {
			VEEPORT_LOGGER_LOG_ERR("Discover failed (err %d)", err);
		} else {
			result = 1;
		}
	}

	return result;
}

uint8_t LLBLUETOOTH_IMPL_sendReadRequest(uint16_t conn_handle, uint16_t attr_handle) {
	uint8_t result = 0;

	VEEPORT_LOGGER_LOG_DBG("attr_handle=%u", attr_handle);

	struct bt_conn *conn = get_matching_connection(conn_handle, "Read request");

	if (NULL != conn) {
		int err = bt_helper_start_read(conn, attr_handle);
		if (err != 0) {
			VEEPORT_LOGGER_LOG_ERR("Read request failed (err %d)", err);
		} else {
			result = 1;
		}
	}

	return result;
}

uint8_t LLBLUETOOTH_IMPL_sendWriteRequest(uint16_t conn_handle, uint16_t attr_handle, const uint8_t *value,
                                          uint32_t value_size, uint8_t no_response) {
	uint8_t result = 0U;

	VEEPORT_LOGGER_LOG_DBG("attr_handle=%u, value_size=%u, no_response=%u", attr_handle, value_size, no_response);

	struct bt_conn *conn = get_matching_connection(conn_handle, "Write request");

	/* A write to any attribute — including a CCCD (0x2902) — is a raw GATT write in the
	 * MicroEJ model: the exact bytes are sent to the peer unchanged. CCCD writes must NOT be
	 * intercepted and turned into a Zephyr subscribe here (that corrupts the payload and cannot
	 * carry a long value). Notification reception is a separate concern (see bt_helper subscribe). */
	if (NULL == conn) {
		/* the error has already been logged by get_matching_connection */
	} else if (value_size > (uint32_t)UINT16_MAX) {
		VEEPORT_LOGGER_LOG_ERR("Write request failed: value size %u exceeds the maximum ATT value length", value_size);
	} else if (0U != no_response) {
		/* Write Without Response cannot use the Long Write procedure, so the
		 * payload must fit within a single ATT PDU (ATT_MTU - 3). */
		uint16_t max_size = (uint16_t)(bt_gatt_get_mtu(conn) - BT_HELPER_ATT_WRITE_HEADER_SIZE);
		if (value_size > max_size) {
			VEEPORT_LOGGER_LOG_ERR("Write without response size must be maximum %u bytes due to the MTU", max_size);
		} else {
			int err = bt_gatt_write_without_response(conn, attr_handle, value, (uint16_t)value_size, false);
			if (err != 0) {
				VEEPORT_LOGGER_LOG_ERR("Write without response failed (err %d)", err);
			} else {
				result = 1U;
			}
		}
	} else {
		/* Write With Response. bt_gatt_write() switches to the GATT Long Write (Prepare/Execute)
		 * procedure when the value exceeds the negotiated ATT_MTU - 3, but bt_helper_start_write
		 * copies the payload into a fixed slot of CONFIG_BT_L2CAP_TX_MTU - 3 bytes and rejects
		 * anything longer, so that is the effective cap — not the 512-byte GATT maximum. */
		int err = bt_helper_start_write(conn, attr_handle, value, (uint16_t)value_size);
		if (err != 0) {
			VEEPORT_LOGGER_LOG_ERR("Write request failed (err %d)", err);
		} else {
			result = 1U;
		}
	}

	return result;
}

uint8_t LLBLUETOOTH_IMPL_addService(const LLBLUETOOTH_gatts_service_t *llservice, uint16_t *handles) {
	bool do_break = false;
	uint8_t ret;
	struct bt_gatt_service *current_service = &services[services_count];

	do{
		// total attributes = 1 service declaration + num_characteristics * (characteristics declaration + value) +
		// num_descriptors
		int total_attributes = 1 + llservice->num_characteristics * 2 + llservice->num_descriptors;

		if (services_count >= CFG_BLE_NUM_GATT_SERVICES) {
			VEEPORT_LOGGER_LOG_ERR(
				"Cannot add service : Maximum number of services allowed in the GATT Database reached !");
			return 0;
		}

		if (attributes_count + total_attributes > CFG_BLE_NUM_GATT_ATTRIBUTES) {
			VEEPORT_LOGGER_LOG_ERR(
				"Cannot add service : Impossible to add %d attributes, only %d allowed to complete the GATT Database",
				total_attributes, CFG_BLE_NUM_GATT_ATTRIBUTES - attributes_count);
			return 0;
		}

		bool handles_to_save[total_attributes];
		current_service->attrs = k_heap_alloc(&services_heap, total_attributes * sizeof(struct bt_gatt_attr),
		                                      K_NO_WAIT);
		if (NULL == current_service->attrs) {
			VEEPORT_LOGGER_LOG_ERR("The attributes can not be allocated");
			break;
		}
		/* Initialize the attrs memory with zeros */
		memset(current_service->attrs, 0, total_attributes * sizeof(struct bt_gatt_attr));

		/* Allocate the service attributes */
		ret = bt_helper_add_service(&current_service->attrs[current_service->attr_count], &llservice->uuid);
		if (0 == ret) {
			VEEPORT_LOGGER_LOG_ERR("The service can not be allocated");
			break;
		}
		handles_to_save[current_service->attr_count] = true;
		current_service->attr_count += 1;

		const LLBLUETOOTH_gatts_attribute_t *llattributes = (LLBLUETOOTH_gatts_attribute_t *)(llservice + 1);
		size_t num_attributes = llservice->num_characteristics + llservice->num_descriptors;

		for (size_t i = 0; i < num_attributes; i++) {
			const LLBLUETOOTH_gatts_attribute_t *llattribute = &llattributes[i];

			if (ATTRIBUTE_TYPE_CHARACTERISTIC == llattribute->type) {
				ret = bt_helper_add_characteristic(&current_service->attrs[current_service->attr_count],
				                                   &current_service->attrs[current_service->attr_count + 1],
				                                   &llattribute->characteristic.uuid,
				                                   llattribute->characteristic.permissions,
				                                   llattribute->characteristic.properties);
				// we don't save the characteristic declaration
				handles_to_save[current_service->attr_count] = false;
				current_service->attr_count += 1;
				handles_to_save[current_service->attr_count] = true;
				current_service->attr_count += 1;
				if (0 == ret) {
					VEEPORT_LOGGER_LOG_ERR("The characteristic can not be allocated");
					do_break = true;
					break;
				}
			} else if (ATTRIBUTE_TYPE_DESCRIPTOR == llattribute->type) {
				ret = bt_helper_add_descriptor(&current_service->attrs[current_service->attr_count],
				                               &llattribute->characteristic.uuid,
				                               llattribute->characteristic.permissions);
				handles_to_save[current_service->attr_count] = true;
				current_service->attr_count += 1;
				if (0 == ret) {
					VEEPORT_LOGGER_LOG_ERR("The descriptor can not be allocated");
					do_break = true;
					break;
				}
			}
		}
		/* break the do-while loop if the for (int i = 0; i < num_attributes; i++) has been broken */
		if (true == do_break) {
			break;
		}

		/* Register GATT service */
		int err = bt_gatt_service_register(current_service);
		if (0 != err) {
			VEEPORT_LOGGER_LOG_ERR("Add services failed (err %d)", err);
			break;
		}

		/* Save handles */
		for (uint8_t h = 0, h_index = 0; h < current_service->attr_count; h++) {
			if (true == handles_to_save[h]) {
				handles[h_index++] = bt_gatt_attr_get_handle(&current_service->attrs[h]);
			}
		}

		attributes_count += current_service->attr_count;
		services_count += 1;
		return 1;
	} while (1);

	bt_helper_free_service(current_service);
	return 0;
}

uint8_t LLBLUETOOTH_IMPL_sendReadResponse(uint16_t conn_handle, uint16_t attr_handle, uint8_t status,
                                          const uint8_t *value, uint32_t value_size) {
	VEEPORT_LOGGER_LOG_DBG("conn_handle=%d, attr_handle=%d, status=%d, value_size=%d", conn_handle, attr_handle, status,
	                       value_size);
	uint8_t ret = 1;

	if (NULL == get_matching_connection(conn_handle, "Send read response")) {
		return 0;
	}

	const struct bt_gatt_attr *attr = bt_helper_find_attr(attr_handle);
	if (NULL == attr) {
		VEEPORT_LOGGER_LOG_ERR("Send read response failed : Attribute handle not valid !");
		return 0;
	}

	int32_t result = bt_event_read_request_put(value, value_size, -status);
	if (0 != result) {
		VEEPORT_LOGGER_LOG_ERR("Send read response failed : k_msgq_put error %d", result);
		ret = 0;
	}

	return ret;
}

uint8_t LLBLUETOOTH_IMPL_sendWriteResponse(uint16_t conn_handle, uint16_t attr_handle, uint8_t status) {
	VEEPORT_LOGGER_LOG_DBG("conn_handle=%d, attr_handle=%d, status=%d", conn_handle, attr_handle, status);
	uint8_t ret = 1;
	int8_t err = -status;

	if (NULL == get_matching_connection(conn_handle, "Send write response")) {
		return 0;
	}

	const struct bt_gatt_attr *attr = bt_helper_find_attr(attr_handle);
	if (NULL == attr) {
		VEEPORT_LOGGER_LOG_ERR("Send write response failed : Attribute handle not valid !");
		return 0;
	}

	int32_t result = bt_event_write_request_put(&err);
	if (0 != result) {
		VEEPORT_LOGGER_LOG_ERR("Send write response failed : k_msgq_put error %d", result);
		ret = 0;
	}

	return ret;
}

uint8_t LLBLUETOOTH_IMPL_sendPrepareWriteResponse(uint16_t conn_handle, uint16_t attr_handle, uint8_t status,
                                                  const uint8_t *value, uint32_t value_size, uint32_t offset) {
	/* Zephyr's GATT server owns the prepare/execute write queue and builds the Prepare Write
	 * Response itself, so this port has no value to echo back — only the status is forwarded. */
	ARG_UNUSED(value);

	VEEPORT_LOGGER_LOG_DBG("conn_handle=%d, attr_handle=%d, status=%d, value_size=%d, offset=%d", conn_handle,
	                       attr_handle, status, value_size, offset);
	return LLBLUETOOTH_IMPL_sendWriteResponse(conn_handle, attr_handle, status);
}

uint8_t LLBLUETOOTH_IMPL_sendExecuteWriteResponse(uint16_t conn_handle, uint16_t attr_handle, uint8_t status) {
	VEEPORT_LOGGER_LOG_DBG("conn_handle=%d, attr_handle=%d, status=%d", conn_handle, attr_handle, status);
	return LLBLUETOOTH_IMPL_sendWriteResponse(conn_handle, attr_handle, status);
}

uint8_t LLBLUETOOTH_IMPL_sendNotification(uint16_t conn_handle, uint16_t attr_handle, const uint8_t *value,
                                          uint32_t value_size, uint8_t confirm) {
	VEEPORT_LOGGER_LOG_DBG("conn_handle=%d, attr_handle=%d, value_size=%d, confirm=%d", conn_handle, attr_handle,
	                       value_size, confirm);
	uint8_t ret = 1;
	int err;
	struct bt_conn *connection = get_matching_connection(conn_handle, "Send notification");

	if (NULL == connection) {
		return 0;
	}

	const struct bt_gatt_attr *attr = bt_helper_find_attr(attr_handle);
	if (NULL == attr) {
		VEEPORT_LOGGER_LOG_ERR("Send notification : Attribute handle not valid !");
		return 0;
	}

	if (1 == confirm) {
		VEEPORT_LOGGER_LOG_DBG("GATT indicate");
		err = bt_helper_start_indicate(connection, attr, value, (uint16_t)value_size);
		if (0 != err) {
			ret = 0;
			VEEPORT_LOGGER_LOG_ERR("Send Indication failed (err %d)", err);
		}
	} else {
		struct bt_gatt_notify_params notify_params = { 0 };
		notify_params.attr = attr;
		notify_params.data = value;
		notify_params.len = value_size;
		notify_params.func = bt_event_notification_complete;
		notify_params.user_data = &attr_handle;
		VEEPORT_LOGGER_LOG_DBG("GATT notify");
		err = bt_gatt_notify_cb(connection, &notify_params);
		if (0 != err) {
			ret = 0;
			VEEPORT_LOGGER_LOG_ERR("Send Notification failed (err %d)", err);
		}
	}

	return ret;
}

#ifdef __cplusplus
}
#endif
