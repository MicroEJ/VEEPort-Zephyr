![SDK](https://shields.microej.com/endpoint?url=https://repository.microej.com/packages/badges/sdk_6.0.json)
![ARCH](https://shields.microej.com/endpoint?url=https://repository.microej.com/packages/badges/arch_8.3.json)

# MicroEJ VEE Port for Zephyr

This project is used to build a MicroEJ VEE Port for Zephyr.

It relies on the Generic Abstraction Layer for Zephyr available [here](https://github.com/MicroEJ/AbstractionLayer-Zephyr/blob/master/README.md).

Ensure you have cloned the repository and all its submodules.

To update git submodules, run the following command `git submodule update --init --recursive` at the root of this project.

## Related Files

This directory also contains:

* [CHANGELOG](./CHANGELOG.md) to track the changes in the VEE Port
* [RELEASE NOTES](./RELEASE_NOTES.md) to list:

    - the supported hardware,
    - the known issues and the limitations,
    - the development environment,
    - the list of the dependencies and their versions.

* [README Abstraction Layer Zephyr](https://github.com/MicroEJ/AbstractionLayer-Zephyr/blob/master/README.md) recommended for users familiar with the BSP
  and advanced usage on how to customize the build process.


## VEE Port Specifications

The architecture version is `8.3.0`.

This VEE Port provides the following Foundation Libraries:

| Foundation Library | Version |
|--------------------|---------|
| BON                | 1.4     |
| DEVICE             | 1.2     |
| EVENT              | 2.1     |
| FS                 | 2.1     |
| KF                 | 1.7     |
| NET                | 1.1     |
| RESOURCEMANAGER    | 1.0     |
| SECURITY           | 1.7     |
| SNI                | 1.4     |
| WATCHDOG           | 1.0     |

The VEE Port is derived into:

- a Mono-Sandbox VEE Port (default)
- a Multi-Sandbox VEE Port

## Requirements

- PC with either Windows or Linux.
- Internet connection to [MicroEJ Central Repository](https://developer.microej.com/central-repository/).
- [Zephyr supported board](https://docs.zephyrproject.org/latest/boards/index.html#).

## VEE Port Setup

[Open the VEE Port project in your favorite IDE](https://docs.microej.com/en/latest/SDK6UserGuide/importProject.html)
by selecting the root folder of the cloned repository.

The project contains the following subprojects:

- `vee-port`: Contains the VEE Port configuration description.
  Some modules are described in a specific sub-folder / with some optional configuration files (`.properties` and/or `.xml`).

- `vee-port/bsp`: Contains specific Zephyr board configurations files for the Zephyr Abstraction Layer.

- `bsp/zephyr-src`: Contains Zephyr source code.

- `bsp/modules` and `bsp/optional`: Contains Zephyr needed modules.

- `bsp/vee`: Contains a ready-to-use Zephyr Abstraction Layer, including a CMake project and
  an implementation of MicroEJ Core Engine (and extensions) port on Zephyr.

- `app`: Contains a sample application which uses the VEE Port.

- `vee-port:validation`: Contains the testsuite to validate the implementation

By default, the VEE Port is configured as a Mono-Sandbox Evaluation
VEE Port (Refer to the [RELEASE NOTES](./RELEASE_NOTES.md) limitations section for more details).

## Run an Application on the Simulator

To run an Application on the Simulator, the BSP or C/C++ Toolchain are not required.

- Run the `app` sample Application [on the Simulator](https://docs.microej.com/en/latest/SDK6UserGuide/runOnSimulator.html).
  This automatically builds the VEE Port and uses it.

The build may take several minutes.
The first time, the VEE Port build requires to download modules that are available on the MicroEJ Central Repository.

Wait for the Application start. It is a simple Hello World printed in the console.

An evaluation license is needed for building an Application. Refer to
https://docs.microej.com/en/latest/SDK6UserGuide/licenses.html#evaluation-licenses
for information on how to acquire and activate a license.


## Run an Application on the Device

To build and run the Application on the Device, a Zephyr supported board, the Zephyr SDK and Zephyr sources are required.

### Host tools dependencies

Follow [Zephyr dependencies installation procedure](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-dependencies) to install the required dependencies according to your OS.

### Zephyr setup

Follow [Zephyr installation procedure](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#get-zephyr-and-install-python-dependencies) to install Zephyr and its Python dependencies.

Or even simpler, use the [setup.sh](./scripts/setup.sh) or [setup.bat](./scripts/setup.bat) helper scripts to install all the required dependencies.

This script will fetch the Zephyr source using `west`, then install all the required Python packages, using a Python `venv`.
Finally, it will install the Zephyr SDK, if it is not already installed on the host environment.

#### Linux / Git bash for Windows

```sh
./scripts/setup.sh
```

#### Windows

```powershell
.\scripts\setup.bat
```

### Board Setup

This generic Zephyr VEE Port contains configuration examples for the following three cards:

- [NXP MIMXRT1170-EVK](https://docs.zephyrproject.org/latest/boards/nxp/mimxrt1170_evk/doc/index.html) used by default.
- [NXP FRDM-MCXN947](https://docs.zephyrproject.org/latest/boards/nxp/frdm_mcxn947/doc/index.html).
- [Nordic nRF54L15 DK](https://docs.zephyrproject.org/latest/boards/nordic/nrf54l15dk/doc/index.html).

If you have another Zephyr supported board, please follow the [How to configure a new board](#how-to-configure-a-new-board) section 
to configure the VEE Port before continuing to [Build and Run the Application](#build-and-run-the-application) section.

If you wish to use one of the three example cards, please follow the respective Zephyr documentation to set up the board 
and install the necessary tools to program it.

MIMXRT1170-EVK is the default configuration.
To change the board, update [set_project_env.sh](vee-port/bsp/scripts/set_project_env.sh) or [set_project_env.bat](vee-port/bsp/scripts/set_project_env.bat) depending on your OS to set the new `BOARD` environment variable.
(e.g. `export BOARD=frdm_mcxn947/mcxn947/cpu0)` for FRDM-MCXN947).

Then update the architecture inside [libs.versions.toml](gradle/libs.versions.toml) depending on your board:
* MIMXRT1170-EVK: `architecture = { group = "com.microej.architecture.CM7.CM7hardfp_GCC48", name = "flopi7G26", version = "8.3.0" }`
* FRDM-MCXN947 or nRF54L15 DK: `architecture = { group = "com.microej.architecture.CM4.CM4hardfp_GCC48", name = "flopi4G25", version = "8.3.0" }`

Have a look at your board configuration on the VEE Port:

#### NXP MIMXRT1170-EVK configuration

* [mimxrt1170_evk_mimxrt1176_cm7.cmake](vee-port/bsp/boards/mimxrt1170_evk_mimxrt1176_cm7.cmake)
  * `BOARD_FLASH_RUNNER` is set to `linkserver`, can be set to `jlink` if needed.
  * `nxp,enet1g.overlay` added to use the [1G Ethernet instance](https://docs.zephyrproject.org/latest/boards/nxp/mimxrt1170_evk/doc/index.html#enet1g-driver).
* [mimxrt1170_evk_mimxrt1176_cm7.conf](vee-port/bsp/boards/mimxrt1170_evk_mimxrt1176_cm7.conf)
  * `FatFS` File System deployed in the SD Card.
* [mimxrt1170_evk_mimxrt1176_cm7.overlay](vee-port/bsp/boards/mimxrt1170_evk_mimxrt1176_cm7.overlay)
  * Set a unique MAC address for the 1G Ethernet instance.
  * Configuration if FS is deployed in RAM.

#### NXP FRDM-MCXN947 configuration

* [frdm_mcxn947_mcxn947_cpu0.cmake](vee-port/bsp/boards/frdm_mcxn947_mcxn947_cpu0.cmake)
    * `BOARD_FLASH_RUNNER` is set to `linkserver`, can be set to `jlink` if needed.
* [frdm_mcxn947_mcxn947_cpu0.conf](vee-port/bsp/boards/frdm_mcxn947_mcxn947_cpu0.conf)
    * `FatFS` File System deployed in the SD Card.
    * Kernel buffer size where features will be deployed is set to 4KB.
    * FRDM-MCXN947 does not provide a Random Generator.
    * Reduce NET buffer memory consumption.
* [frdm_mcxn947_mcxn947_cpu0.overlay](vee-port/bsp/boards/frdm_mcxn947_mcxn947_cpu0.overlay)
    * Set a unique MAC address Ethernet instance.

#### Nordic nRF54L15 DK configuration

* [nrf54l15dk_nrf54l15_cpuapp.cmake](vee-port/bsp/boards/nrf54l15dk_nrf54l15_cpuapp.cmake)
* [nrf54l15dk_nrf54l15_cpuapp.conf](vee-port/bsp/boards/nrf54l15dk_nrf54l15_cpuapp.conf)
  * Disable MicroAI Abstraction Layer -> Takes too much RAM.
  * Disable NET Abstraction Layer -> No Ethernet instance.
  * `LittleFS` File System deployed in the internal flash.
  * Kernel buffer size where features will be deployed is set to 4KB.
* [nrf54l15dk_nrf54l15_cpuapp.overlay](vee-port/bsp/boards/nrf54l15dk_nrf54l15_cpuapp.overlay)
    * Increase storage partitions to 128KB.

### Build and Run the Application

- Run the `app` sample Application [on Device](https://docs.microej.com/en/latest/SDK6UserGuide/runOnDevice.html).
  This automatically builds the VEE Port and uses it.

The build may take several minutes.  
The first time, the VEE Port build requires to download modules that are available on the MicroEJ Central Repository.  
The VEE Port will also build the `bsp` project and link it with MicroEJ VEE and the Application to produce an executable.

Wait for the Application start. It is a simple Hello World printed in the board's serial port.

## Advanced Tips

### BSP Manual Compilation

This section shows how to build the `bsp` project manually without having to rebuild the MicroEJ application.

Note: this can only be done if `buildExecutable` or `runOnDevice` has been called at least once, see [Run on Device](https://docs.microej.com/en/latest/SDK6UserGuide/runOnDevice.html)

To build the `bsp` project, open a
terminal and enter the following command lines:

**On Windows:**

```sh
$ cd "bsp/vee/scripts"
$ build.bat
```

**On Linux:**

```sh
$ cd "bsp/vee/scripts"
$ build.sh
```

You can now run your program:

**On Windows:**
```sh
$ cd "bsp/vee/scripts"
$ run.bat
```

**On Linux:**

```sh
$ cd "bsp/vee/scripts"
$ run.sh
```

### How to configure a new board

If you want to use this VEE Port with a board that is not part of the sample boards:

* Follow [Zephyr board documentation](https://docs.zephyrproject.org/latest/boards/index.html) to set up the board and install the necessary tools to program it.
* Select the architecture that corresponds to your board and update it on [libs.versions.toml](gradle/libs.versions.toml) file. See [MicroEJ supported Architectures](https://developer.microej.com/mej32-supported-embedded-runtime-architectures/).
* Add missing Zephyr modules:
  * Your board could require additional Zephyr modules (e.g. `hal-nxp` for NXP boards).
  * Add the Zephyr modules to [bsp/west.yml](bsp/west.yml).
  * Run the `west update` command to fetch the newly added modules.
* Add the board configuration files on [vee-port/bsp/boards](vee-port/bsp/boards) folder:
  * Configurations files names must start with the Zephyr Board identifier so that Zephyr takes it into account during the build (e.g. `mimxrt1170_evk_mimxrt1176_cm7` for MIMXRT1170-EVK).
  * **Required file**
    * `<board_name>.cmake`: Set specific BSP configurations such as the flash runner, and extended overlays. 
    It also allows the user to specify specific Abstraction Layers properties that will overwrite Zephyr Abstraction Layer ones (see [Abstraction Layer configuration](#abstraction-layer-configuration) section).
    You can start from the example configuration for the MIMXRT1170 found in [mimxrt1170_evk_mimxrt1176_cm7.cmake](vee-port/bsp/boards/mimxrt1170_evk_mimxrt1176_cm7.cmake) file.    
  * **Optional files**
    * `<board_name>.overlay`: Extends your board device tree.
    * `<board_name>.conf`: Set specific KConfigs.
* Update [set_project_env.sh](vee-port/bsp/scripts/set_project_env.sh) or [set_project_env.bat](vee-port/bsp/scripts/set_project_env.bat) depending on your OS to set the new `BOARD` environment variable.

## Board Configuration

To enable/disable modules, configure each Foundation Library feature by following the instructions of the [VEE Port Configuration](https://github.com/MicroEJ/AbstractionLayer-Zephyr/blob/master/README.md#vee-port-configuration) section.

This project relies on the Zephyr Configuration system.
For detailed information, have a look at Zephyr's [Application Development](https://docs.zephyrproject.org/latest/develop/application/index.html#application) guide.

---
_Markdown_  
_Copyright 2024-2025 MicroEJ Corp. All rights reserved._  
_Use of this source code is governed by a BSD-style license that can be found with this software._ 
