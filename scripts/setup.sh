#!/bin/bash
#
# BASH
#
# Copyright 2025-2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.

# Create venv if not already created
python3 -m venv .venv

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
python -m pip install --upgrade pip setuptools wheel
pip install west
west init -l bsp/zephyr-app
cd bsp
west update
west zephyr-export
west packages pip --install
west patch apply

# install git submodules
git submodule update --init --recursive

# install zephyr sdk, it will detect if it is already installed in the user $HOME
west sdk install --no-hosttools -t arm-zephyr-eabi
