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

                implementation(libs.testsuite.net)
                implementation(libs.testsuite.net.openjdk)
                implementation(libs.api.net)

                // Add the following dependency if running the testsuite over Wi-Fi.
                // implementation(libs.net.wifi.testsuite)
            }

            targets {
                all {
                    testTask.configure {
                        filter {
                            includeTestsMatching("Test*")
                            excludeTestsMatching("*AllTestClasses")
                            excludeTestsMatching("*SingleTest*")
                            // do not embed inner classes as test classes
                            excludeTestsMatching("*$*")
                            // do not pass standalone tests, only group tests
                            excludeTestsMatching("com.microej.net.test.standalone*")

                            // Test excluded because it doesn't include the test tag "ASSERTIOn OK="
                            excludeTestsMatching("*BindException.Test")
                            excludeTestsMatching("*DatagramSocket.TestAfterClose*")
                            excludeTestsMatching("*ServerSocket.TestAfterClose*")
                            excludeTestsMatching("*NetworkInterface.Test")
                            excludeTestsMatching("*Socket.TestAfterClose*")
                            excludeTestsMatching("*Socket.TestTcpNoDelay*")

                            // Test excluded because testsuite does not wait for IP assignation
                            excludeTestsMatching("openjdk.test.main.Socket.TestClose")
                            excludeTestsMatching("*TestNetOpenjdkB6469803*")
                            excludeTestsMatching("*TestNetOpenjdkPromiscuous*")
                            excludeTestsMatching("*TestNetOpenjdkB6427403")
                            excludeTestsMatching("*TestNetOpenjdkJoinGroup")
                            excludeTestsMatching("*TestNetOpenjdkLeave")
                            excludeTestsMatching("*TestNetOpenjdkMulticastAddresses")

                            // Test excluded because testsuite does not wait for IP assignation + Zephyr unable to disconnect DatagramSocket
                            excludeTestsMatching("*TestNetOpenjdkSendDatagramToBadAddress*")

                            // Test excluded because the socket option SO_BROADCAST is not available in zephyr v4.1 + Zephyr unable to disconnect DatagramSocket
                            excludeTestsMatching("*TestDatagramSocketAdditional*")

                            // Test excluded because we cannot connect with ipv6
                            excludeTestsMatching("com.microej.net.test.group.dual.TestDualStackSocket")
                            excludeTestsMatching("com.microej.net.test.group.dual.TestDualStackDatagram")
                            excludeTestsMatching("com.microej.net.test.group.dual.TestDualStackServerSocket")

                            // Exclude additional OS specific tests
                            excludeTestsMatching("com.microej.net.test.group.TestNetAdditionalLinux")
                            excludeTestsMatching("com.microej.net.test.group.TestNetAdditionalWindows")

                            // Test excluded because the socket option IP_MULTICAST_LOOP is not available in zephyr v4.1
                            excludeTestsMatching("*TestMulticastSocket*")
                            excludeTestsMatching("*TestNetOpenjdkDefaults*")
                            excludeTestsMatching("*TestNetOpenjdkNormalUsage*")
                            excludeTestsMatching("*TestNetOpenjdkSetLoopbackMode*")

                            // Test excluded because the option SO_LINGER is only there for compatibility reason
                            excludeTestsMatching("*TestSocketOptionsLinger*")

                            // Test excluded because the option SO_OOBINLINE is there for compatibility reason
                            excludeTestsMatching("*TestSocketOptionsOOBInline*")

                            // Test excluded because it run out of memory
                            excludeTestsMatching("*TestNetOpenjdkAsyncClose*")

                            // Test excluded because wrong error message is received
                            excludeTestsMatching("*TestNetOpenjdkSocketClosedException*")

                            // Test excluded because it tries to set back the send buffer to its default value that is 0.
                            // Socket.setSendBufferSize does not support value that is 0 or negative.
                            excludeTestsMatching("*TestSocketOptionsSendRecvBuffer")
                            excludeTestsMatching("*TestSocketOptionsSendRecvBuffer2")
                            excludeTestsMatching("*TestDatagramSocketSetBufferSize*")

                            // Test excluded because server end delay too early
                            excludeTestsMatching("*TestCloseSocket*")

                            // Test PASSED but is unstable and fails sometimes
                            excludeTestsMatching("TestSocketTimeout2*")

                            // Excluding this test because it requires a too big Java and immortal heaps.
                            excludeTestsMatching("*TestNetOpenjdkBrokenPipe*")

                            // No activity on input stream
                            excludeTestsMatching("*TestSocketConcurrent*")
                        }
                    }
                }
            }
        }
    }
}
