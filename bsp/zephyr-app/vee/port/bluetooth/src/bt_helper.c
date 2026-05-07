/*
 * Copyright 2024-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

/**
 * @file bt_helper.c
 * @brief Bluetooth Helper
 * @author MicroEJ Developer Team
 * @date 4 September 2024
 */

#include "bt_helper.h"
#include <zephyr/logging/log.h>
#include "cmsis_gcc.h"

LOG_MODULE_REGISTER(bt_helper);

extern struct k_heap services_heap;

/**
 *  @brief Helper to convert permissions from the Application to the Zephyr stack
 * 		   The values refer to the BluetoothPermissions.java file.
 * 		   The values are SDK-dependent.
 *
 *  @param perm Attribute permissions to convert
 *
 *  @return Attribute permissions converted
 */
static uint8_t bt_helper_convert_perm(uint8_t perm) {
	switch (perm) {
	case ATTRIBUTE_PERMISSION_NONE:
		return BT_GATT_PERM_NONE;

	case ATTRIBUTE_PERMISSION_READ:
		return BT_GATT_PERM_READ;

	case ATTRIBUTE_PERMISSION_READ_ENCRYPT:
		return BT_GATT_PERM_READ_ENCRYPT;

	case ATTRIBUTE_PERMISSION_READ_AUTH:
		return BT_GATT_PERM_READ_AUTHEN;

	case ATTRIBUTE_PERMISSION_WRITE:
		return BT_GATT_PERM_WRITE;

	case ATTRIBUTE_PERMISSION_WRITE_ENCRYPT:
		return BT_GATT_PERM_WRITE_ENCRYPT;

	case ATTRIBUTE_PERMISSION_WRITE_AUTH:
		return BT_GATT_PERM_WRITE_AUTHEN;

	case ATTRIBUTE_PERMISSION_READ | ATTRIBUTE_PERMISSION_WRITE:
		return BT_GATT_PERM_READ | BT_GATT_PERM_WRITE;

	case ATTRIBUTE_PERMISSION_READ_ENCRYPT | ATTRIBUTE_PERMISSION_WRITE_ENCRYPT:
		return BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT;

	case ATTRIBUTE_PERMISSION_READ_AUTH | ATTRIBUTE_PERMISSION_WRITE_AUTH:
		return BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_ENCRYPT;

	default:
		return BT_GATT_PERM_NONE;
	}
}

/**
 *  @brief Helper to reverse the bytes of the UUID (from little-endian to big-endian or the opposite)
 *
 *  @param uuid_reverse     uuid_reverse is the pointer where the reversed uuid will be stored
 *  @param uuid             uuid is the pointer of the uuid to reverse
 *  @param len              size of bytes of the uuid
 */
static void bt_helper_reverse_uuid(uint8_t *uuid_reverse, const uint8_t *uuid, uint8_t len) {
	for (uint8_t i = 0; i < len; i++) {
		uuid_reverse[i] = uuid[len - 1 - i];
	}
}

void bt_helper_free_service(struct bt_gatt_service *service) {
	if (NULL != service) {
		for (uint8_t i = 0; i < service->attr_count; i++) {
			k_heap_free(&services_heap, (void *)service->attrs->uuid);
			k_heap_free(&services_heap, (void *)service->attrs->user_data);
		}
		k_heap_free(&services_heap, (void *)service->attrs);
	}
}

uint8_t bt_helper_add_service(struct bt_gatt_attr *attr_service, const LLBLUETOOTH_uuid_t *uuid) {
	uint8_t uuid_le[16];
	bool valid_uuid;

	/* Allocate the service attributes */
	attr_service->user_data = k_heap_alloc(&services_heap, sizeof(struct bt_uuid_128), K_NO_WAIT);
	if (NULL == attr_service->user_data) {
		return 0;
	}
	bt_helper_reverse_uuid(uuid_le, uuid->uuid, sizeof(uuid->uuid));
	valid_uuid = bt_uuid_create((struct bt_uuid *)attr_service->user_data, uuid_le, sizeof(uuid_le));
	if (false == valid_uuid) {
		LOG_ERR("The attr_service->user_data UUID was not successfully created");
		return 0;
	}

	attr_service->uuid = k_heap_alloc(&services_heap, sizeof(struct bt_uuid_16), K_NO_WAIT);
	if (NULL == attr_service->uuid) {
		return 0;
	}
	uint16_t bt_uuid_gatt_primary_value = BT_UUID_GATT_PRIMARY_VAL;
	valid_uuid = bt_uuid_create((struct bt_uuid *)attr_service->uuid, (uint8_t *)&bt_uuid_gatt_primary_value,
	                            sizeof(bt_uuid_gatt_primary_value));
	if (false == valid_uuid) {
		LOG_ERR("The attr_serv->uuid UUID was not successfully created");
		return 0;
	}

	attr_service->handle = 0;
	attr_service->perm = BT_GATT_PERM_READ;
	attr_service->read = bt_gatt_attr_read_service;
	attr_service->write = NULL;

	return 1;
}

uint8_t bt_helper_add_characteristic(struct bt_gatt_attr *attr_char_decl, struct bt_gatt_attr *attr_char_val,
                                     const LLBLUETOOTH_uuid_t *uuid, uint8_t perms, uint8_t props) {
	void *read;
	void *write;
	uint8_t uuid_le[16];
	bool valid_uuid;

	attr_char_val->uuid = k_heap_alloc(&services_heap, sizeof(struct bt_uuid_128), K_NO_WAIT);
	if (NULL == attr_char_val->uuid) {
		return 0;
	}

	bt_helper_reverse_uuid(uuid_le, uuid->uuid, sizeof(uuid->uuid));
	valid_uuid = bt_uuid_create((struct bt_uuid *)attr_char_val->uuid, uuid_le, sizeof(uuid_le));
	if (false == valid_uuid) {
		LOG_ERR("The attr_char_val->uuid UUID was not successfully created");
		return 0;
	}

	/* CHARACTERISTIC DECLARATION */
	attr_char_decl->handle = 0;
	attr_char_decl->perm = BT_GATT_PERM_READ;
	attr_char_decl->read = bt_gatt_attr_read_chrc;
	attr_char_decl->write = NULL;

	attr_char_decl->user_data = k_heap_alloc(&services_heap, sizeof(struct bt_gatt_chrc), K_NO_WAIT);
	if (NULL == attr_char_decl->user_data) {
		return 0;
	}

	struct bt_gatt_chrc *attr_char_decl_val = attr_char_decl->user_data;
	attr_char_decl_val->uuid = attr_char_val->uuid;
	attr_char_decl_val->value_handle = 0;
	attr_char_decl_val->properties = props;

	attr_char_decl->uuid = k_heap_alloc(&services_heap, sizeof(struct bt_uuid_16), K_NO_WAIT);
	if (NULL == attr_char_decl->uuid) {
		return 0;
	}
	uint16_t bt_uuid_gatt_chrc_val = BT_UUID_GATT_CHRC_VAL;
	valid_uuid = bt_uuid_create((struct bt_uuid *)attr_char_decl->uuid, (uint8_t *)&bt_uuid_gatt_chrc_val,
	                            sizeof(bt_uuid_gatt_chrc_val));
	if (false == valid_uuid) {
		LOG_ERR("The attr_char_decl->uuid UUID was not successfully created");
		return 0;
	}

	/* CHARACTERISTIC VALUE */
	if (0 != (props & BT_GATT_CHRC_READ)) {
		read = bt_event_read_request;
	} else {
		read = NULL;
	}
	if ((0 != (props & BT_GATT_CHRC_WRITE)) || (0 != (props & BT_GATT_CHRC_WRITE_WITHOUT_RESP))) {
		write = bt_event_write_request;
	} else {
		write = NULL;
	}
	uint8_t perms_ = bt_helper_convert_perm(perms);

	attr_char_val->handle = 0;
	attr_char_val->perm = perms_;
	attr_char_val->read = read;
	attr_char_val->write = write;
	attr_char_val->user_data = NULL;

	return 1;
};

uint8_t bt_helper_add_descriptor(struct bt_gatt_attr *attr_desc, const LLBLUETOOTH_uuid_t *uuid, uint8_t perms) {
	void *read;
	void *write;
	uint8_t uuid_le[4];

	attr_desc->uuid = k_heap_alloc(&services_heap, sizeof(struct bt_uuid_16), K_NO_WAIT);
	if (NULL == attr_desc->uuid) {
		return 0;
	}
	// Extract the descriptor
	bt_helper_reverse_uuid(uuid_le, uuid->uuid, 4);
	uint16_t uuid_desc = *(uint16_t *)uuid_le;
	bool valid_uuid = bt_uuid_create((struct bt_uuid *)attr_desc->uuid, (uint8_t *)&uuid_desc, sizeof(uuid_desc));
	if (false == valid_uuid) {
		LOG_ERR("The attr_desc->uuid UUID was not successfully created");
		return 0;
	}

	switch (uuid_desc) {
	case BT_UUID_GATT_CEP_VAL:
	case BT_UUID_GATT_CPF_VAL:
	case BT_UUID_GATT_CUD_VAL:
		read = bt_event_read_request;
		write = NULL;
		break;

	case BT_UUID_GATT_CCC_VAL:
		read = bt_event_read_request;
		write = bt_event_write_request;
		break;

	default:
		read = NULL;
		write = NULL;
		break;
	}
	uint8_t perms_ = bt_helper_convert_perm(perms);

	attr_desc->handle = 0;
	attr_desc->perm = perms_;
	attr_desc->read = read;
	attr_desc->write = write;
	attr_desc->user_data = NULL;

	return 1;
}

/** @typedef bt_gatt_attr_func_t
 *  @brief Attribute iterator callback.
 *
 *  @param attr Attribute found.
 *  @param handle Attribute handle found.
 *  @param user_data Data given.
 *
 *  @return BT_GATT_ITER_CONTINUE if should continue to the next attribute.
 *  @return BT_GATT_ITER_STOP to stop.
 */
static uint8_t found_attr(const struct bt_gatt_attr *attr, uint16_t handle, void *user_data) {
	const struct bt_gatt_attr **found = user_data;

	*found = attr;

	return BT_GATT_ITER_STOP;
}

const struct bt_gatt_attr * bt_helper_find_attr(uint16_t attr_handle) {
	const struct bt_gatt_attr *attr = NULL;

	bt_gatt_foreach_attr_type(attr_handle, attr_handle, NULL, NULL, 0, found_attr, &attr);

	return attr;
}
