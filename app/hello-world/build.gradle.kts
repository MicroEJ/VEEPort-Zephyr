/*
 * Kotlin
 *
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

plugins {
    id("com.microej.gradle.application") version libs.versions.microej.sdk
}

microej {
    skippedCheckers = "readme,changelog,license,nullanalysis"
}

microej {
    applicationEntryPoint = "com.mycompany.myapplication.HelloWorld"
    architectureUsage = System.getProperty("com.microej.architecture.usage") ?: "eval" // or "prod"
    produceExecutableDuringBuild()
}

dependencies {
    implementation(libs.api.edc)
    
    implementation(libs.library.logging)
    
    microejVee(project(":vee-port"))
}
