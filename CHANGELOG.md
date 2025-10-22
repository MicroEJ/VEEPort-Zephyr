# Changelog
   
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
_Copyright 2024-2025 MicroEJ Corp. All rights reserved._  
_Use of this source code is governed by a BSD-style license that can be found with this software._  
