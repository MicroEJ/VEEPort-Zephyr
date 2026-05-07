# Overview

MicroEJ Application: `ble_minimal_scope_test`.

This test enables users to verify the proper functioning of the advertisement. It simply advertises its name in BLE and is connectable.
Modify the DEVICE_NAME variable in Main.java to set the name you want to advertise.

## LLAPIs to implement
- LLBLUETOOTH_IMPL_enable
- LLBLUETOOTH_IMPL_waitEvent
- LLBLUETOOTH_IMPL_startAdvertising

## Events to implement
- EVENT_GAP_CONNECTED
- EVENT_GAP_DISCONNECTED

# Flow

Here's the flow of the ble_minimal_scope_test:

1. The app is configured to advertise its name.
2. Launch a BLE application on your mobile (nRF Connect, ST BLE Toolbox...), start the scanning.
You can filter the devices by name "minim..." to find ours.
3. Check that our device correctly advertises its name with an interval of 100ms.

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