/*
 * Kotlin
 *
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

plugins {
    id("com.microej.gradle.mock-frontpanel")
}

microej {
    skippedCheckers = "changelog,readme,license"
}

dependencies {
    implementation(libs.frontpanel.framework)
}

artifacts {
    add("additionalElements", file("../../../README.md")) {
        classifier = "readme"
    }
    add("additionalElements", file("../../../CHANGELOG.md")) {
        classifier = "changelog"
    }
    add("additionalElements", file("../../../LICENSE.txt")) {
        classifier = "license"
    }
}
