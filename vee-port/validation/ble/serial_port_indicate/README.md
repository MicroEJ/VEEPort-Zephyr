# Overview

MicroEJ Application: `Serial_port_indicate`.
This test enables developers to validate their serial_port_scope implementation of the Bluetooth VEE Port. This
application
works with its twin serial_port_notify, both are almost identical. In this application, the TX characteristic is
Indicate whereas
in serial_port_notify, the TX characteristic is Notify. To validate this scope, both of the tests should be passed with
success.

## LLAPIs to implement

- LLBLUETOOTH_IMPL_enable
- LLBLUETOOTH_IMPL_waitEvent
- LLBLUETOOTH_IMPL_startAdvertising
- LLBLUETOOTH_IMPL_addService
- LLBLUETOOTH_IMPL_sendReadResponse
- LLBLUETOOTH_IMPL_sendWriteResponse
- LLBLUETOOTH_IMPL_sendNotification

## Events to implement

- EVENT_GAP_CONNECTED
- EVENT_GAP_DISCONNECTED
- EVENT_GATTS_READ_REQUEST
- EVENT_GATTS_WRITE_REQUEST
- EVENT_GATTS_NOTIFICATION_SENT

Here is the attribute table exposed by the application :

                                                UUID                            Properties
    Service                |     0783b03e-8535-b5a0-7140-a304d2495cb7       |
        Characteristic     |     0783b03e-8535-b5a0-7140-a304d2495cb8       |   INDICATE
            Descriptor     |     2901                                       |   READ
            Descriptor     |     2902                                       |   READ | WRITE
        Characteristic     |     0783b03e-8535-b5a0-7140-a304d2495cba       |   WRITE
            Descriptor     |     2901                                       |   READ

## Flow

Check the success of the app by following the flow below :

1. Launch a BLE application on your mobile (nRF Connect recommended), start the scanning.
   You can filter the devices by name Ser.. to find ours.
2. Once found, connect to the device.
3. Discover the characteristics' names by reading their characteristic user description.
   TX for the first characteristic and RX for the second one should appear.
4. Subscribe to the TX characteristic by clicking on the double arrows.
5. Write any value in the RX characteristic with the up-arrow button.
6. The application automatically indicates in the TX characteristic what has been
   written in the RX one.
7. Check you received the data sent in the 'Value' field of the TX characteristic.

# Requirements

* MICROEJ SDK 6.
* A VEE Port that contains:
    * EDC-1.3 or higher.
    * BON-1.4 or higher.
    * BLUETOOTH-2.2 or higher.

This example has been tested on:

* IntelliJ IDEA Community Edition 2024.1.3
* CE Brands VEE Port

## Run on simulator

Not tested

## Run on device

Make sure to properly setup the VEE Port environment before going further.
Refer to the VEE Port README for more information.

In Android Studio:

- Open the Gradle tool window by clicking on the elephant on the right side,
- Expand the `Tasks` list,
- From the `Tasks` list, expand the `microej` list,
- Double-Click on `runOnDevice`.
- The device is flashed. Use the appropriate tool to retrieve the execution traces.

Alternative ways to run on device are described in
the [Run on Device](https://docs.microej.com/en/latest/SDK6UserGuide/runOnDevice.html) documentation.

# Dependencies

_All dependencies are retrieved transitively by Gradle_.

# Source

N/A

# Restrictions

None.

---
_Copyright 2018-2026 MicroEJ Corp. All rights reserved._  
_Use of this source code is governed by a BSD-style license that can be found with this software._  
