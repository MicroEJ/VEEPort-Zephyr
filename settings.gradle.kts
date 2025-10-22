/*
 * Kotlin
 *
 * Copyright 2025 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

rootProject.name = "zephyr"
include("vee-port", "vee-port:front-panel")
include("app")

project(":vee-port:front-panel").projectDir = file("vee-port/extensions/front-panel")

include("vee-port:validation:core")
include("vee-port:validation:event-queue")
include("vee-port:validation:fs")
include("vee-port:validation:microai")
include("vee-port:validation:net")

dependencyResolutionManagement {
    versionCatalogs {
        create("libsOverride") {
            from(files("./bsp/vee/gradle/libs.versions.toml"))
        }
    }
}
