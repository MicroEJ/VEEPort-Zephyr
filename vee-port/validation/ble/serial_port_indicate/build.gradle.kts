/*
 * Kotlin
 *
 * Copyright 2024-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

plugins {
    id("com.microej.gradle.application")
}

group = "com.microej.example.foundation.bluetooth"
version = "1.0.0-RC"

microej {
    applicationEntryPoint = "com.microej.example.bluetooth.peripheral.Main"
    architectureUsage = System.getProperty("com.microej.architecture.usage") ?: "eval" // or "prod"
    skippedCheckers = "readme,changelog,license, nullanalysis"
}

dependencies {
    implementation(libs.api.edc)
    implementation(libs.api.bluetooth)
    implementation(libs.bluetooth.util)

    microejVee(project(":vee-port"))
}

