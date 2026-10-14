@echo off
REM
REM Windows Batch
REM
REM Copyright 2025-2026 MicroEJ Corp. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be found with this software.
REM
REM Build: 7E4D1F7C

REM Every step this script runs is required for the BSP build, so a step that fails stops the
REM script. A missing prerequisite is then reported here, instead of surfacing much later as an
REM unrelated CMake, Kconfig or compiler error. `west update` needs this most: it exits non-zero
REM when a manifest project is unreachable, but still updates every other project, so the failure
REM is easy to miss in the output.

REM Create venv if not already created
python3 -m venv .venv || EXIT /B 1

REM Detect OS and activate the correct venv
IF EXIST ".venv\Scripts\activate.bat" (
    CALL ".venv\Scripts\activate.bat"
) ELSE (
    ECHO Could not find the virtual environment activation script.
    EXIT /B 1
)

REM Install zephyr source and tools
python -m pip install --upgrade pip setuptools wheel || EXIT /B 1
pip install west || EXIT /B 1

REM `west init` refuses to run on an already initialized workspace, so a re-run of this script
REM reuses the existing one.
IF NOT EXIST "bsp\.west" (
    west init -l bsp\zephyr-app || EXIT /B 1
)
CD bsp || EXIT /B 1
west update || EXIT /B 1
west zephyr-export || EXIT /B 1
west packages pip --install || EXIT /B 1

REM Not checked: `git apply` fails on an already patched tree, so this step exits non-zero on
REM every re-run over a workspace it already patched.
west patch apply

REM Install gitsubmodules
git submodule update --init --recursive || EXIT /B 1

REM Install Zephyr SDK (will detect if already installed in user home)
west sdk install -t arm-zephyr-eabi || EXIT /B 1
