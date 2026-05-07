/*
 * Kotlin
 *
 * Copyright 2024-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

plugins {
    id("com.microej.gradle.application")
}

microej {
    skippedCheckers = "readme,changelog,license,nullanalysis"
}

microej {
    applicationEntryPoint = "com.example.ble_minimal_scope_test.Main"
    architectureUsage = System.getProperty("com.microej.architecture.usage") ?: "eval" // or "prod"
    produceExecutableDuringBuild()
}

dependencies {
    implementation(libs.api.edc)
    implementation(libs.api.bluetooth)
    implementation("ej.library.iot:bluetooth-util:2.0.0")

    microejVee(project(":vee-port"))
}