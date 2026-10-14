# Changelog
   
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [4.0.0] - 2026-10-09

### Added

  - Add Bluetooth Central role support (scanning, connection, pairing, GATT read/write, and notifications/indications).
  - Add a `System.arraycopy()` throughput check to the Core validation testsuite, with a configurable minimum throughput for each of the four copies it measures.

### Changed

  - Update Zephyr to 4.4.2.
  - Update MicroEJ SDK to 1.8.0.
  - Update to Bluetooth 3.0.0.
  - Update to NET Pack 15.0.1.
  - Update the Crypto (formerly Security) Foundation Library to 2.1.0.
  - Update the SSL testsuite to 6.0.0.
  - Build the Crypto and SSL Foundation Libraries on Mbed TLS 4.1 and TF-PSA-Crypto.
  - Use picolibc as the C library instead of newlib.
  - Select the target board with a single `-Pveeport.board=<name>` Gradle property (`mimxrt1170`, `mcxn947`, `nrf54l15`) instead of editing `set_project_env.sh` and `gradle/libs.versions.toml`.
  - Configure boards with Zephyr's own mechanisms instead of per-board CMake files.
  - Integrate ABLA Essentials version 1.1.0.
  - Log the abstraction layers through the Zephyr logging subsystem, at the error level by default.
  - Throw the exceptions of the abstraction layers through the ABLA Essentials error helpers.
  - Bluetooth implementation reject a connection request while a connection is already ongoing, handling a single connection at a time.

### Fixed

  - Fix the shebang location for bash BSP scripts.
  - Fix `setup.sh` and `setup.bat` continuing after a step failed.
  - Fix a race condition in the async worker waiting threads list.
  - Fix a system halt when a filesystem operation was given an invalid file or directory handle.
  - Fix a corruption of the directory pool when closing a directory failed, which let two directories share one entry.
  - Fix merge-request CI dropping all but one board: the dispatcher's concurrent per-board worker builds no longer abort each other (worker sets `DISABLE_CONCURRENT_BUILDS=false`; the dispatcher owns supersede-on-new-push via `disableConcurrentBuilds(abortPrevious: true)`).
  - Fix link failure by ordering `microejapp.o` before `microejruntime.a`, resolving their circular symbol dependency in a single linker pass.
  - Fix a corruption of the Bluetooth subscription pool when disabling Bluetooth, the pool being reset while the host still referenced its entries.
  - Fix the duplicated notifications and the subscription pool exhaustion when rediscovering a peer, each rediscovery registering the same CCCD once more.
  - Fix the SSL certificate parsing errors reported as `J_UNKNOWN_ERROR` instead of their own error code.

### Removed

  - Remove the DES and DESede ciphers, including the `PBEWithSHA1AndDESede` password-based cipher and the PKCS#12 key derivation, no longer provided by TF-PSA-Crypto.
  - Remove the `secp192r1`, `secp192k1`, `secp224r1` and `secp224k1` elliptic curves, not longer supported by TF-PSA-Crypto.
  - Remove the TLS 1.2 RSA, RSA-PSK, DHE and static ECDH key exchanges.
  - Remove the local Zephyr patches for the nRF54L15 UARTE and the i.MX RT1170 LPUART, fixed upstream in Zephyr 4.4.

## [3.1.0] - 2026-05-07

### Added

  - Add Core Engine Testsuite (RAM, Coremark, ...).

### Changed

  - Switch from -Og to -O2 compilation optimization flag.
  - Optimize FS Zephyr configuration for each board.
  - Update NET buffers configuration to improve TCP download/upload performances.
  - Change the default task priority layout: network AsyncSelect (1) and FS workers (2) now run above the Core Engine task (3), the Security worker (4) below.

### Fixed

  - Fix `LLSERIAL_CONNECTION_IMPL_close` that wait for the UART to finish disabling.

## [3.0.0] - 2026-04-27

### Added

  - Add Security/SSL Foundations/testsuite.
  - Add Serial Foundation Library support.
  - Add Bluetooth pack, only support partial implementation of the Peripheral role.

### Changed
  
  - Merge Zephyr Abstraction Layers inside Zephyr VEE Port git repository
  - Update MicroEJ SDK to 1.6.0
  - Migration to Zephyr v4.3.0.
  - Update to NET Pack 13.0.0.
  - Update to Architecture 8.6.0.
  - Update to Event Queue 3.0.3.
  - Update to Device 1.4.0.

## [2.0.0] - 2025-11-21

### Added
  - Add Simulator support
  - Add Event-Queue pack/testsuite
  - Add MicroAI pack/testsuite
  - Add Device pack
  - Add i.MXRT1170-EVK board support example
  - Add FRDM-MCXN947 board support example
  - Add nRF54L15 DK board support example

### Changed
  - Update to MicroEJ SDK gradle plugin 1.4.0
  - Update MicroEJ Core test suite to 3.5.0
  - Move Zephyr VEE Port specific configurations to `vee-port/bsp/boards` folder

### Removed
  - Remove Renode support

## [1.1.0] - 2025-09-02

### Added
  - Support for different runing modes
  - Support for different filesystem (FatFS and LittleFS)
  - Configurtion file for the rt1064
  - Testsuite for FS ABLA
  - Testsuite for network interface
  - Set the RT1064 as default board for renode

## [1.0.0] - 2025-05-05

### Added

  - Initial release
  - Port Zephyr on Renode disco_l475_iot1 board

---
_Markdown_  
_Copyright 2024-2026 MicroEJ Corp. All rights reserved._  
_Use of this source code is governed by a BSD-style license that can be found with this software._  
_Build: 7E4D1F7C_  
