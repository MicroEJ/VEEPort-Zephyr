/*
 * Copyright 2024-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

/**
 * @file bt_helper.h
 * @brief Bluetooth Helper
 * @author MicroEJ Developer Team
 * @date 4 September 2024
 */

#include <stdint.h>
#include <stdio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include "bt_event.h"

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
