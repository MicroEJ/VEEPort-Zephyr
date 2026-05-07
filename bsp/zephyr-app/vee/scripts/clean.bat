@ECHO off

REM BAT
REM Copyright 2026 MicroEJ Corp. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be found with this software.

REM 'clean.bat' implementation
REM 'clean.bat' is responsible for cleaning the project

SET CURRENT_DIRECTORY=%CD%
SET WEST_BUILD_DIR="%~dp0\..\..\main"

CD %WEST_BUILD_DIR%

IF %ERRORLEVEL% NEQ 0 (
	ECHO "Can not open %WEST_BUILD_DIR%"
	EXIT /B %ERRORLEVEL%
)

IF EXIST build RD /s /Q build

IF %ERRORLEVEL% NEQ 0 (
	EXIT /B %ERRORLEVEL%
)

CD "%CURRENT_DIRECTORY%"

IF %ERRORLEVEL% NEQ 0 (
	EXIT /B %ERRORLEVEL%
)
