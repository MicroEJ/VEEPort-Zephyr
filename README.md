![SDK](https://shields.microej.com/endpoint?url=https://repository.microej.com/packages/badges/sdk_6.0.json)
![ARCH](https://shields.microej.com/endpoint?url=https://repository.microej.com/packages/badges/arch_8.6.json)

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

The architecture version is `8.6.0`.

This VEE Port provides the following Foundation Libraries:

| Foundation Library | Version |
|--------------------|---------|
| BON                | 1.5     |
| BLUETOOTH          | 3.0     |
| DEVICE             | 1.4     |
| EVENT              | 3.0     |
| FS                 | 2.1     |
| KF                 | 1.7     |
| MICROAI            | 2.2     |
| NET                | 1.1     |
| RESOURCEMANAGER    | 1.0     |
| CRYPTO             | 2.1     |
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

- `bsp/zephyr-app/zephyr`: Contains the Zephyr module manifest, the Zephyr patches and the board extensions (see [Board extensions](#board-extensions)).

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

Zephyr 4.4 requires Python 3.12 or newer and Zephyr SDK 1.0.x; the script installs the SDK version the Zephyr tree asks for.

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
To change the board, pass its **board name** to Gradle with `-Pveeport.board=<name>`.
Board names are defined by this project in [gradle/boards.gradle.kts](gradle/boards.gradle.kts),
and each one maps to a Zephyr board:

| `-Pveeport.board` | board |
|---|---|
| `mimxrt1170` | NXP MIMXRT1170-EVK (default) |
| `mcxn947` | NXP FRDM-MCXN947 |
| `nrf54l15` | Nordic nRF54L15 DK |

For example, to build and flash the FRDM-MCXN947:

```sh
./gradlew :app:hello-world:runOnDevice -Pveeport.board=mcxn947
```

Passing `-Pveeport.board` on every command gets tedious. To pin a board for your whole checkout,
create a `gradle.properties` file at the root of the project:

```properties
veeport.board=nrf54l15
```

Gradle reads it on every invocation, so this works both from the command line and from an IDE.
A `-Pveeport.board` on the command line still takes precedence.


After a board change, run [bsp/zephyr-app/vee/scripts/clean.sh](bsp/zephyr-app/vee/scripts/clean.sh)
before you build. `west build -p=auto` does not always reconfigure an existing Zephyr build directory
for the new board, and a build directory that keeps the previous board produces the same wrong-board
flash.

Have a look at your board configuration on the VEE Port:

#### NXP MIMXRT1170-EVK configuration

* [zephyr/boards/microej/mimxrt1170_evk](bsp/zephyr-app/zephyr/boards/microej/mimxrt1170_evk) — board extension
  (see [Board extensions](#board-extensions)) tying `NET_L2_ETHERNET` to the 1G Ethernet controller.
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
  * Swaps to the [1G Ethernet instance](https://docs.zephyrproject.org/latest/boards/nxp/mimxrt1170_evk/doc/index.html#enet1g-driver)
    in place of the 10/100 one, with a MAC address derived from the SoC unique ID.
  * Configuration if FS is deployed in RAM.
  * Configure 1 UART interfaces as follows:
  
| MicroEJ Port | Zephyr UART | TX Pin | RX Pin |
| ------------ | ----------- | ------ | ------ |
| `uart0`      | `lpuart2`   | J9 P4  | J9 P2  |

#### NXP FRDM-MCXN947 configuration

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

* [nrf54l15dk_nrf54l15_cpuapp.conf](bsp/zephyr-app/main/boards/nrf54l15dk_nrf54l15_cpuapp.conf)
  * Disable MicroAI Abstraction Layer -> Takes too much RAM.
  * Disable NET/SSL Abstraction Layer -> No Ethernet instance.
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

For example, it sets the main stack size to 8KB, the heap memory pool to 2KB and the compiler optimizations to -O2.

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

To use this VEE Port with a board that is not one of the three samples:

**1. Set up the board.** Follow the [Zephyr board documentation](https://docs.zephyrproject.org/latest/boards/index.html)
to get it supported and its programming tools installed. Note its Zephyr board identifier, and pick the
[MicroEJ Architecture](https://developer.microej.com/mej32-supported-embedded-runtime-architectures/) that
matches its core.

**2. Register the board in the matrix** in [gradle/boards.gradle.kts](gradle/boards.gradle.kts) -- one entry,
three values:

```kotlin
"mcxn947" to mapOf(
    "zephyr" to "frdm_mcxn947/mcxn947/cpu0",
    "arch"   to "com.microej.architecture.CM4.CM4hardfp_GCC48:flopi4G25",
    "group"  to "com.microej.veeport.zephyr_mcxn947_frdm"
)
```

The key is the name you will pass to `-Pveeport.board`. `arch` is the Architecture `<group>:<name>`; its
version is shared by every board and stays in [libs.versions.toml](gradle/libs.versions.toml). `group` is
the group this VEE Port gets published under.

**3. Add the Zephyr modules your board needs** -- `hal_nxp` for NXP boards, `hal_nordic` for Nordic ones --
to [bsp/zephyr-app/west.yml](bsp/zephyr-app/west.yml), then run `west update` to fetch them.

**4. Add the board configuration files** to [bsp/zephyr-app/main/boards](bsp/zephyr-app/main/boards). They
are found by file name, and that name is the Zephyr board identifier with every `/` replaced by `_`, so
`frdm_mcxn947/mcxn947/cpu0` becomes `frdm_mcxn947_mcxn947_cpu0`:

| file | required | what it does |
|---|---|---|
| `<zephyr_board>.conf` | no | board-specific Kconfig options |
| `<zephyr_board>.overlay` | no | device tree overlay |

If the board needs a Kconfig *default* rather than a fixed value — one conditional on the hardware
actually present — add a board extension under [bsp/zephyr-app/zephyr/boards](bsp/zephyr-app/zephyr/boards).
See [Board extensions](#board-extensions).

### Board extensions

A `<board_target>.conf` can only *assign* a Kconfig symbol. Some board settings need to be a **default
conditional on the hardware present**, which only a board `Kconfig.defconfig` can express. For those, this
VEE Port extends the upstream Zephyr board with a `board.yml` declaring `extend: <upstream board>`, rather
than redefining the board — see Zephyr's
[Board Porting Guide](https://docs.zephyrproject.org/latest/hardware/porting/board_porting.html#extend-board).
Extensions live under [bsp/zephyr-app/zephyr/boards](bsp/zephyr-app/zephyr/boards) and are found through the `board_root`
setting in [bsp/zephyr-app/zephyr/module.yml](bsp/zephyr-app/zephyr/module.yml), so no extra `west` argument
is needed and the board target is unchanged.

[zephyr/boards/microej/mimxrt1170_evk](bsp/zephyr-app/zephyr/boards/microej/mimxrt1170_evk) is the reference example, and
is deliberately just two files — `board.yml` plus a `Kconfig.defconfig` that ties `NET_L2_ETHERNET` to the
1G Ethernet controller the board devicetree selects. Board target, SoC selection, `_defconfig`, boot header,
flash runners and revisions all stay upstream, and the devicetree changes stay in
[main/boards](bsp/zephyr-app/main/boards) with the rest of the board configuration.

If you do put devicetree in an extension directory, note that it contributes only **revision-suffixed**
overlays — a non-revision `<board_target>.overlay` there is silently ignored.

### How to Customize the VEE Port

This project relies on the Zephyr Configuration system.
For detailed information, have a look at Zephyr's [Application Development](https://docs.zephyrproject.org/latest/develop/application/index.html#application) guide.

#### Board selection

You pick the board once, with the `veeport.board` Gradle property -- see [Board Setup](#board-setup) for
the board names.

Gradle looks that name up in [gradle/boards.gradle.kts](gradle/boards.gradle.kts) and gets three things
out of it: the MicroEJ Architecture to build against, the group the VEE Port is published under, and the
Zephyr board name -- `mcxn947` gives `frdm_mcxn947/mcxn947/cpu0`.

That last one has to reach `west`, which knows nothing about Gradle. So Gradle writes it to a file:

```
-Pveeport.board=mcxn947  ->  .board.env  ->  $BOARD  ->  west build
```

`build.sh` sources `set_project_env.sh`, which reads `bsp/zephyr-app/vee/scripts/.board.env` and exports
`BOARD`. `west build` picks `BOARD` up from there. Windows works the same way, with `build.bat` and
`set_project_env.bat`.

This is why running `./build.sh` by hand still builds for the board you chose: `.board.env` is left behind
by the last Gradle run. If you have never run Gradle, the file does not exist and the scripts fall back to
the board hard-coded in `set_project_env` (`mimxrt1170_evk/mimxrt1176/cm7`).

Give it a board name that is not in the matrix and the build stops and tells you which names it knows.

If you need to bypass all of this, `west build -b <zephyr board>` wins over `BOARD`, see Zephyr's
[Application Development](https://docs.zephyrproject.org/latest/develop/application/index.html#important-build-system-variables)
guide.

#### Board-specific configuration

Once the BOARD is set, the Zephyr Abstraction Layer project will apply several board-specific configurations:

* ``bsp/zephyr-app/main/boards/<board_target>.conf``

    * This Zephyr configuration file is used to set specific Kconfig options for your board. See [Kconfig](https://docs.zephyrproject.org/latest/build/kconfig/index.html).

* ``bsp/zephyr-app/main/boards/<board_target>.overlay``

    * This Zephyr overlay is used to extend your board device tree.

A `_<revision>` suffix on either file restricts it to a single board revision.

* ``bsp/zephyr-app/main/boards/<board_target>.ld``

    * Optional. Extra linker sections for this board, passed to Zephyr's `zephyr_linker_sources(SECTIONS ...)`.
      Use it to place a buffer in a specific memory region, for instance a framebuffer in SDRAM.
      Unlike the two files above, this one does not support a `_<revision>` suffix.

Settings that are neither Kconfig nor devicetree go through the `west` command line, via
``vee/scripts/set_project_env.sh`` (``set_project_env.bat`` on Windows): use `WEST_BUILD_ARGS` for build
options such as `SHIELD` or `EXTRA_DTC_OVERLAY_FILE`, and `WEST_FLASH_ARGS` to flash or debug with
something other than the board's default runner.

On Linux and macOS, in ``set_project_env.sh``:

```sh
export WEST_FLASH_ARGS="--runner jlink"
```

On Windows, in ``set_project_env.bat`` — without quotes, otherwise the variable expansion fails:

```bat
SET WEST_FLASH_ARGS=--runner jlink
```

#### How to Enable/Disable Each Module

The support of each MicroEJ pack is separated into different modules, which can all be disabled using Zephyr's `Kconfig` system.

Examples: 

* If your VEE Port doesn't need the FS Foundation Library, you can disable `CONFIG_MICROEJ_FS`.
* `CONFIG_MICROEJ_VALIDATION` is only used to validate the core architecture and kernel implementation.

To disable a feature, add the following line in the `bsp/zephyr-app/main/boards/<board_target>.conf` file:

```
CONFIG_MICROEJ_FS=n
```

#### FS configurations

##### File System

To configure the File System, you can set the variable `CONFIG_MICROEJ_FS_BACKEND_FATFS` or `CONFIG_MICROEJ_FS_BACKEND_LITTLEFS `in `bsp/zephyr-app/main/boards/<board_target>.conf`.

It will automatically select the relevant File System Kconfig options in Zephyr.

##### Mount point

To configure the filesystem location, you can set the variable `CONFIG_MICROEJ_FS_MNT_POINT` in `bsp/zephyr-app/main/boards/<board_target>.conf` to four values:

Here are some examples:
- LITTLEFS: "/lfs"
- FLASH: "/NAND:"
- SDMMC: "/SD:"
- RAM: "/RAM:"

If you use FatFS on RAM, you can modify the partition size in `bsp/zephyr-app/main/boards/<board_target>.overlay`.

##### Misc

Furthermore, following KConfig options are available to configure the FS Abstraction Layer:

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_MICROEJ_FS_WORKER_JOB_COUNT` | int | `4` | Number of workers dedicated to the FS in async_worker. |
| `CONFIG_MICROEJ_FS_WAITING_LIST_SIZE` | int | `16` | Size of the waiting list for FS jobs in async_worker. |
| `CONFIG_MICROEJ_FS_WORKER_STACK_SIZE` | int | `4096` | Size of the FS worker stack in bytes. |
| `CONFIG_MICROEJ_FS_WORKER_PRIORITY` | int | `2` | Priority of the FS workers. |
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

#### VEE Port logging configuration

The Abstraction Layers log through the Zephyr logging subsystem. Logging requires `CONFIG_LOG=y`. When this option is off, every log call compiles to nothing.
Each Abstraction Layer has its own log level, set with one `CONFIG_MICROEJ_<LAYER>_LOG_LEVEL_<LEVEL>=y` option. The default level is `ERR`:
- `<LAYER>` is one of `BLUETOOTH`, `CRYPTO`, `EVENT_QUEUE`, `FS`, `KF`, `MICROAI`, `NET`, `SERIAL` or `SSL`.
- `<LEVEL>` is one of `OFF`, `ERR`, `WRN`, `INF`, `DBG` or `DEFAULT`. `DEFAULT` uses the Zephyr global level, `CONFIG_LOG_DEFAULT_LEVEL`.

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

### Bluetooth Disabled When SSL Is Enabled

The current configuration does not support enabling the SSL and Bluetooth Abstraction Layers simultaneously, due to Mbed TLS dependency conflicts.
When `CONFIG_MICROEJ_SSL` is enabled, the Bluetooth Abstraction Layer is not built.

---

_Markdown_  
_Copyright 2024-2026 MicroEJ Corp. All rights reserved._  
_Use of this source code is governed by a BSD-style license that can be found with this software._  
_Build: 7E4D1F7C_ 
