/*
 * Kotlin
 *
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

import java.util.*
import javax.xml.parsers.DocumentBuilderFactory
import javax.xml.transform.TransformerFactory
import javax.xml.transform.dom.DOMSource
import javax.xml.transform.stream.StreamResult
import java.io.FileOutputStream

plugins {
    id("com.microej.gradle.veeport") version libs.versions.microej.sdk
}

microej {
    skippedCheckers = "readme,changelog,license"
    additionalFilesDir.set(rootProject.layout.projectDirectory)
}

// Board is selected via the matrix in gradle/boards.gradle.kts (-Pveeport.board=<board name>).
@Suppress("UNCHECKED_CAST")
val boardSpec = rootProject.extra["boardSpec"] as Map<String, String>

dependencies {
    // Architecture comes from the board matrix (<group>:<name>), still overridable per build via
    // -Dveeport.architecture=<group>:<name>. The version stays single-sourced in libs.versions.toml.
    val architecture = System.getProperty("veeport.architecture") ?: boardSpec["arch"]!!
    microejArchitecture("$architecture:${libs.versions.architecture.get()}")

    api(libs.api.device)
    api(libs.api.event)
    microejPack(libs.pack.fs)
    microejPack(libs.pack.net)
    microejPack(libs.pack.serial)
    microejPack(libs.api.bluetooth)
    microejPack(libs.api.microai)
    microejPack(libs.api.crypto)

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

tasks.register("updateIvyDescriptor") {
    doLast {
        val ivyFile = layout.buildDirectory.file("ivy.xml").get().asFile.absolutePath

        val factory = DocumentBuilderFactory.newInstance()
        val builder = factory.newDocumentBuilder()
        val document = builder.parse(ivyFile)
        val publicationsElements = document.getElementsByTagName("publications")
        if (publicationsElements.length == 1) {
            val publications = publicationsElements.item(0)
            val newPublication = document.createElement("artifact")
            newPublication.setAttribute("name", "vee-port")
            newPublication.setAttribute("m:classifier", "notice")
            newPublication.setAttribute("ext", "txt")
            newPublication.setAttribute("type", "text")
            newPublication.setAttribute("conf", "dist")
            publications.appendChild(newPublication)

            val transformerFactory = TransformerFactory.newInstance()
            val transformer = transformerFactory.newTransformer()
            val source = DOMSource(document)
            val result = StreamResult(FileOutputStream(ivyFile))
            transformer.transform(source, result)
        }
    }
}

tasks.getByName("generateIvyDescriptor").finalizedBy("updateIvyDescriptor")

artifacts {
    add("additionalElements", file("../NOTICE.txt")) {
        classifier = "notice"
    }
}
