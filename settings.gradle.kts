/*
 * Kotlin
 *
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

rootProject.name = "zephyr"
include("vee-port", "vee-port:front-panel")
include("app:hello-world")
include("app:serial-echo")

project(":vee-port:front-panel").projectDir = file("vee-port/extensions/front-panel")

include("vee-port:validation:core")
include("vee-port:validation:event-queue")
include("vee-port:validation:fs")
include("vee-port:validation:microai")
include("vee-port:validation:net")
include("vee-port:validation:security")
include("vee-port:validation:serial")
include("vee-port:validation:ssl")
// BLE test applications
include("vee-port:validation:ble:ble_minimal_scope_test")
include("vee-port:validation:ble:ble_add_service_scope_test_2")
include("vee-port:validation:ble:serial_port_indicate")
include("vee-port:validation:ble:serial_port_notify")
include("vee-port:validation:ble:BLE_Throughput_Server")