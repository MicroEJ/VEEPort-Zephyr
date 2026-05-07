/*
 * Kotlin
 *
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
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

    api(libs.api.device)
    api(libs.api.event)
    microejPack(libs.pack.fs)
    microejPack(libs.pack.net)
    microejPack(libs.pack.serial)
    microejPack(libs.api.bluetooth)
    microejPack(libs.api.microai)

    microejFrontPanel(project(":vee-port:front-panel"))

    microejMock(libs.jenkins)
}

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
        bspRootDir + properties.getProperty("bsp.microejapp.relative.dir")
    )
    delete(directories)

    // Execute clean script from the BSP
    val microejScriptDir = bspRootDir + properties.getProperty("bsp.microejscript.relative.dir")
    if(org.gradle.internal.os.OperatingSystem.current().isWindows) {
        val scriptFile = project.file("$microejScriptDir/clean.bat")
        exec {
            executable = scriptFile.absolutePath
        }
    } else { // Linux or MacOS
        val scriptFile = project.file("$microejScriptDir/clean.sh")
        exec {
            executable = "bash"
            args(scriptFile.absolutePath)
        }
    }
}

tasks.named("clean") {
    dependsOn(cleanVee)
}

artifacts {
    add("additionalElements", file("../README.md")) {
        classifier = "readme"
    }
    add("additionalElements", file("../CHANGELOG.md")) {
        classifier = "changelog"
    }
    add("additionalElements", file("../LICENSE.txt")) {
        classifier = "license"
    }
    add("additionalElements", file("../NOTICE.txt")) {
        classifier = "notice"
    }
}
