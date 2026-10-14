@echo off

REM BAT
REM Copyright 2021-2026 MicroEJ Corp. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be found with this software.
REM
REM Build: 7E4D1F7C

REM 'set_project_env.bat' implementation for MicroEJ Windows builds.
REM 'set_project_env' is responsible for
REM - setting project variables for 'build.bat' and 'run.bat'

REM BOARD is provided by the Gradle build (see gradle/boards.gradle.kts), which writes the resolved
REM Zephyr board name to the generated '.board.env' beside this script (a single bare value). Reading it
REM here lets the west build use the board selected via -Pveeport.board. The default below is only the
REM standalone fallback (manual build.bat with no Gradle run); keep it consistent with the Gradle default.
SET "BOARD="
IF EXIST "%~dp0.board.env" SET /P BOARD=<"%~dp0.board.env"
IF NOT DEFINED BOARD SET "BOARD=mimxrt1170_evk/mimxrt1176/cm7"

REM Set additional arguments to the west build command
REM Don't use quotes, otherwise the variables expansion will fail, ex:
REM SET WEST_BUILD_ARGS=--board=frdm_mcxn947/mcxn947/cpu0 -d new_dir

REM Set additional arguments to the west flash command
REM SET WEST_FLASH_ARGS=""

