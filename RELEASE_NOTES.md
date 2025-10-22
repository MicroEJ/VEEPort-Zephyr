# VEE Port Release Notes for Zephyr

## Description

This is the release notes of the VEE Port for Zephyr.

## Versions

### VEE Port

2.0.0

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


Please refer to the Abstraction Layer [README Abstraction Layer Zephyr](https://github.com/MicroEJ/AbstractionLayer-Zephyr/blob/master/README.md)
for the MicroEJ pack versions.

### Board Support Package

- BSP provider and name: Zephyr
- BSP version: 4.1

Please refer to the Zephyr git repository
available [here](https://github.com/zephyrproject-rtos/zephyr).

## Features

### Network

This VEE Port features a network interface with ethernet as an underlying hardware media.

### File System

This VEE Port features a file system interface.
RAM, Flash or SD card could be used for the storage (previously formated to FatFS or LittleFS file system) depending on the VEE Port configurations.

## MISRA Compliance

The MISRA compliance report is available in [Abstraction-Layer-Zephyr](https://github.com/MicroEJ/AbstractionLayer-Zephyr/blob/master/README.md) project.

## Known Issues/Limitations

Limitations:

- Wi-Fi not supported by NET implementation. The board needs an Ethernet connector.

- The following feature of the file system are not available:
  - Setting last modified date
  - Getting last modified date
  - Synchronize related feature

---
_Markdown_  
_Copyright 2025 MicroEJ Corp. All rights reserved._  
_Use of this source code is governed by a BSD-style license that can be found with this software._  
