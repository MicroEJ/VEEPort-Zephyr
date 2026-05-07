/*
 * Copyright 2024-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
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
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(LLBLUETOOTH_impl);

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

static struct bt_conn_cb conn_callbacks = {
	.connected = bt_event_connected,
	.disconnected = bt_event_disconnected,
	.le_param_updated = bt_event_le_param_updated
};

static struct bt_conn_auth_cb le_auth_cb = {
	.pairing_accept = bt_event_pairing_accept
};

static struct bt_conn_auth_info_cb bt_conn_auth_info = {
	.pairing_complete = bt_event_pairing_complete
};

uint8_t LLBLUETOOTH_IMPL_enable(void) {
	int err = bt_enable(NULL);
	if (0 != err) {
		LOG_ERR("Bluetooth failed to be enabled (err %d)", err);
		return 0;
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	/* Register authentication callbacks */
	err = bt_conn_auth_cb_register(&le_auth_cb);
	if (0 != err) {
		LOG_ERR("Failed to register authentication callbacks (err %d)", err);
		return 0;
	}

	/* Register authentication info callbacks */
	err = bt_conn_auth_info_cb_register(&bt_conn_auth_info);
	if (0 != err) {
		LOG_ERR("Failed to register authentication info callbacks (err %d)", err);
		return 0;
	}

	/* Register connection callbacks */
	bt_conn_cb_register(&conn_callbacks);

	return 1;
}

void LLBLUETOOTH_IMPL_disable(void) {
	return;
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
			LOG_WRN("Warning: buffer it not big enough to hold the event");
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
	return 0;
}

uint8_t LLBLUETOOTH_IMPL_stopScanning(void) {
	return 0;
}

uint8_t LLBLUETOOTH_IMPL_startAdvertising(const uint8_t *adv_data, uint32_t adv_data_size) {
	uint8_t ret = 1;
	uint8_t nb_elements = 0;
	struct bt_data ad_data[MAX_BT_DATA_PER_ADV_PACKET];

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
			LOG_ERR("Bad advertising packet format !");
			return 0;
		}
	}

	int err = bt_le_adv_start(CFG_ADV_PARAMETERS, ad_data, nb_elements, NULL, 0);
	if (0 != err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		ret = 0;
	}

	return ret;
}

uint8_t LLBLUETOOTH_IMPL_stopAdvertising(void) {
	return 0;
}

uint8_t LLBLUETOOTH_IMPL_connect(const LLBLUETOOTH_address_t *addr) {
	return 0;
}

uint8_t LLBLUETOOTH_IMPL_disconnect(uint16_t conn_handle) {
	return 0;
}

uint8_t LLBLUETOOTH_IMPL_sendPairRequest(uint16_t conn_handle) {
	return 0;
}

uint8_t LLBLUETOOTH_IMPL_sendPairResponse(uint16_t conn_handle, uint8_t accept) {
	uint8_t ret = 1;

	if (NULL != bt_event_get_connection()) {
		if (conn_handle != bt_event_get_current_connection_handle()) {
			LOG_ERR("Send pair response failed : Connection handle invalid");
			return 0;
		}
	} else {
		LOG_ERR("Send pair response failed : No connection ongoing");
		return 0;
	}

	int32_t result = bt_event_pairing_request_put(&accept);
	if (0 != result) {
		LOG_ERR("Send pair response failed : k_msgq_put error %d", result);
		ret = 0;
	}

	return ret;
}

uint8_t LLBLUETOOTH_IMPL_sendPasskeyResponse(uint16_t conn_handle, uint8_t accept, uint32_t passkey) {
	return 0;
}

uint8_t LLBLUETOOTH_IMPL_discoverServices(uint16_t conn_handle, const LLBLUETOOTH_uuid_t *uuid) {
	return 0;
}

uint8_t LLBLUETOOTH_IMPL_sendReadRequest(uint16_t conn_handle, uint16_t attr_handle) {
	return 0;
}

uint8_t LLBLUETOOTH_IMPL_sendWriteRequest(uint16_t conn_handle, uint16_t attr_handle, const uint8_t *value,
                                          uint32_t value_size, uint8_t no_response) {
	return 0;
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
			LOG_ERR("Cannot add service : Maximum number of services allowed in the GATT Database reached !");
			return 0;
		}

		if (attributes_count + total_attributes > CFG_BLE_NUM_GATT_ATTRIBUTES) {
			LOG_ERR(
				"Cannot add service : Impossible to add %d attributes, only %d allowed to complete the GATT Database",
				total_attributes, CFG_BLE_NUM_GATT_ATTRIBUTES - attributes_count);
			return 0;
		}

		bool handles_to_save[total_attributes];
		current_service->attrs = k_heap_alloc(&services_heap, total_attributes * sizeof(struct bt_gatt_attr),
		                                      K_NO_WAIT);
		if (NULL == current_service->attrs) {
			LOG_ERR("The attributes can not be allocated");
			break;
		}
		/* Initialize the attrs memory with zeros */
		memset(current_service->attrs, 0, total_attributes * sizeof(struct bt_gatt_attr));

		/* Allocate the service attributes */
		ret = bt_helper_add_service(&current_service->attrs[current_service->attr_count], &llservice->uuid);
		if (0 == ret) {
			LOG_ERR("The service can not be allocated");
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
					LOG_ERR("The characteristic can not be allocated");
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
					LOG_ERR("The descriptor can not be allocated");
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
			LOG_ERR("Add services failed (err %d)", err);
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
	uint8_t ret = 1;

	if (NULL != bt_event_get_connection()) {
		if (conn_handle != bt_event_get_current_connection_handle()) {
			LOG_ERR("Send read response failed : Connection handle invalid");
			return 0;
		}
	} else {
		LOG_ERR("Send read response failed : No connection ongoing");
		return 0;
	}

	const struct bt_gatt_attr *attr = bt_helper_find_attr(attr_handle);
	if (NULL == attr) {
		LOG_ERR("Send read response failed : Attribute handle not valid !");
		return 0;
	}

	int32_t result = bt_event_read_request_put(value, value_size, -status);
	if (0 != result) {
		LOG_ERR("Send read response failed : k_msgq_put error %d", result);
		ret = 0;
	}

	return ret;
}

uint8_t LLBLUETOOTH_IMPL_sendWriteResponse(uint16_t conn_handle, uint16_t attr_handle, uint8_t status) {
	uint8_t ret = 1;
	int8_t err = -status;

	if (NULL != bt_event_get_connection()) {
		if (conn_handle != bt_event_get_current_connection_handle()) {
			LOG_ERR("Send write response failed : Connection handle invalid");
			return 0;
		}
	} else {
		LOG_ERR("Send write response failed : No connection ongoing");
		return 0;
	}

	const struct bt_gatt_attr *attr = bt_helper_find_attr(attr_handle);
	if (NULL == attr) {
		LOG_ERR("Send write response failed : Attribute handle not valid !");
		return 0;
	}

	int32_t result = bt_event_write_request_put(&err);
	if (0 != result) {
		LOG_ERR("Send write response failed : k_msgq_put error %d", result);
		ret = 0;
	}

	return ret;
}

uint8_t LLBLUETOOTH_IMPL_sendPrepareWriteResponse(uint16_t conn_handle, uint16_t attr_handle, uint8_t status,
                                                  const uint8_t *value, uint32_t value_size, uint32_t offset) {
	return 0;
}

uint8_t LLBLUETOOTH_IMPL_sendExecuteWriteResponse(uint16_t conn_handle, uint16_t attr_handle, uint8_t status) {
	return 0;
}

uint8_t LLBLUETOOTH_IMPL_sendNotification(uint16_t conn_handle, uint16_t attr_handle, const uint8_t *value,
                                          uint32_t value_size, uint8_t confirm) {
	uint8_t ret = 1;
	int err;
	static struct bt_gatt_indicate_params params;
	memset(&params, 0, sizeof(params));
	struct bt_conn *connection = bt_event_get_connection();

	if (NULL != connection) {
		if (conn_handle != bt_event_get_current_connection_handle()) {
			LOG_ERR("Send notification : Connection handle invalid");
			return 0;
		}
	} else {
		LOG_ERR("Send notification : No connection ongoing");
		return 0;
	}

	const struct bt_gatt_attr *attr = bt_helper_find_attr(attr_handle);
	if (NULL == attr) {
		LOG_ERR("Send notification : Attribute handle not valid !");
		return 0;
	}

	if (1 == confirm) {
		params.attr = attr;
		params.func = bt_event_indication_cb;
		params.destroy = bt_event_indication_destroy;
		params.len = value_size;
		params.data = value;

		err = bt_gatt_indicate(connection, &params);
		if (0 != err) {
			ret = 0;
			LOG_ERR("Send Indication failed (err %d)", err);
		}
	} else {
		err = bt_gatt_notify(connection, attr, value, value_size);
		if (0 != err) {
			ret = 0;
			LOG_ERR("Send Notification failed (err %d)", err);
		}
	}

	return ret;
}

#ifdef __cplusplus
}
#endif
