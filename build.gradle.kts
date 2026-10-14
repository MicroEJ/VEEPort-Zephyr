/*
 * Kotlin
 *
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */

// Resolve the selected board from the matrix and expose it (rootProject.extra["board"]/["boardSpec"]).
// Also writes bsp/zephyr-app/vee/scripts/.board.env so the west/BSP build gets the Zephyr board.
apply(from = "gradle/boards.gradle.kts")

@Suppress("UNCHECKED_CAST")
val boardSpec = rootProject.extra["boardSpec"] as Map<String, String>

allprojects {
    // Publication group is board-specific (single source: the matrix in gradle/boards.gradle.kts), so
    // parallel per-board builds publish distinct coordinates and never collide. Only vee-port and its
    // front-panel actually publish; other projects inherit it harmlessly (they are never published).
    group = boardSpec["group"]!!
    version = "4.0.0"

    // The selected board reaches the native (west) build via .board.env, which Gradle cannot see as an
    // input. Declare it as an input on the tasks that run west so switching -Pveeport.board invalidates
    // them (incl. same-architecture switches, e.g. nrf54l15 <-> mcxn947) instead of reusing stale output.
    tasks.matching { it.name == "buildExecutable" || it.name == "runOnDevice" }.configureEach {
        inputs.property("veeport.board", rootProject.extra["board"])
    }
    val board = rootProject.extra["board"] as String
    tasks.withType(Test::class.java).configureEach {
        inputs.property("veeport.board", board)

        // Per-board testsuite option overlay, shared by every test project so none needs to
        // repeat this: a project's boards/microej-testsuite-<board>.properties (when present) is
        // layered over its microej-testsuite-common.properties by injecting each entry as a
        // microej.testsuite.properties.* system property the MicroEJ test engine reads. Kept
        // outside validation/ so the engine never auto-merges every board's file. Declared only
        // when the file exists, so a project without an overlay registers nothing.
        val overlay = project.file("boards/microej-testsuite-$board.properties")
        if (overlay.exists()) {
            // The systemProperty calls below happen in doFirst, i.e. at execution time, too late for
            // Gradle to fingerprint them. Declaring the file itself is what makes an overlay edit
            // re-run the suite; drop this line and edits are silently treated as up-to-date.
            inputs.file(overlay).withPropertyName("testsuiteOverlay")
            doFirst {
                val overlayProps = java.util.Properties()
                overlay.inputStream().use { overlayProps.load(it) }
                // These override microej-testsuite-common.properties key by key: the engine's
                // MicroejTestEngine.completePropertiesWithSystemProperties() loads that file into a
                // Properties map first, then puts every microej.testsuite.properties.* system property
                // into the same map, so whatever the overlay sets is written last and wins.
                for (key in overlayProps.stringPropertyNames()) {
                    systemProperty("microej.testsuite.properties.$key", overlayProps.getProperty(key))
                }
            }
        }
    }
}
