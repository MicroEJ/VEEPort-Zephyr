@echo off

REM BAT
REM Copyright 2021-2025 MicroEJ Corp. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be found with this software.

REM 'set_project_env.bat' implementation for MicroEJ Windows builds.
REM 'set_project_env' is responsible for
REM - setting project variables for 'build.bat' and 'run.bat'

SET BOARD=mimxrt1170_evk/mimxrt1176/cm7

REM Set additional arguments to the west build command
REM Don't use quotes, otherwise the variables expansion will fail, ex:
REM SET WEST_BUILD_ARGS=--board=frdm_mcxn947/mcxn947/cpu0 -d new_dir

REM Set additional arguments to the west flash command
REM SET WEST_FLASH_ARGS=""

