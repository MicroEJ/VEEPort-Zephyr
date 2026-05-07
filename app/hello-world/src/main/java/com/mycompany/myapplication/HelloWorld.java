/*
 * Java
 *
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */
package com.mycompany.myapplication;

import java.util.logging.Logger;

public class HelloWorld {
    private static final Logger LOGGER = Logger.getLogger("[Main]");
    public static final String HELLO_WORLD = "Hello World from Zephyr";

    public static void main(String[] args) {
        LOGGER.info(HELLO_WORLD);
    }
}
