# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.

#!/bin/bash
#
# BASH
#

# 'clean.sh' implementation
# 'clean.sh' is responsible for cleaning the project

set -euo pipefail

current_dir=$(pwd)
scripts_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd) || exit
west_build_dir="${scripts_dir}/../../main"

cd "${west_build_dir}" || exit

rm -rf build

cd "${current_dir}" || exit
