/*
 * Kotlin
 *
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

plugins {
    id("com.microej.gradle.mock-frontpanel")
}

microej {
    skippedCheckers = "changelog,readme,license"
    additionalFilesDir.set(rootProject.layout.projectDirectory)
}

dependencies {
    implementation(libs.frontpanel.framework)
}
