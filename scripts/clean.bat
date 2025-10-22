@echo off
REM
REM Windows Batch
REM
REM Copyright 2025 MicroEJ Corp. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be found with this software.

REM Remove any zephyr source and artifacts
SET FOLDERS=".venv" ".west" "bsp\zephyr-src" "bsp\modules" "bsp\optional" "bsp\vee\build"

for %%F in (%FOLDERS%) do (
    if exist %%~F (
        echo Deleting %%~F ...
        rmdir /S /Q %%~F
    ) else (
        echo Skipping %%~F: not found
    )
)
