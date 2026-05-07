# Overview

Welcome to this guide for developers to validate step by step the implementation the Bluetooth LE VEE Port on their own platform. This validation folder contains several tests, which call all the different LLBLUETOOTH APIs and cover different typical scenarios.

According to your final BLE use case, you shall be led to start your implementation either by the Peripheral role or by the Central role. So, you can adapt this guide following to your LLBLUETOOTH APIs development process.

The tests run on SDK6 environment.

Here is an example of order of validation tests with a peripheral approach : 

	1) advertising_name
	2) my_first_gatt_db
	3) serial_service
	4) peripheral_sc
	5) scanner
	6) simple_central
	7) central
	8) central_sc

# Notes

Please, note that depending on your application platform (iOS, Android, Windows...), there may have some differences in the display of the GATT Database (some descriptors
may be hidden, some may have different arrows). These choices are implementation specific and should not disrupt your develoment.

# Requirements

- MicroEJ SDK 6
- VEE Port with at least bluetooth-pack 2.4.1

# Dependencies

Please, make sure to add your VEE Port dependency in the build.gradle.kts file in all the tests.

---
_Copyright 2025-2026 MicroEJ Corp. All rights reserved._  
_Use of this source code is governed by a BSD-style license that can be found with this software._  