/*
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

package com.mycompany.myapplication;

import ej.serial.SerialConnection;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintStream;

import java.util.logging.Logger;

public class Main {
	private static final Logger LOGGER = Logger.getLogger("[Main]");
	
	private static final String PORT = "uart0";
	private static final int BAUDRATE = 115_200;
	private static final int DATABITS = SerialConnection.DATABITS_8;
	private static final int PARITY = SerialConnection.PARITY_NONE;
	private static final int STOPBITS = SerialConnection.STOPBITS_1;

	public static void main(String[] args) throws IOException {
		try (SerialConnection conn = new SerialConnection(PORT)) {
			LOGGER.info("Serial connection opened");
			conn.configure(BAUDRATE, DATABITS, PARITY, STOPBITS);
			LOGGER.info("Serial connection configured");

			OutputStream os = conn.getOutputStream();
			InputStream is = conn.getInputStream();

			PrintStream ps = new PrintStream(os, true);

			ps.print("Serial Sample\r\n"
					+ "This serial port echoes everything it receives\r\n");

			LOGGER.info("Type characters in your terminal:");

			while (true) {
				int b = is.read();
				if (b < 0) {
					break;
				}
				os.write(b);
			}
		}
	}
}
