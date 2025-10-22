#!/bin/bash
#
# BASH
#
# Copyright 2025 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.

# remove any zephyr source and artifacts
FOLDERS=(
  ".venv"
  ".west"
  "bsp/zephyr-src"
  "bsp/modules"
  "bsp/optional"
  "bsp/vee/build"
)

for F in "${FOLDERS[@]}"; do
    if [ -d "$F" ]; then
        echo "Deleting $F ..."
        rm -rf "$F"
    else
        echo "Skipping $F: not found"
    fi
done
