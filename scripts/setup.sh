#!/bin/bash
#
# BASH
#
# Copyright 2025-2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.
#
# Build: 7E4D1F7C

# Parse arguments.
# --shallow: narrow + depth-1 `west update`. Fetches only the pinned manifest revision (not every
#   remote ref) and makes each project a depth-1 clone. CI only needs the pinned source to build, so
#   this is a large time/bandwidth win; local dev keeps the full clone (default) for history/bisect.
WEST_UPDATE_ARGS=""
for arg in "$@"; do
    case "$arg" in
        --shallow)
            WEST_UPDATE_ARGS="--narrow --fetch-opt=--depth=1"
            ;;
        *)
            echo "Unknown argument: $arg" >&2
            echo "Usage: $0 [--shallow]" >&2
            exit 1
            ;;
    esac
done

# Every step this script runs is required for the BSP build, so a step that fails stops the
# script. A missing prerequisite is then reported here, instead of surfacing much later as an
# unrelated CMake, Kconfig or compiler error. `west update` needs this most: it exits non-zero
# when a manifest project is unreachable, but still updates every other project, so the failure
# is easy to miss in the output.
run_step() {
    if ! "$@"; then
        echo "$0: step failed: $*" >&2
        exit 1
    fi
}

# Create venv if not already created
run_step python3 -m venv .venv

# Detect OS and source the correct activate script
if [ -f ".venv/bin/activate" ]; then
    # Linux/macOS
    source .venv/bin/activate
elif [ -f ".venv/Scripts/activate" ]; then
    # Windows (Git Bash, Cygwin, etc.)
    source .venv/Scripts/activate
else
    echo "Could not find the virtual environment activation script."
    exit 1
fi

# install zephyr source and tools
run_step python -m pip install --upgrade pip setuptools wheel
run_step pip install west

# `west init` refuses to run on an already initialized workspace, so a re-run of this script
# reuses the existing one.
if [ ! -d "bsp/.west" ]; then
    run_step west init -l bsp/zephyr-app
fi
run_step cd bsp
run_step west update ${WEST_UPDATE_ARGS}
run_step west zephyr-export
run_step west packages pip --install

# Not checked: `git apply` fails on an already patched tree, so this step exits non-zero on
# every re-run over a workspace it already patched.
west patch apply

# install git submodules
run_step git submodule update --init --recursive

# install zephyr sdk, it will detect if it is already installed in the user $HOME
run_step west sdk install --no-hosttools -t arm-zephyr-eabi
