/*
 * Copyright 2024-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

/**
 * @file bt_helper.h
 * @brief Bluetooth Helper
 * @author MicroEJ Developer Team
 * @date 4 September 2024
 */

#ifndef BT_HELPER_H
#define BT_HELPER_H

#include <stdint.h>
#include <stdio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include "bt_event.h"
#include "veeport_logger.h"

/**
 * @brief Verbosity of the log modules of the Bluetooth Abstraction Layer.
 *
 * The layer keeps one log module per source file, so that each can be filtered on its own at
 * runtime, but they share a single verbosity, set with CONFIG_MICROEJ_BLUETOOTH_LOG_LEVEL.
 */
#define BT_LOG_MODULE_LEVEL VEEPORT_LOGGER_MODULE_LEVEL(BLUETOOTH)

/** @brief Logs a 6-byte BT address array (e.g. LLBLUETOOTH_address_t.addr or bt_addr_le_t.a.val) at debug level. */
#define BT_HELPER_LOG_ADDR(a) VEEPORT_LOGGER_LOG_DBG("%02X:%02X:%02X:%02X:%02X:%02X", (a)[0], (a)[1], (a)[2], (a)[3], \
													 (a)[4], (a)[5])

/** @brief Size of the ATT write header (1-byte opcode + 2-byte attribute handle).
 *  The largest value that fits a single ATT write PDU is therefore ATT_MTU minus this size. */
#define BT_HELPER_ATT_WRITE_HEADER_SIZE    (3U)

/**
 *  @brief Helper to free all the elements allocated in a service
 *
 *  @param service  The GATT service to free from the heap
 */
void bt_helper_free_service(struct bt_gatt_service *service);

/**
 *  @brief Helper to add a service in the GATT Database before calling bt_gatt_service_register()
 *
 *  @param attr_service The attribute to fill with the service
 *  @param uuid         The UUID of the service
 *
 *  @return         1 if the service has well been added, 0 otherwise
 */
uint8_t bt_helper_add_service(struct bt_gatt_attr *attr_service, const LLBLUETOOTH_uuid_t *uuid);

/**
 *  @brief Helper to add a characteristic in the GATT Database before calling bt_gatt_service_register()
 *
 *  @param attr_char_decl   The attribute to fill with the characteristic declaration
 *  @param attr_char_val    The attribute to fill with the characteristic value
 *  @param uuid             The UUID of the service
 *  @param perms            Attribute permissions
 *  @param props            Attribute properties
 *
 *  @return         1 if the characteristic has well been added, 0 otherwise
 */
uint8_t bt_helper_add_characteristic(struct bt_gatt_attr *attr_char_decl, struct bt_gatt_attr *attr_char_val,
                                     const LLBLUETOOTH_uuid_t *uuid, uint8_t perms, uint8_t props);

/**
 *  @brief Helper to add a descriptor in the GATT Database before calling bt_gatt_service_register()
 *
 *  @param attr_desc    The attribute to fill with the descriptor
 *  @param uuid         The UUID of the service
 *  @param perms        Attribute permissions
 *
 *  @return         1 if the descriptor has well been added, 0 otherwise
 */
uint8_t bt_helper_add_descriptor(struct bt_gatt_attr *attr_desc, const LLBLUETOOTH_uuid_t *uuid, uint8_t perms);

/**
 *  @brief Helper to find an attribute in the GATT database by its handle
 *
 *  @param attr_handle  The handle of the attribute to find
 *
 *  @return             the attribute if found, NULL otherwise
 */
const struct bt_gatt_attr * bt_helper_find_attr(uint16_t attr_handle);

/**
 * @brief Converts the given Bluetooth address from zephyr format to LLBLUETOOTH format.
 *
 * @param[in] addr      the bluetooth address in esp-idf format.
 *
 * @return the bluetooth address in LLBLUETOOTH format.
 */

LLBLUETOOTH_address_t bt_helper_write_addr(const bt_addr_le_t *addr);

/**
 * @brief Converts the given Bluetooth address from LLBLUETOOTH format to Zephyr format.
 *
 * @param[in] addr  the bluetooth address in LLBLUETOOTH format.
 *
 * @return the bluetooth address in Zephyr format.
 */
bt_addr_le_t bt_helper_read_addr(const LLBLUETOOTH_address_t *addr);

/**
 * @brief Converts the given Bluetooth UUID from LLBLUETOOTH format to Zephyr 128-bit UUID format.
 *
 * @param[in] uuid  the bluetooth UUID in LLBLUETOOTH format (big-endian 16 bytes).
 *
 * @return the bluetooth UUID as a Zephyr bt_uuid_128.
 */
struct bt_uuid_128 bt_helper_read_uuid(const LLBLUETOOTH_uuid_t *uuid);

/**
 * @brief Starts a GATT service discovery on the given connection.
 *
 * Once started, the discovery reports each service found and always ends with a completion event,
 * including when it is interrupted by an error. Only one discovery runs at a time, whatever the
 * connection: starting another one before the completion event fails with -EBUSY.
 *
 * A service is reported with all of its attributes or not at all: a service holding more attributes
 * than a result event can carry ends the discovery, leaving the services already reported valid.
 *
 * @param conn  Active BLE connection object.
 * @param uuid  UUID of the services to discover, or NULL to discover all services. A peer may
 *              expose several instances of the same service: each of them is reported.
 * @return 0 on success, negative error code otherwise.
 */
int bt_helper_start_discovery(struct bt_conn *conn, const LLBLUETOOTH_uuid_t *uuid);

/**
 * @brief Starts a GATT service discovery on the given connection.
 *
 * @param conn  Active BLE connection object.
 * @param attr_handle the attribute handle.
 * @return 0 on success, negative error code otherwise.
 */
int bt_helper_start_read(struct bt_conn *conn, uint16_t attr_handle);

/**
 * @brief Releases a read_params slot back to the pool after a read callback.
 *
 * @param params  Pointer to the read_params that was passed to the callback.
 */
void bt_helper_release_read_params(struct bt_gatt_read_params *params);

/**
 * @brief Appends a chunk of a GATT read response to the read's accumulation buffer.
 *
 * Used to reassemble a value split across the GATT Read Long procedure. The buffer is
 * capped at the maximum attribute length; excess bytes are dropped.
 *
 * @param params  the read parameters identifying the ongoing read.
 * @param data    the chunk data.
 * @param len     the chunk length in bytes.
 * @return the total number of bytes accumulated so far.
 */
int bt_helper_read_accumulate(struct bt_gatt_read_params *params, const uint8_t *data, uint16_t len);

/**
 * @brief Returns the value accumulated so far for a GATT read.
 *
 * @param[in]  params  the read parameters identifying the read.
 * @param[out] length  set to the number of accumulated bytes.
 * @return pointer to the accumulated value buffer.
 */
const uint8_t * bt_helper_read_data(struct bt_gatt_read_params *params, uint16_t *length);

/**
 * @brief Starts a GATT write request (with response) on the given connection.
 *
 * @param conn        Active BLE connection object.
 * @param attr_handle The attribute handle to write to.
 * @param data        The value to write.
 * @param len         Length of the value in bytes.
 * @return 0 on success, negative error code otherwise.
 */
int bt_helper_start_write(struct bt_conn *conn, uint16_t attr_handle, const uint8_t *data, uint16_t len);

/**
 * @brief Sends a GATT indication on the given connection.
 *
 * The indication parameters are taken from an internal pool because bt_gatt_indicate() is
 * asynchronous: Zephyr keeps them until the peer confirms. The slot is released automatically
 * once the procedure completes.
 *
 * @param conn Active BLE connection object.
 * @param attr The attribute whose value changed.
 * @param data The value to indicate.
 * @param len  Length of the value in bytes.
 * @return 0 on success, negative error code otherwise.
 */
int bt_helper_start_indicate(struct bt_conn *conn, const struct bt_gatt_attr *attr, const uint8_t *data, uint16_t len);

/**
 * @brief Maps the native zephyr advertising type to upper layer type.
 *
 * @param zephyr_type Native zephyr advertising type.
 * @return the corresponding upper layer type.
 */
int bt_helper_map_adv_type(int zephyr_type);

/**
 * @brief Resets the GATT client per-session state (read/write/indicate pools, discovery).
 *
 * Called after bt_disable() so that a subsequent enable() starts from clean pools. The subscription
 * pool is not part of it: the host releases those slots itself when it removes the corresponding
 * entry from the peer's subscription list.
 */
void bt_helper_reset(void);

#endif /* BT_HELPER_H */
