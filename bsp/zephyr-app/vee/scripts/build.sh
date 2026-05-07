# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.

#!/bin/bash
#
# BASH
#

# 'build.sh' implementation for Zephyr.

# 'build.sh' is responsible for producing the executable file 
# then copying this executable file to the current directory where it has been executed to a file named 'application.out'

set -euo pipefail

current_dir=$(pwd)
scripts_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd) || exit
veeport_dir="${scripts_dir}/../../../.."
west_build_dir="${scripts_dir}/../../main"
bsp_dir="${scripts_dir}/../../.."

cd "${west_build_dir}" || exit

. "${scripts_dir}/set_project_env.sh" || exit 1


# Zephyr scripts may have lost their execution flag is they come from a zip
find "${bsp_dir}/zephyr/scripts" \( -name "*.py" -o -name "*.sh" \) | xargs chmod +x 2>/dev/null || true

if [ -d "${veeport_dir}/.venv" ]; then
	if [[ -n "${DEPLOY_ENV+x}" ]]; then
		echo "Already in a virtual environment"
	else
		. "${veeport_dir}/.venv/bin/activate"
	fi
else
	echo "[WARNING] west is called outside the venv."
	echo "[WARNING] Be sure to have all the required tools installed on your machine."
	echo "[WARNING] If you want to use the venv, run scripts/setup.sh to create the venv and install the required tools."
fi

# Append optional arguments to the default build command
BUILD_CMD=(west build -p=auto)
if [[ ! -z "${WEST_BUILD_ARGS:-}" ]]; then
    BUILD_CMD+=(${WEST_BUILD_ARGS})
fi

"${BUILD_CMD[@]}"

cp build/zephyr/zephyr.elf "${current_dir}/application.out"

cd "${current_dir}" || exit
