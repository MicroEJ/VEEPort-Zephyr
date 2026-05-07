# VEE Port Release Notes for Zephyr

## Description

This is the release notes of the VEE Port for Zephyr.

## Versions

### VEE Port

3.0.0

### Dependencies

This VEE Port contains the following dependencies:

| Dependency Name                   |
|-----------------------------------|
| Architecture (default: flopi7G26) |
| Pack DEVICE                       |
| Pack EVENT                        |
| Pack FS                           |
| Pack MICROAI                      |
| Pack NET                          |
| Pack SERIAL                       |


Please refer to the Abstraction Layer [README Abstraction Layer Zephyr](https://github.com/MicroEJ/AbstractionLayer-Zephyr/blob/master/README.md)
for the MicroEJ pack versions.

### Board Support Package

- BSP provider and name: Zephyr
- BSP version: v4.3.0

Please refer to the Zephyr git repository
available [here](https://github.com/zephyrproject-rtos/zephyr).

## Features

### Architecture

The Multi-Sandbox Application download implementation is based on Best Fit Allocator backend in RAM, so without reboot persistence.
Features are allocated in a fixed-size dedicated buffer.

### Device Pack

The Device UID is based on the Zephyr [Hardware Information](https://docs.zephyrproject.org/latest/hardware/peripherals/hwinfo.html) API.

### Network Pack

This VEE Port features a network interface with ethernet as an underlying hardware media.

### File System Pack

This VEE Port features a file system interface.
RAM, Flash or SD card could be used for the storage (previously formated to FatFS or LittleFS file system) depending on the VEE Port configurations.

### MicroAI Pack

MicroAI implementation is based on TensorFlow Lite for Microcontrollers.

### Serial Pack

This VEE Port features a serial foundation library via UART.

## Known Issues/Limitations

Limitations:

- Wi-Fi not supported by NET implementation. The board needs an Ethernet connector.

- The following features of the file system are not available:
  - Setting last modified date
  - Getting last modified date
  - Synchronized I/O file integrity completion (FS_O_SYNC not supported by Zephyr)
  - Classes impacted by these limitations are the following:
    - [File](https://repository.microej.com/javadoc/microej_5.x/apis/java/io/File.html)
    - [RandomAccessFile](https://repository.microej.com/javadoc/microej_5.x/apis/java/io/RandomAccessFile.html)

- The security test suite can't pass fully on RAM constrained platforms, such as FRDM-MCXN947. However you can either:
  - disable other Foundation libraries (ex: MicroAI, KF, etc.)
    in [frdm_mcxn947_mcxn947_cpu0.conf](vee-port/bsp/boards/frdm_mcxn947_mcxn947_cpu0.conf)
    ```
    CONFIG_MICROEJ_MICROAI=n
    ```
  - or reduce the key size to 2048 bytes
    in [microej-testsuite-common.properties](vee-port/validation/security/validation/microej-testsuite-common.properties):
    ```
    microej.java.property.security.keypairgen.rsakeysizes=1024, 2048
    microej.java.property.security.rsacipher.rsakeysizes=1024, 2048
    ```

---
_Markdown_  
_Copyright 2025-2026 MicroEJ Corp. All rights reserved._  
_Use of this source code is governed by a BSD-style license that can be found with this software._  
