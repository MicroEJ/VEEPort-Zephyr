/*
 * Kotlin
 *
 * Copyright 2025 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

import java.util.*

plugins {
    id("com.microej.gradle.veeport") version libs.versions.microej.sdk
}

microej {
    skippedCheckers = "readme,changelog,license"
}

dependencies {
    microejArchitecture(libs.architecture)


    /* Packs are fetched from bsp/vee/gradle/libs.versions.toml */
    microejPack(libsOverride.pack.device)
    microejPack(libsOverride.pack.event)
    microejPack(libsOverride.pack.fs)
    microejPack(libsOverride.pack.net)
    microejPack(libsOverride.api.microai)

    microejFrontPanel(project(":vee-port:front-panel"))

    microejMock(libs.jenkins)
}

tasks.register<Copy>("copyBspScripts") {
    from("bsp")
    into("../bsp/vee")
}

tasks.getByName("buildVeePort").dependsOn("copyBspScripts")
tasks.getByName("buildVeePortConfiguration").dependsOn("copyBspScripts")

val cleanVee by tasks.registering(Delete::class) {
    // Load configuration file
    val configurationFile = project.layout.projectDirectory.file("configuration.properties").asFile
    val properties = Properties().apply {
        configurationFile.inputStream().use { fis ->
            load(fis)
        }
    }

    // Get BSP root dir
    val bspRootDir = properties.getProperty("bsp.root.dir").replace("\${project.parent.dir}", "..") + "/"

    // Delete inc/, lib/ and boards/ files
    val directories = listOf(
        bspRootDir + properties.getProperty("bsp.microejinc.relative.dir"),
        bspRootDir + properties.getProperty("bsp.microejlib.relative.dir"),
        bspRootDir + properties.getProperty("bsp.microejapp.relative.dir"),
        bspRootDir + "vee/boards/"
    )
    delete(directories)

    // Execute clean script from the BSP
    val microejScriptDir = bspRootDir + properties.getProperty("bsp.microejscript.relative.dir")
    if(org.gradle.internal.os.OperatingSystem.current().isWindows) {
        exec {
            executable = "$microejScriptDir/clean.bat"
        }
    } else { // Linux or MacOS
        exec {
            executable = "bash"
            args("$microejScriptDir/clean.sh")
        }
    }
}

tasks.named("clean") {
    dependsOn(cleanVee)
}
