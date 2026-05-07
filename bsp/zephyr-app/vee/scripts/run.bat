@ECHO off

REM BAT
REM Copyright 2021-2026 MicroEJ Corp. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be found with this software.

REM 'run.bat' implementation for Zephyr.
REM 'run.bat' is responsible for flashing the executable file on the target device

SET CURRENT_DIRECTORY=%CD%
SET VEEPORT_DIRECTORY="%~dp0\..\..\..\.."
SET WEST_BUILD_DIR="%~dp0\..\..\main"

CD %WEST_BUILD_DIR%

CALL "%~dp0\set_project_env.bat"
IF %ERRORLEVEL% NEQ 0 (
	ECHO "Error during set_project_env.bat"
	EXIT /B %ERRORLEVEL%
)

IF exist %VEEPORT_DIRECTORY%\.venv (
	IF defined VIRTUAL_ENV (
		ECHO Already in a virtual environment
	) ELSE (
		CALL %VEEPORT_DIRECTORY%\.venv\Scripts\activate.bat
	)
) ELSE (
	ECHO [WARNING] west is called outside the venv.
	ECHO [WARNING] Be sure to have all the required tools installed on your machine.
	ECHO [WARNING] If you want to use the venv, run scripts/setup.bat to create the venv and install the required tools.
)

set FLASH_CMD=west flash
if defined WEST_FLASH_ARGS (
    set FLASH_CMD=%FLASH_CMD% %WEST_FLASH_ARGS%
)

%FLASH_CMD%

IF %ERRORLEVEL% NEQ 0 (
	exit /B %ERRORLEVEL%
)

CD "%CURRENT_DIRECTORY%"

IF %ERRORLEVEL% NEQ 0 (
	EXIT /B %ERRORLEVEL%
)
