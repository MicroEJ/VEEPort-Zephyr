#!/bin/bash

# Copyright 2026 MicroEJ Corp. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found with this software.
#
# Build: 7E4D1F7C

VENV=0
activate_venv() {
	if [ -d "../../.venv" ]; then 
	if [[ -v DEPLOY_ENV ]]; then 
	   echo Already in a virtual environment
	   VENV=1
	else
	   source ../../.venv/bin/activate
	   echo "Getting in venv"
	fi
	else
	    echo you must create a python .venv and install all the required tools:
	    echo     west zephyr-export
	    echo     west packages pip --install
	    exit 1
	fi
}


deactivate_venv() {	
	if [[ "$VENV" -eq 0 ]]; then
    		deactivate
		echo "Deactivating venv"
	fi
}
