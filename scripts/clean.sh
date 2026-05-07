#!/bin/bash
#
# BASH
#
# Copyright 2025-2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.

# remove any zephyr source and artifacts
FOLDERS=(
  ".venv"
  "bsp/.west"
  "bsp/zephyr"
  "bsp/modules"
  "bsp/optional"
  "bsp/zephyr-app/main/build"
)

for F in "${FOLDERS[@]}"; do
    if [ -d "$F" ]; then
        echo "Deleting $F ..."
        rm -rf "$F"
    else
        echo "Skipping $F: not found"
    fi
done
