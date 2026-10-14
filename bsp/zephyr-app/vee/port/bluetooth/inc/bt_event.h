/*
 * Copyright 2024-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

/**
 * @file bt_event.h
 * @brief Bluetooth event implementation.
 * @author MicroEJ Developer Team
 * @version 1.0.0
 * @date 10 October 2024
 */

#ifndef BT_EVENT_H
#define BT_EVENT_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sni.h"
#include "LLBLUETOOTH_defines.h"
#include "LLBLUETOOTH_impl.h"
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include "host/conn_internal.h"
#include "host/att_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Structure describing an event passing through the queue. */
typedef struct bt_event_info_t {
	LLBLUETOOTH_event_t *event;
	uint32_t size;
} bt_event_info_t;

/**
 *  @brief Free the dynamic memory that holds an event.
 *  @param  event   Event to free
 */
void bt_event_free(LLBLUETOOTH_event_t *event);

/**
 *  @brief  Gets an event from the event queue
 *  @param  event_info The reference to the event
 *  @return 0 if SUCCESS, error code otherwise
 */
ssize_t bt_event_queue_get(bt_event_info_t *event_info);

/**
 *  @brief Puts an event in the event queue
 *  @param  event_info The reference to the event
 *  @return 0 if SUCCESS, error code otherwise
 */
ssize_t bt_event_queue_put(bt_event_info_t *event_info);

/**
 *  @brief Gets the suspended thread id
 */
int32_t bt_event_get_suspended_thread_id();

/**
 *  @brief Sets the suspended thread id
 */
void bt_event_set_suspended_thread_id(uint16_t thread_id);

/**
 *  @brief Gets the connection reference
 */
struct bt_conn * bt_event_get_connection();

/**
 *  @brief Gets the current connection handle
 */
uint16_t bt_event_get_current_connection_handle();

/**
 *  @brief Puts the pair response in the queue of the bt_event_pairing_confirm event
 *
 *  @param data     Value of the pair response
 *
 * 	@return         0 if success, error code otherwise
 */
int bt_event_pairing_request_put(int8_t *data);

/** @brief notify that pairing procedure was complete.
 *
 *  This callback notifies the application that the pairing procedure
 *  has been completed.
 *
 *  @param conn Connection object.
 *  @param bonded Bond information has been distributed during the
 *                pairing procedure.
 */
void bt_event_pairing_complete(struct bt_conn *conn, bool bonded);

/**
 * @brief notify that the pairing procedure failed.
 *
 * This callback notifies the user that the pairing procedure has failed with the provided reason code.
 *
 * @param conn Connection object.
 * @param reason Reason for the pairing failure.
 */
void bt_event_pairing_failed(struct bt_conn *conn, enum bt_security_err reason);

/**
 * @brief notify that the pairing procedure is cancelled.
 *
 * This callback notifies the user that the pairing procedure has been cancelled.
 *
 * @param conn Connection object.
 */
void bt_event_pairing_cancel(struct bt_conn *conn);

/**
 * @brief notify a pairing confirmation.
 *
 * This callback notifies the user that the pairing needs to be confirmed.
 *
 * @param conn Connection object.
 */
void bt_event_pairing_confirm(struct bt_conn *conn);

/** @brief A new connection has been established.
 *
 *  This callback notifies the application of a new connection.
 *  In case the err parameter is non-zero it means that the
 *  connection establishment failed.
 *
 *  @param conn New connection object.
 *  @param err HCI error. Zero for success, non-zero otherwise.
 *
 *  @p err can mean either of the following:
 *  - @ref BT_HCI_ERR_UNKNOWN_CONN_ID Creating the connection started by
 *    @ref bt_conn_le_create was canceled either by the user through
 *    @ref bt_conn_disconnect or by the timeout in the host through
 *    @ref bt_conn_le_create_param timeout parameter, which defaults to
 *    @option{CONFIG_BT_CREATE_CONN_TIMEOUT} seconds.
 *  - @p BT_HCI_ERR_ADV_TIMEOUT High duty cycle directed connectable
 *    advertiser started by @ref bt_le_adv_start failed to be connected
 *    within the timeout.
 */
void bt_event_connected(struct bt_conn *conn, uint8_t err);

/** @brief A connection has been disconnected.
 *
 *  This callback notifies the application that a connection
 *  has been disconnected.
 *
 *  When this callback is called the stack still has one reference to
 *  the connection object. If the application in this callback tries to
 *  start either a connectable advertiser or create a new connection
 *  this might fail because there are no free connection objects
 *  available.
 *  To avoid this issue it is recommended to either start connectable
 *  advertise or create a new connection using @ref k_work_submit or
 *  increase @option{CONFIG_BT_MAX_CONN}.
 *
 *  @param conn Connection object.
 *  @param reason HCI reason for the disconnection.
 */
void bt_event_disconnected(struct bt_conn *conn, uint8_t reason);

/** @brief The parameters for an LE connection have been updated.
 *
 *  This callback notifies the application that the connection
 *  parameters for an LE connection have been updated.
 *
 *  @param conn Connection object.
 *  @param interval Connection interval.
 *  @param latency Connection latency.
 *  @param timeout Connection supervision timeout.
 */
void bt_event_le_param_updated(struct bt_conn *conn, uint16_t interval,
                               uint16_t latency, uint16_t timeout);

/**
 *  @brief Puts the read response in the queue of the bt_event_read_request event
 *
 * 	@param value    Value of the read response
 *  @param len      Length of the read response
 * 	@param err      ATT error code.
 *
 * 	@return         0 if success, error code otherwise
 */
int bt_event_read_request_put(const uint8_t *value, uint16_t len, int8_t err);

/** @brief Attribute read callback
 *
 *  The callback can also be used locally to read the contents of the
 *  attribute in which case no connection will be set.
 *
 *  @param conn   The connection that is requesting to read
 *  @param attr   The attribute that's being read
 *  @param buf    Buffer to place the read result in
 *  @param len    Length of data to read
 *  @param offset Offset to start reading from
 *
 *  @return Number fo bytes read, or in case of an error
 *          BT_GATT_ERR() with a specific ATT error code.
 */
ssize_t bt_event_read_request(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len,
                              uint16_t offset);

/**
 *  @brief Puts the write response in the queue of the bt_event_write_request event
 *
 * 	@param data     Value of the write response
 *
 * 	@return         0 if success, error code otherwise
 */
int bt_event_write_request_put(int8_t *data);

/** @brief Attribute write callback
 *
 *  @param conn   The connection that is requesting to write
 *  @param attr   The attribute that's being written
 *  @param buf    Buffer with the data to write
 *  @param len    Number of bytes in the buffer
 *  @param offset Offset to start writing from
 *  @param flags  Flags (BT_GATT_WRITE_*)
 *
 *  @return Number of bytes written, or in case of an error
 *          BT_GATT_ERR() with a specific ATT error code.
 */
ssize_t bt_event_write_request(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
                               uint16_t offset, uint8_t flags);

/**
 *  @brief Indicate operation complete callback
 *
 *  @param params Indication params object.
 */
void bt_event_indication_destroy(struct bt_gatt_indicate_params *params);

/** @typedef bt_gatt_indicate_func_t
 *  @brief Indication complete result callback.
 *
 *  @param conn Connection object.
 *  @param params Indication params object.
 *  @param err ATT error code
 */
void bt_event_indication_cb(struct bt_conn *conn, struct bt_gatt_indicate_params *params, uint8_t err);

/**
 * @brief Notification complete callback.
 *
 * @param conn Connection object.
 * @param user_data User defined data: used here to pass a pointer to the attribute handle.
 */
void bt_event_notification_complete(struct bt_conn *conn, void *user_data);

/**
 *  @brief Sets the scan filter parameters applied to incoming scan results.
 *
 *  @param action    One of SCAN_FILTER_ACTION_* values
 *  @param type      AD type to match
 *  @param data      AD data to match (may be NULL when action is FIELD_EXISTS)
 *  @param data_size Length of data
 */
void bt_event_set_scan_filter(uint8_t action, uint8_t type, const uint8_t *data, uint8_t data_size);

/**
 * @brief Callback for reporting LE scan results.
 *
 * A function given to the @ref bt_le_scan_start function
 * and will be called for any discovered LE device.
 *
 * @param addr Advertiser LE address and type.
 * @param rssi Strength of advertiser signal.
 * @param adv_type Type of advertising response from advertiser.
 *                 Uses the @ref bt_gap_adv_type values.
 * @param buf Buffer containing advertiser data.
 */
void bt_event_scan_device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad);

/**
 * @brief Emits the EVENT_GAP_SCAN_COMPLETED event to the application.
 *
 * Called when LE scanning stops, whether explicitly via stopScanning() or implicitly when a
 * connection is initiated.
 */
void bt_event_scan_completed(void);

/**
 * @brief Sends a GATTC discovery result event for one discovered service.
 *
 * @param conn          Active BLE connection object.
 * @param service_handle  Handle of the discovered service.
 * @param service_uuid    UUID of the discovered service.
 * @param num_attributes  Number of attributes (characteristics + descriptors) in the service.
 * @param attrs           Array of attributes to copy into the event.
 */
void bt_event_search_result(struct bt_conn *conn, uint16_t service_handle,
                            const LLBLUETOOTH_uuid_t *service_uuid,
                            uint8_t num_attributes,
                            const LLBLUETOOTH_gattc_attribute_t *attrs);

/**
 * @brief Sends a GATTC discovery complete event.
 *
 * @param conn          Active BLE connection object.
 */
void bt_event_search_complete(struct bt_conn *conn);

uint8_t bt_event_read_complete(struct bt_conn *conn, uint8_t att_err,
                               struct bt_gatt_read_params *params, const void *data, uint16_t read_len);

/**
 * @brief Sends a GATTC write completed event.
 *
 * @param conn       Active BLE connection object.
 * @param status     ATT error code (0 = success).
 * @param attr_handle  Handle of the written attribute.
 */
void bt_event_write_complete(struct bt_conn *conn, uint8_t status, uint16_t attr_handle);

/**
 * @brief Sends a GATTC notification received event.
 *
 * @param conn        Active BLE connection object.
 * @param attr_handle Handle of the characteristic value that sent the notification.
 * @param data        Notification payload.
 * @param length      Length of the payload in bytes.
 */
void bt_event_notification_received(struct bt_conn *conn, uint16_t attr_handle,
                                    const void *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* BT_EVENT_H */
