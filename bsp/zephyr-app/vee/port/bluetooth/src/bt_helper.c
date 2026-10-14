/*
 * Copyright 2024-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

/**
 * @file bt_helper.c
 * @brief Bluetooth Helper
 * @author MicroEJ Developer Team
 * @date 4 September 2024
 */

#include "bt_helper.h"
#include "cmsis_gcc.h"

#include <zephyr/sys/byteorder.h>

VEEPORT_LOGGER_DEFINE_MODULE(bt_helper, BT_LOG_MODULE_LEVEL)

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
	uint8_t result = BT_GATT_PERM_NONE;
	if ((perm & ATTRIBUTE_PERMISSION_NONE) != 0) {
		result |= BT_GATT_PERM_NONE;
	}
	if ((perm & ATTRIBUTE_PERMISSION_READ) != 0) {
		result |= BT_GATT_PERM_READ;
	}
	if ((perm & ATTRIBUTE_PERMISSION_READ_ENCRYPT) != 0) {
		result |= BT_GATT_PERM_READ_ENCRYPT;
	}
	if ((perm & ATTRIBUTE_PERMISSION_READ_AUTH) != 0) {
		result |= BT_GATT_PERM_READ_AUTHEN;
	}
	if ((perm & ATTRIBUTE_PERMISSION_WRITE) != 0) {
		result |= (BT_GATT_PERM_WRITE | BT_GATT_PERM_PREPARE_WRITE);
	}
	if ((perm & ATTRIBUTE_PERMISSION_WRITE_ENCRYPT) != 0) {
		result |= (BT_GATT_PERM_WRITE_ENCRYPT | BT_GATT_PERM_PREPARE_WRITE);
	}
	if ((perm & ATTRIBUTE_PERMISSION_WRITE_AUTH) != 0) {
		result |= (BT_GATT_PERM_WRITE_AUTHEN | BT_GATT_PERM_PREPARE_WRITE);
	}
	return result;
}

void bt_helper_free_service(struct bt_gatt_service *service) {
	if (NULL != service) {
		for (uint8_t i = 0; i < service->attr_count; i++) {
			/* Free each attribute's own uuid/user_data (indexed by i). Freeing service->attrs->*
			 * freed attrs[0] repeatedly, double-freeing it and leaking the rest. k_heap_free is a
			 * no-op on NULL, so attributes without a user_data (e.g. characteristic value) are fine. */
			k_heap_free(&services_heap, (void *)service->attrs[i].uuid);
			k_heap_free(&services_heap, (void *)service->attrs[i].user_data);
		}
		k_heap_free(&services_heap, (void *)service->attrs);
		service->attr_count = 0;
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
	/* LLBLUETOOTH UUID is big-endian, Zephyr expects little-endian */
	sys_memcpy_swap(uuid_le, uuid->uuid, sizeof(uuid->uuid));
	valid_uuid = bt_uuid_create((struct bt_uuid *)attr_service->user_data, uuid_le, sizeof(uuid_le));
	if (false == valid_uuid) {
		VEEPORT_LOGGER_LOG_ERR("The attr_service->user_data UUID was not successfully created");
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
		VEEPORT_LOGGER_LOG_ERR("The attr_serv->uuid UUID was not successfully created");
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

	/* LLBLUETOOTH UUID is big-endian, Zephyr expects little-endian */
	sys_memcpy_swap(uuid_le, uuid->uuid, sizeof(uuid->uuid));
	valid_uuid = bt_uuid_create((struct bt_uuid *)attr_char_val->uuid, uuid_le, sizeof(uuid_le));
	if (false == valid_uuid) {
		VEEPORT_LOGGER_LOG_ERR("The attr_char_val->uuid UUID was not successfully created");
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
		VEEPORT_LOGGER_LOG_ERR("The attr_char_decl->uuid UUID was not successfully created");
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

	VEEPORT_LOGGER_LOG_DBG("Characteristic permissions: in = 0x%x, out = 0x%x", perms, perms_);

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
	// Extract the descriptor: LLBLUETOOTH UUID is big-endian, Zephyr expects little-endian
	sys_memcpy_swap(uuid_le, uuid->uuid, sizeof(uuid_le));
	uint16_t uuid_desc = *(uint16_t *)uuid_le;
	bool valid_uuid = bt_uuid_create((struct bt_uuid *)attr_desc->uuid, (uint8_t *)&uuid_desc, sizeof(uuid_desc));
	if (false == valid_uuid) {
		VEEPORT_LOGGER_LOG_ERR("The attr_desc->uuid UUID was not successfully created");
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

struct bt_uuid_128 bt_helper_read_uuid(const LLBLUETOOTH_uuid_t *uuid) {
	struct bt_uuid_128 uuid128;
	uuid128.uuid.type = BT_UUID_TYPE_128;
	/* LLBLUETOOTH UUID is big-endian, Zephyr expects little-endian */
	sys_memcpy_swap(uuid128.val, uuid->uuid, sizeof(uuid->uuid));
	return uuid128;
}

bt_addr_le_t bt_helper_read_addr(const LLBLUETOOTH_address_t *lladdr) {
	VEEPORT_LOGGER_LOG_DBG("mej native address:");
	BT_HELPER_LOG_ADDR(lladdr->addr);
	bt_addr_le_t addr;
	addr.type = (lladdr->type == ADDRESS_TYPE_PUBLIC) ? BT_ADDR_LE_PUBLIC : BT_ADDR_LE_RANDOM;
	/* LLBLUETOOTH address is big-endian, Zephyr expects little-endian */
	sys_memcpy_swap(addr.a.val, lladdr->addr, sizeof(addr.a.val));
	return addr;
}

/** @brief Maximum number of GATT attributes accumulated during discovery */
#define MAX_DISCOVERY_ATTRIBUTES 20U

/** @brief Discovery state machine phases */
typedef enum {
	DISCOVERY_STATE_PRIMARY,
	DISCOVERY_STATE_CHARACTERISTIC,
	DISCOVERY_STATE_DESCRIPTOR,
} discovery_state_t;

static struct bt_gatt_discover_params discover_params; /* PRIMARY discovery only — .uuid holds the service filter */
static struct bt_gatt_discover_params inner_params;    /* CHARACTERISTIC and DESCRIPTOR phases */
static struct bt_uuid_128 discover_uuid;

static bool discovery_in_use; /* a discovery owns discover_params until its completion event is sent */
static discovery_state_t discovery_state;
static uint16_t discovery_service_handle;
static uint16_t discovery_service_end_handle;
static LLBLUETOOTH_uuid_t discovery_service_uuid;
static LLBLUETOOTH_gattc_attribute_t discovery_attrs[MAX_DISCOVERY_ATTRIBUTES];
static uint16_t discovery_attr_handles[MAX_DISCOVERY_ATTRIBUTES]; /* handle each discovery_attrs entry is sorted on */
static uint8_t discovery_attr_count;
static uint16_t discovery_char_value_handles[MAX_DISCOVERY_ATTRIBUTES];
static uint8_t discovery_char_count;

/* Registers reception of notifications/indications for a discovered CCCD without writing it. */
static void register_notify_reception(struct bt_conn *conn, uint16_t cccd_handle, uint16_t value_handle);

/**
 * @brief Converts a Zephyr bt_uuid to LLBLUETOOTH_uuid_t (big-endian 128-bit).
 *        16-bit UUIDs are expanded using the Bluetooth Base UUID.
 */
static LLBLUETOOTH_uuid_t zephyr_uuid_to_ll(const struct bt_uuid *uuid) {
	LLBLUETOOTH_uuid_t lluid;
	uint8_t le[16];

	if (uuid->type == BT_UUID_TYPE_128) {
		const struct bt_uuid_128 *u128 = (const struct bt_uuid_128 *)uuid;
		for (uint8_t i = 0U; i < 16U; i++) {
			lluid.uuid[i] = u128->val[15U - i];
		}
	} else if (uuid->type == BT_UUID_TYPE_16) {
		const struct bt_uuid_16 *u16 = (const struct bt_uuid_16 *)uuid;
		/* Bluetooth Base UUID (little-endian): 00000000-0000-1000-8000-00805F9B34FB */
		static const uint8_t base_uuid_le[16] = {
			0xFBU, 0x34U, 0x9BU, 0x5FU, 0x80U, 0x00U, 0x00U, 0x80U,
			0x00U, 0x10U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
		};
		memcpy(le, base_uuid_le, sizeof(le));
		le[12] = (uint8_t)(u16->val & 0xFFU);
		le[13] = (uint8_t)(u16->val >> 8U);
		for (uint8_t i = 0U; i < 16U; i++) {
			lluid.uuid[i] = le[15U - i];
		}
	} else {
		memset(lluid.uuid, 0, sizeof(lluid.uuid));
	}

	return lluid;
}

/**
 * @brief Inserts an attribute in the discovery result, keeping the result ordered by handle.
 *
 * The BLUETOOTH Foundation Library rebuilds a service from a flat attribute list in which every
 * descriptor belongs to the characteristic preceding it. Characteristics and descriptors being
 * collected in two separate discovery phases, they have to be merged back in handle order.
 *
 * @param attr   the attribute to insert.
 * @param handle the handle the attribute is sorted on.
 *
 * @return true if the attribute was inserted, false if the result is full.
 */
static bool insert_discovery_attr(const LLBLUETOOTH_gattc_attribute_t *attr, uint16_t handle) {
	bool inserted = false;

	if (discovery_attr_count < MAX_DISCOVERY_ATTRIBUTES) {
		uint8_t index = discovery_attr_count;
		while ((index > 0U) && (discovery_attr_handles[index - 1U] > handle)) {
			discovery_attrs[index] = discovery_attrs[index - 1U];
			discovery_attr_handles[index] = discovery_attr_handles[index - 1U];
			index--;
		}
		discovery_attrs[index] = *attr;
		discovery_attr_handles[index] = handle;
		discovery_attr_count++;
		inserted = true;
	}

	return inserted;
}

/**
 * @brief Returns the value handle of the characteristic a discovered descriptor belongs to.
 *
 * A descriptor belongs to the last characteristic declared before it, hence the parent is the
 * characteristic holding the highest value handle below the descriptor handle.
 *
 * @param desc_handle handle of the descriptor.
 *
 * @return the value handle of the parent characteristic, or 0 if none was discovered.
 */
static uint16_t get_parent_value_handle(uint16_t desc_handle) {
	uint16_t parent_value_handle = 0U;

	for (uint8_t i = 0U; i < discovery_char_count; i++) {
		if ((discovery_char_value_handles[i] < desc_handle) &&
		    (discovery_char_value_handles[i] > parent_value_handle)) {
			parent_value_handle = discovery_char_value_handles[i];
		}
	}

	return parent_value_handle;
}

/**
 * @brief Ends the discovery: releases the discovery parameters and notifies the application.
 *
 * Zephyr keeps the discovery parameters until the last ATT request of the procedure is answered, so
 * they must not be reused before this point.
 *
 * @param conn active BLE connection object.
 */
static void discovery_complete(struct bt_conn *conn) {
	discovery_in_use = false;
	bt_event_search_complete(conn);
}

/**
 * @brief Aborts the discovery of a service holding more attributes than a result can carry.
 *
 * The service is not reported at all: reporting the attributes collected so far would give the
 * application a service looking complete, which would then fail on a handle it never received.
 *
 * @param conn active BLE connection object.
 */
static void discovery_abort_on_overflow(struct bt_conn *conn) {
	VEEPORT_LOGGER_LOG_ERR("Discovery aborted: service handle=%u holds more than %u attributes",
	                       discovery_service_handle, MAX_DISCOVERY_ATTRIBUTES);
	discovery_complete(conn);
}

/**
 * @brief Resumes the primary service discovery after the last handle of a reported service, or
 *        notifies the application that the discovery is over.
 *
 * The discovery always ends with a completion event, so that a phase which cannot be started never
 * leaves the application waiting for one.
 *
 * @param conn        active BLE connection object.
 * @param last_handle last handle covered by the service just reported. The scan resumes right
 *                    after it, which keeps the discovery moving forward whatever range the peer
 *                    reported.
 */
static void discover_next_service(struct bt_conn *conn, uint16_t last_handle) {
	bool completed = true;

	/* discover_params still holds the UUID filter, if any, so the next scan looks for the same
	 * services as the first one: GATT allows a peer to expose several instances of a service. */
	if (last_handle < BT_ATT_LAST_ATTRIBUTE_HANDLE) {
		discovery_state = DISCOVERY_STATE_PRIMARY;
		discover_params.start_handle = last_handle + 1U;
		discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
		int err = bt_gatt_discover(conn, &discover_params);
		if (err != 0) {
			VEEPORT_LOGGER_LOG_ERR("Next service discovery failed (err %d)", err);
		} else {
			completed = false;
		}
	}

	if (completed) {
		VEEPORT_LOGGER_LOG_INF("Discovery of all services completed");
		discovery_complete(conn);
	}
}

/**
 * @brief GATT discovery callback — state machine over PRIMARY → CHARACTERISTIC → DESCRIPTOR phases.
 */
static uint8_t discover_func(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *params) {
	if (attr == NULL) {
		/* End of current phase */
		switch (discovery_state) {
		case DISCOVERY_STATE_PRIMARY:
			VEEPORT_LOGGER_LOG_INF("Discovery: no more services found");
			discovery_complete(conn);
			break;

		case DISCOVERY_STATE_CHARACTERISTIC:
			/* All characteristics collected — now discover descriptors */
			VEEPORT_LOGGER_LOG_DBG("Discovery: %d characteristic(s) found, starting descriptor phase",
			                       discovery_char_count);
			discovery_state = DISCOVERY_STATE_DESCRIPTOR;
			memset(&inner_params, 0, sizeof(inner_params));
			inner_params.uuid = NULL;
			inner_params.func = discover_func;
			inner_params.start_handle = discovery_service_handle + 1U;
			inner_params.end_handle = discovery_service_end_handle;
			inner_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
			int err = bt_gatt_discover(conn, &inner_params);
			if (err != 0) {
				VEEPORT_LOGGER_LOG_ERR("Descriptor discovery failed (err %d)", err);
				discovery_complete(conn);
			}
			break;

		case DISCOVERY_STATE_DESCRIPTOR:
			VEEPORT_LOGGER_LOG_DBG("Discovery complete: service handle=%u end=%u, %d attr(s)", discovery_service_handle,
			                       discovery_service_end_handle, discovery_attr_count);
			/* Send the discovery result event for this service */
			bt_event_search_result(conn, discovery_service_handle,
			                       &discovery_service_uuid,
			                       discovery_attr_count, discovery_attrs);
			discover_next_service(conn, discovery_service_end_handle);
			break;

		default:
			break;
		}
		return BT_GATT_ITER_STOP;
	}

	switch (discovery_state) {
	case DISCOVERY_STATE_PRIMARY:
	{
		const struct bt_gatt_service_val *svc = attr->user_data;
		discovery_service_handle = attr->handle;
		discovery_service_end_handle = svc->end_handle;
		discovery_service_uuid = zephyr_uuid_to_ll(svc->uuid);
		discovery_attr_count = 0U;
		discovery_char_count = 0U;

		VEEPORT_LOGGER_LOG_DBG("Discovery: service found handle=%u end=%u", discovery_service_handle,
		                       discovery_service_end_handle);

		/* The end handle comes from the peer and is not checked by the stack. A range holding no
		 * attribute after the service declaration cannot be explored: bt_gatt_discover() requires
		 * a start handle lower than or equal to the end handle. */
		if (svc->end_handle <= attr->handle) {
			VEEPORT_LOGGER_LOG_WRN("Discovery: service handle=%u holds no attribute (end handle=%u)",
			                       attr->handle, svc->end_handle);
			bt_event_search_result(conn, discovery_service_handle,
			                       &discovery_service_uuid,
			                       discovery_attr_count, discovery_attrs);
			discover_next_service(conn, attr->handle);
			return BT_GATT_ITER_STOP;
		}

		/* Move to characteristic phase */
		discovery_state = DISCOVERY_STATE_CHARACTERISTIC;
		memset(&inner_params, 0, sizeof(inner_params));
		inner_params.uuid = NULL;
		inner_params.func = discover_func;
		inner_params.start_handle = attr->handle + 1U;
		inner_params.end_handle = svc->end_handle;
		inner_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
		int err = bt_gatt_discover(conn, &inner_params);
		if (err != 0) {
			VEEPORT_LOGGER_LOG_ERR("Characteristic discovery failed (err %d)", err);
			discovery_complete(conn);
		}
		/* Services are explored one at a time: stop this scan and resume it after the end handle of
		 * this service, so the services left in the response are found again by the next scan. */
		return BT_GATT_ITER_STOP;
	}

	case DISCOVERY_STATE_CHARACTERISTIC:
	{
		const struct bt_gatt_chrc *chrc = attr->user_data;
		LLBLUETOOTH_gattc_attribute_t llattr = { 0 };

		llattr.type = ATTRIBUTE_TYPE_CHARACTERISTIC;
		llattr.uuid = zephyr_uuid_to_ll(chrc->uuid);
		llattr.characteristic.value_handle = chrc->value_handle;
		llattr.characteristic.properties = chrc->properties;

		/* Sorted on the declaration handle, which precedes both the value and the descriptors of
		 * the characteristic in the attribute table. */
		if (insert_discovery_attr(&llattr, attr->handle)) {
			/* Track value handle to filter it out during descriptor phase */
			if (discovery_char_count < MAX_DISCOVERY_ATTRIBUTES) {
				discovery_char_value_handles[discovery_char_count] = chrc->value_handle;
				discovery_char_count++;
			}
			VEEPORT_LOGGER_LOG_DBG("Discovery: char value_handle=%u props=0x%02x", chrc->value_handle,
			                       chrc->properties);
		} else {
			discovery_abort_on_overflow(conn);
			return BT_GATT_ITER_STOP;
		}
		return BT_GATT_ITER_CONTINUE;
	}

	case DISCOVERY_STATE_DESCRIPTOR:
	{
		LLBLUETOOTH_gattc_attribute_t llattr = { 0 };

		/* Skip characteristic value attributes (they appear in descriptor discovery) */
		for (uint8_t i = 0U; i < discovery_char_count; i++) {
			if (attr->handle == discovery_char_value_handles[i]) {
				return BT_GATT_ITER_CONTINUE;
			}
		}

		llattr.type = ATTRIBUTE_TYPE_DESCRIPTOR;
		llattr.uuid = zephyr_uuid_to_ll(attr->uuid);
		llattr.descriptor.handle = attr->handle;

		if (insert_discovery_attr(&llattr, attr->handle)) {
			/* Route inbound notifications/indications of the parent characteristic to the
			 * application. The CCCD itself is written by the app as a raw descriptor write. */
			if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CCC) == 0) {
				register_notify_reception(conn, attr->handle, get_parent_value_handle(attr->handle));
			}
			VEEPORT_LOGGER_LOG_DBG("Discovery: desc handle=%u", attr->handle);
		} else {
			discovery_abort_on_overflow(conn);
			return BT_GATT_ITER_STOP;
		}
		return BT_GATT_ITER_CONTINUE;
	}

	default:
		return BT_GATT_ITER_STOP;
	}
}

int bt_helper_start_discovery(struct bt_conn *conn, const LLBLUETOOTH_uuid_t *uuid) {
	/* A single set of discovery parameters and phase state is shared by all connections. Starting
	 * a discovery while another one runs would reset them under the pending ATT request. */
	if (discovery_in_use) {
		VEEPORT_LOGGER_LOG_ERR("Discovery failed: a discovery is already in progress");
		return -EBUSY;
	}

	memset(&discover_params, 0, sizeof(discover_params));
	discovery_state = DISCOVERY_STATE_PRIMARY;

	if (uuid != NULL) {
		discover_uuid = bt_helper_read_uuid(uuid);
		discover_params.uuid = &discover_uuid.uuid;
	} else {
		discover_params.uuid = NULL;
	}
	discover_params.func = discover_func;
	discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	discover_params.type = BT_GATT_DISCOVER_PRIMARY;

	int err = bt_gatt_discover(conn, &discover_params);
	if (err == 0) {
		discovery_in_use = true;
	}
	return err;
}

LLBLUETOOTH_address_t bt_helper_write_addr(const bt_addr_le_t *addr) {
	LLBLUETOOTH_address_t lladdr;
	VEEPORT_LOGGER_LOG_DBG("zephyr native address:");
	BT_HELPER_LOG_ADDR(addr->a.val);
	lladdr.type = (addr->type == BT_ADDR_LE_PUBLIC ||
	               addr->type == BT_ADDR_LE_PUBLIC_ID) ? ADDRESS_TYPE_PUBLIC : ADDRESS_TYPE_PRIVATE;

	/* Zephyr address is little-endian, LLBLUETOOTH expects big-endian */
	sys_memcpy_swap(lladdr.addr, addr->a.val, sizeof(lladdr.addr));

	return lladdr;
}

/** @brief Maximum number of concurrent GATT read operations */
#define READ_PARAMS_POOL_SIZE 4U
/** @brief Maximum value size accumulated during a GATT Read Long procedure (spec max attribute length). */
#define READ_DATA_MAX_SIZE    512U

typedef struct {
	struct bt_gatt_read_params params;
	uint8_t data[READ_DATA_MAX_SIZE]; /* value reassembled across Read/Read Blob responses */
	uint16_t length;                  /* number of bytes accumulated so far */
	bool in_use;
} read_params_slot_t;

static read_params_slot_t read_params_pool[READ_PARAMS_POOL_SIZE];

int bt_helper_start_read(struct bt_conn *conn, uint16_t attr_handle) {
	read_params_slot_t *slot = NULL;

	for (uint8_t i = 0U; i < READ_PARAMS_POOL_SIZE; i++) {
		if (!read_params_pool[i].in_use) {
			slot = &read_params_pool[i];
			slot->in_use = true;
			break;
		}
	}
	if (slot == NULL) {
		VEEPORT_LOGGER_LOG_ERR("Read failed: no free read_params slot (max %u concurrent reads)",
		                       READ_PARAMS_POOL_SIZE);
		return -ENOMEM;
	}

	slot->length = 0U;
	slot->params.func = bt_event_read_complete;
	slot->params.handle_count = 1U;
	slot->params.single.handle = attr_handle;
	slot->params.single.offset = 0U;

	int err = bt_gatt_read(conn, &slot->params);
	if (err != 0) {
		slot->in_use = false;
	}
	return err;
}

void bt_helper_release_read_params(struct bt_gatt_read_params *params) {
	read_params_slot_t *slot = CONTAINER_OF(params, read_params_slot_t, params);
	slot->in_use = false;
}

int bt_helper_read_accumulate(struct bt_gatt_read_params *params, const uint8_t *data, uint16_t len) {
	read_params_slot_t *slot = CONTAINER_OF(params, read_params_slot_t, params);
	uint16_t copy_len = len;

	if (((uint32_t)slot->length + len) > READ_DATA_MAX_SIZE) {
		copy_len = (uint16_t)(READ_DATA_MAX_SIZE - slot->length);
		VEEPORT_LOGGER_LOG_WRN("Read value truncated to %u bytes", READ_DATA_MAX_SIZE);
	}
	(void)memcpy(&slot->data[slot->length], data, copy_len);
	slot->length += copy_len;

	return slot->length;
}

const uint8_t * bt_helper_read_data(struct bt_gatt_read_params *params, uint16_t *length) {
	read_params_slot_t *slot = CONTAINER_OF(params, read_params_slot_t, params);
	*length = slot->length;
	return slot->data;
}

/** @brief Maximum number of concurrent GATT write operations */
#define WRITE_PARAMS_POOL_SIZE 10U
/** @brief Maximum value size for a write operation (MTU minus the ATT write header) */
#define WRITE_DATA_MAX_SIZE    (CONFIG_BT_L2CAP_TX_MTU - BT_HELPER_ATT_WRITE_HEADER_SIZE)

typedef struct {
	struct bt_gatt_write_params params;
	uint8_t data[WRITE_DATA_MAX_SIZE];
	bool in_use;
} write_params_slot_t;

static write_params_slot_t write_params_pool[WRITE_PARAMS_POOL_SIZE];

static void write_complete_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params) {
	uint16_t handle = params->handle;
	write_params_slot_t *slot = CONTAINER_OF(params, write_params_slot_t, params);
	slot->in_use = false;
	VEEPORT_LOGGER_LOG_DBG("err=%d", err);
	bt_event_write_complete(conn, err, handle);
}

int bt_helper_start_write(struct bt_conn *conn, uint16_t attr_handle, const uint8_t *data, uint16_t len) {
	write_params_slot_t *slot = NULL;

	if (len > WRITE_DATA_MAX_SIZE) {
		VEEPORT_LOGGER_LOG_ERR("Write failed: length %u exceeds maximum %u", len, WRITE_DATA_MAX_SIZE);
		return -EINVAL;
	}

	for (uint8_t i = 0U; i < WRITE_PARAMS_POOL_SIZE; i++) {
		if (!write_params_pool[i].in_use) {
			slot = &write_params_pool[i];
			slot->in_use = true;
			break;
		}
	}
	if (slot == NULL) {
		VEEPORT_LOGGER_LOG_ERR("Write failed: no free write_params slot (max %u concurrent writes)",
		                       WRITE_PARAMS_POOL_SIZE);
		return -ENOMEM;
	}

	(void)memcpy(slot->data, data, len);
	slot->params.func = write_complete_cb;
	slot->params.handle = attr_handle;
	slot->params.offset = 0U;
	slot->params.data = slot->data;
	slot->params.length = len;

	int err = bt_gatt_write(conn, &slot->params);
	if (err != 0) {
		slot->in_use = false;
	}
	return err;
}

/** @brief Maximum number of concurrent GATT indications */
#define INDICATE_PARAMS_POOL_SIZE 10U

typedef struct {
	struct bt_gatt_indicate_params params;
	bool in_use;
} indicate_params_slot_t;

static indicate_params_slot_t indicate_params_pool[INDICATE_PARAMS_POOL_SIZE];

/** @brief Releases the slot once Zephyr is done with the indication parameters. */
static void indicate_destroy_cb(struct bt_gatt_indicate_params *params) {
	indicate_params_slot_t *slot = CONTAINER_OF(params, indicate_params_slot_t, params);

	bt_event_indication_destroy(params);
	slot->in_use = false;
}

int bt_helper_start_indicate(struct bt_conn *conn, const struct bt_gatt_attr *attr, const uint8_t *data,
                             uint16_t len) {
	int err = -ENOMEM;
	indicate_params_slot_t *slot = NULL;

	for (uint8_t i = 0U; i < INDICATE_PARAMS_POOL_SIZE; i++) {
		if (!indicate_params_pool[i].in_use) {
			slot = &indicate_params_pool[i];
			slot->in_use = true;
			break;
		}
	}

	if (NULL == slot) {
		VEEPORT_LOGGER_LOG_ERR("Indication failed: no free indicate_params slot (max %u concurrent indications)",
		                       INDICATE_PARAMS_POOL_SIZE);
	} else {
		/* bt_gatt_indicate() copies the payload into the ATT PDU before returning, so only the
		 * parameters have to outlive the call and the slot needs no data buffer. Clear the fields
		 * left by the previous indication: a stale uuid would make Zephyr resolve the attribute by
		 * UUID instead of using attr, and overwrite attr with the result. */
		slot->params = (struct bt_gatt_indicate_params) { 0 };
		slot->params.attr = attr;
		slot->params.func = bt_event_indication_cb;
		slot->params.destroy = indicate_destroy_cb;
		slot->params.data = data;
		slot->params.len = len;

		err = bt_gatt_indicate(conn, &slot->params);
		if (err != 0) {
			slot->in_use = false;
		}
	}

	return err;
}

/** @brief Maximum number of concurrent GATT subscriptions */
#define SUBSCRIBE_PARAMS_POOL_SIZE 10U

static struct bt_gatt_subscribe_params subscribe_params_pool[SUBSCRIBE_PARAMS_POOL_SIZE];
static bool subscribe_in_use[SUBSCRIBE_PARAMS_POOL_SIZE];
/* Peer a claimed slot is registered for: bt_gatt_subscribe_params carries no address, and a
 * registration is per-peer, so a slot is only reusable for the peer that claimed it. */
static bt_addr_le_t subscribe_peer[SUBSCRIBE_PARAMS_POOL_SIZE];

static uint8_t subscribe_notify_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                                   const void *data, uint16_t length) {
	if (data == NULL) {
		/* Unsubscribed — free the slot */
		for (uint8_t i = 0U; i < SUBSCRIBE_PARAMS_POOL_SIZE; i++) {
			if (&subscribe_params_pool[i] == params) {
				subscribe_in_use[i] = false;
				break;
			}
		}
		return BT_GATT_ITER_STOP;
	}
	bt_event_notification_received(conn, params->value_handle, data, length);
	return BT_GATT_ITER_CONTINUE;
}

static void register_notify_reception(struct bt_conn *conn, uint16_t cccd_handle, uint16_t value_handle) {
	if (value_handle == 0U) {
		VEEPORT_LOGGER_LOG_WRN("Notify reception: no parent characteristic for CCCD %u", cccd_handle);
		return;
	}

	const bt_addr_le_t *peer = bt_conn_get_dst(conn);

	/* A claimed slot is still linked in the peer's subscription list, the host releasing it through
	 * subscribe_notify_cb() on every removal path. So a slot claimed for this peer and this CCCD is
	 * already routing its notifications: registering a second one would duplicate the delivery and
	 * consume one more slot per rediscovery until the pool is full. */
	uint8_t slot_idx = SUBSCRIBE_PARAMS_POOL_SIZE;
	for (uint8_t i = 0U; i < SUBSCRIBE_PARAMS_POOL_SIZE; i++) {
		if (!subscribe_in_use[i]) {
			if (slot_idx == SUBSCRIBE_PARAMS_POOL_SIZE) {
				slot_idx = i;
			}
		} else if ((subscribe_params_pool[i].ccc_handle == cccd_handle) &&
		           bt_addr_le_eq(&subscribe_peer[i], peer)) {
			VEEPORT_LOGGER_LOG_DBG("Notify reception already registered for CCCD %u", cccd_handle);
			return;
		} else {
			/* another peer or another CCCD owns this slot */
		}
	}
	if (slot_idx == SUBSCRIBE_PARAMS_POOL_SIZE) {
		VEEPORT_LOGGER_LOG_ERR("Notify reception: no free subscribe slot (max %u)", SUBSCRIBE_PARAMS_POOL_SIZE);
		return;
	}

	struct bt_gatt_subscribe_params *sp = &subscribe_params_pool[slot_idx];
	(void)memset(sp, 0, sizeof(*sp));
	sp->notify = subscribe_notify_cb;
	sp->value_handle = value_handle;
	sp->ccc_handle = cccd_handle;
	sp->value = BT_GATT_CCC_NOTIFY | BT_GATT_CCC_INDICATE;

	/* resubscribe registers the notify/indicate callback WITHOUT writing the CCCD (valid for a
	 * bonded peer). The application enables notifications on the peer via a raw CCCD write; this
	 * only routes the inbound PDUs to bt_event_notification_received. */
	int err = bt_gatt_resubscribe(BT_ID_DEFAULT, peer, sp);
	if (err == 0) {
		bt_addr_le_copy(&subscribe_peer[slot_idx], peer);
		subscribe_in_use[slot_idx] = true;
	} else {
		/* -EALREADY is unreachable: bt_gatt_resubscribe only returns it for a pool entry already
		 * linked in this peer's list, which the lookup above has excluded. */
		VEEPORT_LOGGER_LOG_ERR("Notify reception registration failed (err %d)", err);
	}
}

int bt_helper_map_adv_type(int zephyr_type) {
	int result = -1;
	switch (zephyr_type) {
	case BT_GAP_ADV_TYPE_ADV_IND:
		result = ADV_TYPE_ADV_IND;
		break;
	case BT_GAP_ADV_TYPE_ADV_DIRECT_IND:
		result = ADV_TYPE_ADV_DIRECT_IND;
		break;
	case BT_GAP_ADV_TYPE_ADV_SCAN_IND:
		result = ADV_TYPE_ADV_SCAN_IND;
		break;
	case BT_GAP_ADV_TYPE_ADV_NONCONN_IND:
		result = ADV_TYPE_ADV_NONCONN_IND;
		break;
	default:
		result = zephyr_type;
		break;
	}
	return result;
}

void bt_helper_reset(void) {
	for (uint8_t i = 0U; i < READ_PARAMS_POOL_SIZE; i++) {
		read_params_pool[i].in_use = false;
	}
	for (uint8_t i = 0U; i < WRITE_PARAMS_POOL_SIZE; i++) {
		write_params_pool[i].in_use = false;
	}
	for (uint8_t i = 0U; i < INDICATE_PARAMS_POOL_SIZE; i++) {
		indicate_params_pool[i].in_use = false;
	}
	/* subscribe_params_pool is deliberately left alone: the host owns those entries until it
	 * removes them from the peer's subscription list, and every removal path ends up calling
	 * subscribe_notify_cb() with a NULL value, which releases the slot. A bonded peer keeps its
	 * subscriptions across a disconnect and across bt_disable(), so clearing the pool here would
	 * hand out slots whose parameters are still linked in that list. */
	discovery_in_use = false;
}
