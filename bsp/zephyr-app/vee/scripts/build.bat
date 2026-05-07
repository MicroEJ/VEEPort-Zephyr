@ECHO off
SETLOCAL ENABLEEXTENSIONS

REM BAT
REM Copyright 2021-2026 MicroEJ Corp. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be found with this software.

REM 'build.bat' implementation for Zephyr.

REM 'build.bat' is responsible for producing the executable file
REM then copying this executable file to the current directory where it has been executed to a file named 'application.out'

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

set BUILD_CMD=west build -p=auto
if defined WEST_BUILD_ARGS (
    set BUILD_CMD=%BUILD_CMD% %WEST_BUILD_ARGS%
)

%BUILD_CMD%

IF %ERRORLEVEL% NEQ 0 (
	EXIT /B %ERRORLEVEL%
)

COPY build\zephyr\zephyr.elf "%CURRENT_DIRECTORY%\application.out"

IF %ERRORLEVEL% NEQ 0 (
	EXIT /B %ERRORLEVEL%
)

CD "%CURRENT_DIRECTORY%"

IF %ERRORLEVEL% NEQ 0 (
	EXIT /B %ERRORLEVEL%
)
