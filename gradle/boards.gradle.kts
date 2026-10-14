/*
 * Kotlin
 *
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

// Board configuration matrix: the single source of truth mapping a board name to the full
// Zephyr board identifier, the MicroEJ architecture (<group>:<name>) and the VEE Port publication group.
//
// Select a board with -Pveeport.board=<board name> (or -Dveeport.board=...); defaults below.
// Consumed by:
//   - vee-port/build.gradle.kts                      -> architecture + publication group
//   - vee-port/validation/crypto/build.gradle.kts    -> per-board test policy (via the board name)
//   - the generated .board.env below                 -> BOARD for the west/BSP build (build.sh)
//
// The architecture *version* stays single-sourced in gradle/libs.versions.toml; the matrix supplies
// only <group>:<name>, joined with the catalog version by vee-port/build.gradle.kts.
val boardMatrix = mapOf(
    "nrf54l15" to mapOf(
        "zephyr" to "nrf54l15dk/nrf54l15/cpuapp",
        "arch" to "com.microej.architecture.CM4.CM4hardfp_GCC48:flopi4G25",
        "group" to "com.microej.veeport.zephyr_nrf54l15_dk"
    ),
    "mimxrt1170" to mapOf(
        "zephyr" to "mimxrt1170_evk/mimxrt1176/cm7",
        "arch" to "com.microej.architecture.CM7.CM7hardfp_GCC48:flopi7G26",
        "group" to "com.microej.veeport.zephyr_mimxrt1170_evkb"
    ),
    "mcxn947" to mapOf(
        "zephyr" to "frdm_mcxn947/mcxn947/cpu0",
        "arch" to "com.microej.architecture.CM4.CM4hardfp_GCC48:flopi4G25",
        "group" to "com.microej.veeport.zephyr_mcxn947_frdm"
    )
)

// Default board; keep consistent with the fallback in bsp/zephyr-app/vee/scripts/set_project_env.sh.
val selectedBoard: String = (findProperty("veeport.board") as String?)
    ?: System.getProperty("veeport.board")
    ?: "mimxrt1170"

val boardSpec = boardMatrix[selectedBoard]
    ?: error("Unknown board '$selectedBoard'. Known boards: ${boardMatrix.keys.joinToString(", ")}")

// Expose the resolved board name and its spec to every project.
extra["board"] = selectedBoard
extra["boardSpec"] = boardSpec

// Hand the full Zephyr board name to the west/BSP build: build.sh / build.bat source
// set_project_env.sh / .bat, which read this generated file (BOARD is thus a Gradle output, never a CI
// input). Written as a single bare value (no shell syntax) so both the POSIX and Windows scripts can
// read it natively.
file("bsp/zephyr-app/vee/scripts/.board.env").writeText("${boardSpec["zephyr"]}\n")
