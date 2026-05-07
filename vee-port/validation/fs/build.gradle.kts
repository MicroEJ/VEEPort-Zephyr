/*
 * Kotlin
 *
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */


import com.microej.gradle.plugins.TestMode
import com.microej.gradle.plugins.TestTarget

plugins {
    id("com.microej.gradle.testsuite")
}

microej {
    architectureUsage = System.getProperty("com.microej.architecture.usage") ?: "eval" // or "prod"
    skippedCheckers = "readme,license,changelog"
}

dependencies {
    microejVee(project(":vee-port"))
}

testing {
    suites {
        val test by getting(JvmTestSuite::class) {
            microej.useMicroejTestEngine(this, TestTarget.EMB, TestMode.MAIN)

            dependencies {
                implementation(project())
                implementation(libs.junit)
                implementation(libs.junit.platform)

                implementation(libs.testsuite.fs)
                implementation(libs.testsuite.fs.openjdk)
                implementation(libs.api.edc)
                implementation(libs.api.bon)
                implementation(libs.api.fs)
            }

            targets {
                all {
                    testTask.configure {
                        filter {
                            includeTestsMatching("Test*")
                            excludeTestsMatching("*AllTestClasses")
                            excludeTestsMatching("*Windows*")
                            excludeTestsMatching("*SingleTest*")
                            // do not embed inner classes as test classes
                            excludeTestsMatching("*$*")

                            // Including this test throws a "ClassNotFoundException: ej.lang.ResourceManager"
                            // because resourcemanager-1.0-api not in the classpath used by JUnit to discover tests classes
                            // because it has to be removed from the test runtime classpath (see dependencies above).
                            excludeTestsMatching("TestResources01")
                            // Excluding this test because Zephyr does not support Sync related flag for fs_open
                            excludeTestsMatching("*TestGeneratedOpenSync*")
                            // Excluding this test because Zephyr does not store the date when a file was last modified
                            excludeTestsMatching("*TestFileProperties*")
                            // Excluding this test because Zephyr does not support reading and writing on the same file at the same time
                            excludeTestsMatching("*TestMultipleStreamOnSameFile*")
                            // Excluding this test because Zephyr does not support reading and writing on the same file at the same time
                            excludeTestsMatching("*TestMultipleThreadOnSameFile*")
                            // Excluding this test because the CI doesn't use a persistent filesytem
                            excludeTestsMatching("*TestListAndCompare*")
                            // Exclude this test because it uses a relative path and those aren't supportted when using Zephyr and FatFs
                            excludeTestsMatching("*TestGeneratedLargeFileAvailable*")
                            // Exclude this test due to different behaviour on RAM than other FS backend
                            excludeTestsMatching("*TestGeneratedReadWritePrimitives*")
                            // Exclude this test due to different behaviour on RAM than other FS backend
                            excludeTestsMatching("*TestGeneratedSetLength*")
                            // Excluding this test because it requires 512KB of Javaheap and 15KB of immortal heap.
                            excludeTestsMatching("*TestGeneratedWriteUTF*")
                        }
                    }
                }
            }
        }
    }
}
