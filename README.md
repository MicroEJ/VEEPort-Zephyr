![SDK](https://shields.microej.com/endpoint?url=https://repository.microej.com/packages/badges/sdk_6.0.json)
![ARCH](https://shields.microej.com/endpoint?url=https://repository.microej.com/packages/badges/arch_8.3.json)

# MicroEJ VEE Port for Zephyr

This project is used to build a MicroEJ VEE Port for Zephyr.

## Related Files

This directory contains:

* [CHANGELOG](./CHANGELOG.md) to track the changes in the VEE Port
* [RELEASE NOTES](./RELEASE_NOTES.md) to list:

    - the supported hardware,
    - the known issues and the limitations,
    - the development environment,
    - the list of the dependencies and their versions.

## VEE Port Specifications

The architecture version is `8.3.0`.

This VEE Port provides the following Foundation Libraries:

| Foundation Library | Version |
|--------------------|---------|
| BON                | 1.4     |
| BLUETOOTH          | 2.4     |
| DEVICE             | 1.2     |
| EVENT              | 2.1     |
| FS                 | 2.1     |
| KF                 | 1.7     |
| NET                | 1.1     |
| RESOURCEMANAGER    | 1.0     |
| SECURITY           | 1.7     |
| SERIAL             | 3.0     |
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

- `app`: Contains a sample application which uses the VEE Port.

- `bsp/zephyr-app`: Zephyr West workspace root

- `bsp/zephyr-app/main`: Contains the default entry point for the MicroEJ VEE Zephyr application.

- `bsp/zephyr-app/main/boards`: Contains specific Zephyr board configuration files.

- `bsp/zephyr-app/tests`: Contains Zephyr applications to run native test code, see [Native Tests](#native-tests) section for more information.

- `bsp/zephyr-app/vee`: Contains the Abstraction Layers supported by this VEE.

- `bsp/modules` and `bsp/optional`: Contains modules required by Zephyr.

- `bsp/zephyr`: Contains Zephyr source code.

- `vee-port`: Contains the VEE Port configuration description.
  Some modules are described in a specific sub-folder / with some optional configuration files (`.properties` and/or `.xml`).

- `vee-port/validation`: Contains the testsuite to validate the implementation

By default, the VEE Port is configured as a Mono-Sandbox Evaluation
VEE Port (Refer to the [RELEASE NOTES](./RELEASE_NOTES.md) limitations section for more details).

## Run an Application on the Simulator

To run an Application on the Simulator, the BSP or C/C++ Toolchain are not required.

- Run the `app` sample Application [on the Simulator](https://docs.microej.com/en/latest/SDK6UserGuide/runOnSimulator.html).
  This automatically builds the VEE Port and uses it.

The build may take several minutes.
The first time, the VEE Port build requires to download modules that are available on the MicroEJ Central Repository.

Wait for the Application start. It is a simple Hello World printed in the console.

## Run an Application on the Device

An evaluation license is needed for building an Application on the Device. Refer to
https://docs.microej.com/en/latest/SDK6UserGuide/licenses.html#evaluation-licenses
for information on how to acquire and activate a license.

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
To change the board, update [set_project_env.sh](bsp/zephyr-app/vee/scripts/set_project_env.sh) or [set_project_env.bat](bsp/zephyr-app/vee/scripts/set_project_env.bat) depending on your OS to set the new `BOARD` environment variable.
(e.g. `export BOARD=frdm_mcxn947/mcxn947/cpu0)` for FRDM-MCXN947).

Then update the architecture inside [libs.versions.toml](gradle/libs.versions.toml) depending on your board:
* MIMXRT1170-EVK: `architecture = { group = "com.microej.architecture.CM7.CM7hardfp_GCC48", name = "flopi7G26", version = "8.3.0" }`
* FRDM-MCXN947 or nRF54L15 DK: `architecture = { group = "com.microej.architecture.CM4.CM4hardfp_GCC48", name = "flopi4G25", version = "8.3.0" }`

Have a look at your board configuration on the VEE Port:

#### NXP MIMXRT1170-EVK configuration

* [mimxrt1170_evk_mimxrt1176_cm7.cmake](bsp/zephyr-app/main/boards/mimxrt1170_evk_mimxrt1176_cm7.cmake)
  * `BOARD_FLASH_RUNNER` is set to `linkserver`, can be set to `jlink` if needed.
  * `nxp,enet1g.overlay` added to use the [1G Ethernet instance](https://docs.zephyrproject.org/latest/boards/nxp/mimxrt1170_evk/doc/index.html#enet1g-driver).
* [mimxrt1170_evk_mimxrt1176_cm7.conf](bsp/zephyr-app/main/boards/mimxrt1170_evk_mimxrt1176_cm7.conf)
  * `FatFS` File System deployed in the SD Card.
    * Set FS read/write buffer to 32KB (4 worker dedicated to the FS implementation -> 128KB used for read/write buffers).
    * This configuration improve the FS read/write performances.
      * **Optimized configuration** Read: 10000 KB/s, Write: 2000 KB/s 
      * **Default configuration** Read: 1500 KB/s, Write: 900 KB/s
  * Fine tune NET buffer to improve TCP download/upload performances.
    * Increase the maximum size of the read/write TCP Window to 16KB.
    * Increase size/number of RX/TX buffers. 32 buffers each of 1600B -> ~100KB
    * **Optimized configuration** Download: 2650 KB/s, Upload: 830 KB/s 
    * **Default configuration** Download: 410 KB/s, Write: 250 KB/s
  * Disable Bluetooth Abstraction Layer.
* [mimxrt1170_evk_mimxrt1176_cm7.overlay](bsp/zephyr-app/main/boards/mimxrt1170_evk_mimxrt1176_cm7.overlay)
  * Set a unique MAC address for the 1G Ethernet instance.
  * Configuration if FS is deployed in RAM.
  * Configure 1 UART interfaces as follows:
  
| MicroEJ Port | Zephyr UART | TX Pin | RX Pin |
| ------------ | ----------- | ------ | ------ |
| `uart0`      | `lpuart2`   | J9 P4  | J9 P2  |

#### NXP FRDM-MCXN947 configuration

* [frdm_mcxn947_mcxn947_cpu0.cmake](bsp/zephyr-app/main/boards/frdm_mcxn947_mcxn947_cpu0.cmake)
  * `BOARD_FLASH_RUNNER` is set to `linkserver`, can be set to `jlink` if needed.
* [frdm_mcxn947_mcxn947_cpu0.conf](bsp/zephyr-app/main/boards/frdm_mcxn947_mcxn947_cpu0.conf)
  * `FatFS` File System deployed in the SD Card.
    * Set FS read/write buffer to 8KB (4 worker dedicated to the FS implementation -> 32KB used for read/write buffers).
    * This configuration improve the FS read/write performances.
      * **Optimized configuration** Read: 8250 KB/s, Write: 4000 KB/s 
      * **Default configuration** Read: 3000 KB/s, Write: 1100 KB/s
  * Kernel buffer size where features will be deployed is set to 4KB.
  * Fine tune NET buffer to improve read/write performances.
    * Increase the maximum size of the read/write TCP Window to 4KB.
    * Increase size/number of RX/TX buffers. 24 buffers each of 800B -> ~37KB
    * **Optimized configuration** Download: 3000 KB/s, Upload: 820 KB/s 
    * **Default configuration** Download: 1800 KB/s, Write: 630 KB/s
  * FRDM-MCXN947 does not provide a Random Generator.
  * Disable Bluetooth Abstraction Layer.
  * Disable Serial Abstraction Layer.
* [frdm_mcxn947_mcxn947_cpu0.overlay](bsp/zephyr-app/main/boards/frdm_mcxn947_mcxn947_cpu0.overlay)
  * Set a unique MAC address Ethernet instance.

#### Nordic nRF54L15 DK configuration

* [nrf54l15dk_nrf54l15_cpuapp.cmake](bsp/zephyr-app/main/boards/nrf54l15dk_nrf54l15_cpuapp.cmake)
* [nrf54l15dk_nrf54l15_cpuapp.conf](bsp/zephyr-app/main/boards/nrf54l15dk_nrf54l15_cpuapp.conf)
  * Disable MicroAI Abstraction Layer -> Takes too much RAM.
  * Disable NET/SSL Abstraction Layer -> No Ethernet instance.
  * Disable Security Abstraction Layer -> Conflict with Bluetooth Abstraction Layer.
  * `LittleFS` File System deployed in the internal flash.
    * Increase littleFS buffers size.
    * Set FS read/write buffer to 4KB (4 worker dedicated to the FS implementation -> 16KB used for read/write buffers).
    * Those configurations improve the FS read/write performances.
      * **Optimized configuration** Read: 8000 KB/s, Write: 235 KB/s 
      * **Default configuration** Read: 4200 KB/s, Write: 190 KB/s
  * Kernel buffer size where features will be deployed is set to 4KB.
* [nrf54l15dk_nrf54l15_cpuapp.overlay](bsp/zephyr-app/main/boards/nrf54l15dk_nrf54l15_cpuapp.overlay)
  * Increase storage partitions to 128KB.
  * Configure 2 UART interfaces as follows:

| MicroEJ Port | Zephyr UART | TX Pin | RX Pin |
| ------------ | ----------- | ------ | ------ |
| `uart0`      | `uart22`    | P1.8   | P1.9   |
| `uart1`      | `uart21`    | P1.10  | P1.11  |

#### Generic configuration

Generic configuration that is applied to all the board are located inside [prj.conf](bsp/zephyr-app/main/prj.conf). 

For example, it sets the main stack size to 4KB, the heap memory pool to 2KB and the compiler optimizations to -O2.

### Build and Run the Application

- Run the `app` sample Application [on Device](https://docs.microej.com/en/latest/SDK6UserGuide/runOnDevice.html).
  This automatically builds the VEE Port and uses it.

The build may take several minutes.  
The first time, the VEE Port build requires to download modules that are available on the MicroEJ Central Repository.  
The VEE Port will also build the `bsp` project and link it with MicroEJ VEE and the Application to produce an executable.

Wait for the Application start. It is a simple Hello World printed in the board's serial port.

## Advanced Tips

### BSP Manual Compilation

This section shows how to build the `zephyr-app` project manually without having to rebuild the MicroEJ application.

Note: this can only be done if `buildExecutable` or `runOnDevice` has been called at least once, see [Run on Device](https://docs.microej.com/en/latest/SDK6UserGuide/runOnDevice.html)

To build the `zephyr-app` project, open a
terminal and enter the following command lines:

**On Windows:**

```sh
$ cd "bsp/zephyr-app/vee/scripts"
$ build.bat
```

**On Linux:**

```sh
$ cd "bsp/zephyr-app/vee/scripts"
$ build.sh
```

You can now run your program:

**On Windows:**
```sh
$ cd "bsp/zephyr-app/vee/scripts"
$ run.bat
```

**On Linux:**

```sh
$ cd "bsp/zephyr-app/vee/scripts"
$ run.sh
```

### Native Tests

The `zephyr-app` project contains native test code that doesn't usually require the MicroEJ VEE.

It contains two sets of tests:

- `bsp/zephyr-app/tests/core`: To test the low level MicroEJ CORE implementation.
- `bsp/zephyr-app/tests/llkernel`: To test the low level MicroEJ KF implementation.

These tests can be run as Zephyr applications by running the build and flash commands like the following:

```sh
$ cd bsp/zephyr-app/tests/core
$ west build -b $BOARD
$ west flash
```

### How to configure a new board

If you want to use this VEE Port with a board that is not part of the sample boards:

* Follow [Zephyr board documentation](https://docs.zephyrproject.org/latest/boards/index.html) to set up the board and install the necessary tools to program it.
* Select the architecture that corresponds to your board and update it on [libs.versions.toml](gradle/libs.versions.toml) file. See [MicroEJ supported Architectures](https://developer.microej.com/mej32-supported-embedded-runtime-architectures/).
* Add missing Zephyr modules:
  * Your board could require additional Zephyr modules (e.g. `hal-nxp` for NXP boards).
  * Add the Zephyr modules to [bsp/zephyr-app/west.yml](bsp/zephyr-app/west.yml).
  * Run the `west update` command to fetch the newly added modules.
* Add the board configuration files on [bsp/zephyr-app/main/boards](bsp/zephyr-app/main/boards) folder:
  * Configuration file names must start with the Zephyr Board identifier so that Zephyr takes it into account during the build (e.g. `mimxrt1170_evk_mimxrt1176_cm7` for MIMXRT1170-EVK).
  * **Required file**
    * `<board_name>.cmake`: Set specific BSP configurations such as the flash runner, and extended overlays. 
    It also allows the user to specify specific Abstraction Layers properties that will overwrite Zephyr Abstraction Layer ones (see [Board specific configuration](#board-specific-configuration) section).
    You can start from the example configuration for the MIMXRT1170 found in [mimxrt1170_evk_mimxrt1176_cm7.cmake](bsp/zephyr-app/main/boards/mimxrt1170_evk_mimxrt1176_cm7.cmake) file.    
  * **Optional files**
    * `<board_name>.overlay`: Extends your board device tree.
    * `<board_name>.conf`: Set specific KConfigs.
* Update [set_project_env.sh](bsp/zephyr-app/vee/scripts/set_project_env.sh) or [set_project_env.bat](bsp/zephyr-app/vee/scripts/set_project_env.bat) depending on your OS to set the new `BOARD` environment variable.

### How to Customize the VEE Port

This project relies on the Zephyr Configuration system.
For detailed information, have a look at Zephyr's [Application Development](https://docs.zephyrproject.org/latest/develop/application/index.html#application) guide.

#### Board selection

The board selection is done by setting an environment variable in the provided scripts : ``vee/scripts/set_project_env.bat/sh``.

This allows quick switching between different boards for the same MicroEJ Application, without any modification in the building and flashing scripts.

If you wish to set the BOARD configuration via the west command line,
it will have precedence on the environment variables, see Zephyr [Application Development](https://docs.zephyrproject.org/latest/develop/application/index.html#important-build-system-variables)

#### Board-specific configuration

Once the BOARD is set, the Zephyr Abstraction Layer project will apply several board-specific configurations:

* ``bsp/zephyr-app/main/boards/<board_name>.cmake``
    * This is where you set all the CMake cache variables for your project, for example:
        * `SHIELD` if you have any shield connected to your board.
        * `BOARD_FLASH_RUNNER` and `BOARD_DEBUG_RUNNER` in case you want to change the default flashing tool (jlink, linkserver, OpenOCD).
        * `EXTRA_CONF_FILE` to customize Zephyr, see [device tree overlays](https://docs.zephyrproject.org/latest/develop/application/index.html#important-build-system-variables).
        * `DTC_OVERLAY_FILE` and `EXTRA_DTC_OVERLAY_FILE` to customize the boards [device tree overlays](https://docs.zephyrproject.org/latest/build/dts/howtos.html#set-devicetree-overlays).

* ``bsp/zephyr-app/main/boards/<board_name>.conf``

    * This Zephyr configuration file is used to set specific Kconfig options for your board. See [Kconfig](https://docs.zephyrproject.org/latest/build/kconfig/index.html).

* ``bsp/zephyr-app/main/boards/<board_name>.overlay``

    * This Zephyr overlay is used to extend your board device tree.

#### How to Enable/Disable Each Module

The support of each MicroEJ pack is separated into different modules, which can all be disabled using Zephyr's `Kconfig` system.

Examples: 

* If your VEE Port doesn't need the FS Foundation Library, you can disable `CONFIG_MICROEJ_FS`.
* `CONFIG_MICROEJ_VALIDATION` is only used to validate the core architecture and kernel implementation.

To disable a feature, add the following line in the `bsp/zephyr-app/main/boards/<board_name>.conf` file:

```
CONFIG_MICROEJ_FS=n
```

#### FS configurations

##### File System

To configure the File System, you can set the variable `CONFIG_MICROEJ_FS_BACKEND_FATFS` or `CONFIG_MICROEJ_FS_BACKEND_LITTLEFS `in `bsp/zephyr-app/main/boards/<board_name>.conf`.

It will automatically select the relevant File System Kconfig options in Zephyr.

##### Mount point

To configure the filesystem location, you can set the variable `CONFIG_MICROEJ_FS_MNT_POINT` in `bsp/zephyr-app/main/boards/<board_name>.conf` to four values:

Here are some examples:
- LITTLEFS: "/lfs"
- FLASH: "/NAND:"
- SDMMC: "/SD:"
- RAM: "/RAM:"

If you use FatFS on RAM, you can modify the partition size in `bsp/zephyr-app/main/boards/<board_name>.overlay`.

##### Misc

Furthermore, following KConfig options are available to configure the FS Abstraction Layer:

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_MICROEJ_FS_WORKER_JOB_COUNT` | int | `4` | Number of workers dedicated to the FS in async_worker. |
| `CONFIG_MICROEJ_FS_WAITING_LIST_SIZE` | int | `16` | Size of the waiting list for FS jobs in async_worker. |
| `CONFIG_MICROEJ_FS_WORKER_STACK_SIZE` | int | `4096` | Size of the FS worker stack in bytes. |
| `CONFIG_MICROEJ_FS_WORKER_PRIORITY` | int | `6` | Priority of the FS workers. |
| `CONFIG_MICROEJ_FS_IO_BUFFER_SIZE` | int | `2048` | Size of the IO buffer in bytes used to read/write operations. There is one IO buffer allocated per worker. |
| `CONFIG_MICROEJ_FS_PATH_LENGTH` | int | `256` | Maximum file path length in characters.  |

#### Kernel configuration

The Kernel Zephyr Abstraction Layer allocates a buffer of `CONFIG_MICROEJ_KF_KERNEL_WORKING_BUFFER_SIZE` bytes in RAM.
Features will be allocated in this dedicated buffer.

#### Serial configuration

The Serial Abstraction Layer uses the Zephyr UART Async API. Received bytes are first placed into one or more RX buffers, then copied into a software ring buffer from which the Managed Code thread reads. 

The following KConfig options are available to configure the Serial Abstraction Layer:

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_MICROEJ_SERIAL_NOCACHE_BUFFERS` | bool | `n` | When enabled, RX and TX buffers are placed in non-cached memory (`.nocache` section) via the `__nocache` attribute. Could be required on platforms where the UART controller uses DMA (e.g. i.MXRT1170 CM7). |
| `CONFIG_MICROEJ_SERIAL_ENABLE_PORT_0` | bool | `y` | Enable Serial Port 0 (`uart0`). The port must be aliased as `microej_serial0` in the board device tree overlay. |
| `CONFIG_MICROEJ_SERIAL_ENABLE_PORT_1` | bool | `y` | Enable Serial Port 1 (`uart1`). The port must be aliased as `microej_serial1` in the board device tree overlay. |
| `CONFIG_MICROEJ_SERIAL_READ_TIMEOUT` | int | `2000` | Inactivity period in microseconds after receiving at least a byte which triggers `UART_RX_RDY ` event (received data is ready for processing). `SYS_FOREVER_US` disables timeout. |
| `CONFIG_MICROEJ_SERIAL_RX_BUFFER_NUMBER` | int | `1` | Number of RX buffers per port. Setting this to `2` or more enables double-buffering: while the UART driver fills one buffer, the callback can process the previous one, which reduces the risk of data loss at high baud rates. Use of a DMA can require at least `2` buffers. |
| `CONFIG_MICROEJ_SERIAL_RX_BUFFER_SIZE` | int | `256` | Size in bytes of the RX buffer per port. The UART driver requests a new buffer after filling one of this size. Must be large enough to hold the maximum burst of incoming data between two `UART_RX_BUF_REQUEST` events. |
| `CONFIG_MICROEJ_SERIAL_RX_RING_BUFFER_SIZE` | int | `512` | Size in bytes of the software ring buffer per port. Data is copied from the RX buffers into this ring buffer inside the UART callback. Managed Code reads from it. Must be large enough to hold data accumulated between two Managed Code read operations to avoid overflow. |
| `CONFIG_MICROEJ_SERIAL_TX_BUFFER_SIZE` | int | `256` | Size in bytes of the TX buffer per port. When `CONFIG_MICROEJ_SERIAL_NOCACHE_BUFFERS=n`, this buffer is not used and size could be set to `0` (Managed Code buffer is used). During a serial write, data is copied into this buffer before being handed to `uart_tx()`. Writes larger than this size are split into chunks. |

## MISRA Compliance

This Abstraction Layer has a list of components that are MISRA-compliant (MISRA C:2012) with some noted exceptions.
Below is the list of folders that have been verified:

- `port/core`

It has been verified with Cppcheck v2.13. Here is the list of deviations from MISRA standard:

| Deviation | Category | Justification                                                                                  |
|-----------|----------|------------------------------------------------------------------------------------------------|
| Rule 17.7 | Required | Unused non-void returned type, the use of<br/>returned values of debug traces is not necessary |
| Rule 21.6 | Required | The Standard Library input/output functions shall not be used                                  |

## Troubleshooting

### Mbed TLS configuration issues when Bluetooth is activated at the same time than Security or SSL Abstraction Layers. 

The current configuration does not support enabling SSL or Security and Bluetooth simultaneously due to mbed TLS dependency conflicts.

---

_Markdown_  
_Copyright 2024-2026 MicroEJ Corp. All rights reserved._  
_Use of this source code is governed by a BSD-style license that can be found with this software._ 
