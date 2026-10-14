/*
 * Kotlin
 *
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 *
 * Build: 7E4D1F7C
 */
import com.microej.gradle.plugins.TestMode
import com.microej.gradle.plugins.TestTarget

plugins {
    id("com.microej.gradle.testsuite")
}

microej {
    architectureUsage = System.getProperty("com.microej.architecture.usage") ?: "eval" // or "prod"
    skippedCheckers = "changelog,readme,license"
}

// Board is selected via the matrix in gradle/boards.gradle.kts (-Pveeport.board=<board name>).
// The per-board testsuite option overlay (boards/microej-testsuite-<board>.properties) is applied
// generically to every test project by the root build.gradle.kts; this project only needs `board`
// for the board-specific test exclusion below.
val board = rootProject.extra["board"] as String

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

                implementation(libs.api.edc)
                implementation(libs.api.bon)
                implementation(libs.testsuite.crypto)
                implementation(libs.api.crypto)
            }

            targets {
                all {
                    testTask.configure {
                        filter {
                            excludeTestsMatching("*AllTestClasses")
                            excludeTestsMatching("*SingleTest*")
                            // do not embed inner classes as test classes
                            excludeTestsMatching("*$*")

                            // AES Key Wrap is Oberon/CRACEN-only (stubbed on the generic port, gated by MICROEJ_CRYPTO_NRF54L15_KMU)
                            excludeTestsMatching("*AESKeyWrapTest*")

                            // External KeyStore: requires SIM/EMB KeyStore provisioning alignment
                            excludeTestsMatching("*ExternalKeyStoreTest*")

                            // Mbed TLS 4 / TF-PSA-Crypto 1.x removed DES and 3DES: DESede ciphers and the
                            // PKCS#12 PBE scheme built on them (PBEWithSHA1AndDESede) are not available.
                            excludeTestsMatching("*DESedeCipherTest*")
                            excludeTestsMatching("*PKCS12PBECipherTest*")

                            // Mbed TLS 4 / TF-PSA-Crypto 1.x also removed the secp192r1 and secp224r1 curves.
                            // The MicroEJ test engine runs a class as one unit, so the P-192/P-224 methods of
                            // CertificateDataTest (testSubject, testIssuer), CertificateTest
                            // (verifyCertificateWith*Key_EC_P192/P224) and SignatureTest (*_key_192/_key_224)
                            // cannot be excluded individually and are the only expected errors of those
                            // classes. See the release notes.
                            excludeTestsMatching("*CertificateDataTest*")
                            excludeTestsMatching("*CertificateTest*")
                            excludeTestsMatching("*SignatureTest*")

                            // CertPathValidatorTest never sets the application time (ej.bon.Util.setCurrentTimeMillis),
                            // so on a board without an RTC the clock reads 1970 and the validator's validity-period
                            // rule (notBefore/notAfter) rejects every positive
                            // case; its certificates are notBefore 2024-06-07.
                            excludeTestsMatching("*CertPathValidatorTest*")
                        }
                    }
                }
            }
        }
    }
}
