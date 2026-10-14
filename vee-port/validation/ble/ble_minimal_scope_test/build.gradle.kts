/*
 * Kotlin
 *
 * Copyright 2024-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
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
}

dependencies {
    implementation(libs.api.edc)
    implementation(libs.api.bluetooth)
    implementation(libs.bluetooth.util)

    microejVee(project(":vee-port"))
}
