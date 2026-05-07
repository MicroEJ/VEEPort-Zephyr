# Multi-Sandbox Test Suite

## Overview

This folder contains sources to test implementation of MicroEJ Multi-Sandbox.

The MicroEJ Multi-Sandbox Test Suite validates the LLAPI `LLKERNEL_impl.h`
implementation executing several tests.

All tests can be run in one step: all tests will be executed one by one
and are run in a specific order, *next one* expects *previous one* is
passed.

## Requirements

- Follow the [main Readme](https://github.com/MicroEJ/Tool-Project-Template-VEEPort/blob/master/vee-port/validation/README.md).

## Configuration

1. In your BSP project, add all files of these folders as source files:

   * `tests/llkernel/c/src/`
   * `framework/c/utils/src/`
   * `framework/c/embunit/embUnit/`

2. In your BSP project, add these folders as include paths:

   * `tests/llkernel/c/inc/`
   * `framework/c/utils/inc/`
   * `framework/c/embunit/embUnit/`

3. Locate the call to `microej_main` in the BSP project. Include the `t_llkernel_main.h` header file in this file, and add a call to the function `T_LLKERNEL_main` just before the call to `microej_main`.
4. Build the BSP and link it with the Architecture library and the Application.

## Expected Results

```
T_LLKERNEL v1.2.0
	.
	T_LLKERNEL_setUp
	LLKERNEL resource allocate
	T_LLKERNEL_tearDown
	.
	T_LLKERNEL_setUp
	LLKERNEL resource allocate and free
	T_LLKERNEL_tearDown
	.
	T_LLKERNEL_setUp
	LLKERNEL resource allocate overflow
	T_LLKERNEL_tearDown
	.
	T_LLKERNEL_setUp
	LLKERNEL resource install overflow
	T_LLKERNEL_tearDown
	.
	T_LLKERNEL_setUp
	LLKERNEL resource install out of bounds
	T_LLKERNEL_tearDown
	.
	T_LLKERNEL_setUp
	LLKERNEL resource installation
	T_LLKERNEL_tearDown

	OK (6 tests)
```

No tests should fail when executing this test.

---

_Copyright 2026 MicroEJ Corp. All rights reserved._
_Use of this source code is governed by a BSD-style license that can be found with this software._
