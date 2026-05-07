@echo off
REM
REM Windows Batch
REM
REM Copyright 2025-2026 MicroEJ Corp. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be found with this software.

REM Create venv if not already created
python3 -m venv .venv

REM Detect OS and activate the correct venv
IF EXIST ".venv\Scripts\activate.bat" (
    CALL ".venv\Scripts\activate.bat"
) ELSE (
    ECHO Could not find the virtual environment activation script.
    EXIT /B 1
)

REM Install zephyr source and tools
python -m pip install --upgrade pip setuptools wheel
pip install west
west init -l bsp\zephyr-app
CD bsp
west update
west zephyr-export
west packages pip --install
west patch apply

REM Install gitsubmodules
git submodule update --init --recursive

REM Install Zephyr SDK (will detect if already installed in user home)
west sdk install -t arm-zephyr-eabi
