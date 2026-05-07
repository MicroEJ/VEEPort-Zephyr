# Overview

MicroEJ Application: `ble_add_service_scope_test_2`.

This test enables developers to validate their add_service_scope implementation of the Bluetooth VEE Port. This application
completes ble_add_service_scope_test as it brings a bigger GATT database, 4 services, new properties used like Indicate. If this test passes without errors, we can consider the add_service_scope validated.

## LLAPIs to implement
- LLBLUETOOTH_IMPL_enable
- LLBLUETOOTH_IMPL_waitEvent
- LLBLUETOOTH_IMPL_startAdvertising
- LLBLUETOOTH_IMPL_addService

## Events to implement
- EVENT_GAP_CONNECTED
- EVENT_GAP_DISCONNECTED

Note : For the following scopes, do not use this test because the device won't respond to any request in the database. 

Here are the attribute table exposed by the application :

                                                UUID                            Properties
    Service                |     0783b03e-8535-b5a0-7140-a304d2495cb7       |
        Characteristic     |     0783b03e-8535-b5a0-7140-a304d2495cb8       |   NOTIFY
            Descriptor     |     2901                                       |   READ
            Descriptor     |     2902                                       |   READ | WRITE
        Characteristic     |     0783b03e-8535-b5a0-7140-a304d2495cba       |   WRITE
            Descriptor     |     2901                                       |   READ

    Service                |     d58c8541-0113-4e37-b3e3-d3229512694b       |
        Characteristic     |     d58c8541-0113-4e37-b3e3-d3229512694c       |   READ
        Characteristic     |     d58c8541-0113-4e37-b3e3-d3229512694d       |   WRITE

    Service                |     6675126c-94ba-46dc-afb6-7bb3db955be3       |
        Characteristic     |     6675126c-94ba-46dc-afb6-7bb3db955be4       |   INDICATE
            Descriptor     |     2902                                       |   READ | WRITE
        Characteristic     |     6675126c-94ba-46dc-afb6-7bb3db955be5       |   READ

    Service                |     8dc871e7-671a-48fa-803a-962d9bb39af1       |
        Characteristic     |     8dc871e7-671a-48fa-803a-962d9bb39af2       |   READ | WRITE
        Characteristic     |     8dc871e7-671a-48fa-803a-962d9bb39af3       |   READ   

# Flow

Here's the flow of the ble_add_service_scope_test_2:

1. The app is configured to provide the 4 services.
2. The device starts advertising with the name "add_service_scope_2".
3. Launch a BLE application on your mobile (nRF Connect, ST BLE Toolbox...), start the scanning.
You can filter the devices by name "add_..." to find ours.
4. Once found, connect to the device.
5. If the connection is done, on nRF Connect, click on client tab and check that the attribute table 
has been created with the desired values.


## Run on Simulator
Not tested

## Run on Device
In the SDK6 environment, launch the Run on Device command, the application should be built and flashed to the target (make sure your JLink Flash works).

# Requirements

This application has been tested on :
    - MicroEJ SDK 6
    - With the CE Brands Development board
        - The platform contains bluetooth-pack 2.4.1

# Dependencies

_All dependencies are retrieved transitively by Gradle_.

# Source

N/A.

# Restrictions

None.

---
_Copyright 2025-2026 MicroEJ Corp. All rights reserved._  
_Use of this source code is governed by a BSD-style license that can be found with this software._  