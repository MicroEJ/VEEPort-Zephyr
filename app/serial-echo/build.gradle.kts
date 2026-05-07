/*
 * Kotlin
 *
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

plugins {
    id("com.microej.gradle.application") version libs.versions.microej.sdk
}

microej {
    applicationEntryPoint = "com.mycompany.myapplication.Main"
    skippedCheckers = "readme,changelog,license,nullanalysis"
    architectureUsage = System.getProperty("com.microej.architecture.usage") ?: "eval" // or "prod"
}

dependencies {
    implementation(libs.api.edc)
    implementation(libs.api.bon)
    implementation(libs.api.serial)
    
    implementation(libs.library.logging)
    
    microejVee(project(":vee-port"))
}
