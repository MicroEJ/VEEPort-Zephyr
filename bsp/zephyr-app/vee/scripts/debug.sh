# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.

#!/bin/bash
#
# BASH
#

# 'debug.sh' implementation for Zephyr.

# 'debug.sh' is responsible for flashing the executable file on the target device and launch a GDB session.

set -euo pipefail

current_dir=$(pwd)
scripts_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd) || exit
veeport_dir="${scripts_dir}/../../../.."
west_build_dir="${scripts_dir}/../../main"

cd "${west_build_dir}" || exit

. "${scripts_dir}/set_project_env.sh" || exit 1

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

# Append optional arguments to the default debug command
DEBUG_CMD=(west debug)
if [[ ! -z "${WEST_FLASH_ARGS:-}" ]]; then
    DEBUG_CMD+=(${WEST_FLASH_ARGS})
fi

"${DEBUG_CMD[@]}"

cd "${current_dir}" || exit
