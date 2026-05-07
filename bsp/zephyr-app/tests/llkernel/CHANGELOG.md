# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - 2025-04-04

### Fixed

- Fix T_LLKERNEL_CHECK_resource_install_overflow and T_LLKERNEL_CHECK_resource_install_out_of_bounds tests.
- Fix the test to verify that a ROM copy that overlaps two feature slots is forbidden.
- Fix relative paths for includes.

### Removed

- Remove useless void* casts in several tests.

## [1.1.0] - 2023-12-13

### Added

- Added new test on ``LLKERNEL_IMPL_copyToROM()`` function.

## [1.0.0] - 2023-10-12

### Added

- Added Multi-Sandbox Test Suite.

---

_Copyright 2026 MicroEJ Corp. All rights reserved._
_Use of this source code is governed by a BSD-style license that can be found with this software._
