/*
 * Java
 *
 * Copyright 2018-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */
package com.microej.example.bluetooth.peripheral;

import java.util.Arrays;

import ej.bluetooth.BluetoothAdapter;
import ej.bluetooth.BluetoothConnection;
import ej.bluetooth.BluetoothService;
import ej.bluetooth.listeners.impl.DefaultConnectionListener;
import ej.bluetooth.util.AdvertisementData;

/**
 * Manages the Bluetooth connection.
 */
public class PeripheralConnectionManager extends DefaultConnectionListener {

	private static final String DEVICE_NAME = "Serial_port_notify";

	private final SerialPortServer serialPortServer;

	/**
	 * Creates a peripheral connection manager.
	 *
	 * @param serialPortService
	 *            the local serial port service.
	 */
	public PeripheralConnectionManager(BluetoothService serialPortService) {
		this.serialPortServer = createSerialPortServer(serialPortService);
	}

	/**
	 * Starts this peripheral connection manager.
	 */
	public void start() {
		BluetoothAdapter.getAdapter().setConnectionListener(this);

		this.serialPortServer.start();

		startAdvertising();

		displayLogs();
	}

	@Override
	public void onConnected(BluetoothConnection connection) {
		System.out.println("Connected");
	}

	@Override
	public void onDisconnected(BluetoothConnection connection) {
		System.out.println("Disconnected");

		startAdvertising();
	}

	private void startAdvertising() {
		System.out.println("Start advertising");
		AdvertisementData data = new AdvertisementData();
		data.setDeviceName(DEVICE_NAME);
		BluetoothAdapter.getAdapter().startAdvertising(data.serialize());
	}

	private static SerialPortServer createSerialPortServer(BluetoothService service) {
		return new SerialPortServer(service) {
			@Override
			protected void onDataSent(BluetoothConnection connection, boolean success) {
				System.out.println("Data sent");
			}

			@Override
			protected void onDataReceived(BluetoothConnection connection, byte[] data) {
				System.out.println("Data received: " + Arrays.toString(data));
				sendData(connection, data); // echo data
			}
		};
	}

	public void displayLogs() {
		System.out.println(
				"*****************************************************************************************************");
		System.out.println(
				"*                       MicroEJ Serial_port_notify Validation 1.0.0                                 *");
		System.out.println(
				"*****************************************************************************************************");
		System.out.println(
				"* Copyright 2013-2025 MicroEJ Corp.                                                                 *");
		System.out.println("* Use of this source code is governed by a BSD-style license that can be found with this software. *");
		System.out.println(
				"*****************************************************************************************************");
		System.out.println();
		System.out
				.println("                                                UUID                            Properties");
		System.out.println(
				"    Service                |     0783b03e-8535-b5a0-7140-a304d2495cb7       |                        ");
		System.out.println(
				"        Characteristic     |     0783b03e-8535-b5a0-7140-a304d2495cb8       |   NOTIFY               ");
		System.out.println(
				"            Descriptor     |     2901                                       |   READ                 ");
		System.out.println(
				"            Descriptor     |     2902                                       |   READ | WRITE         ");
		System.out.println(
				"        Characteristic     |     0783b03e-8535-b5a0-7140-a304d2495cba       |   WRITE                ");
		System.out.println(
				"            Descriptor     |     2901                                       |   READ                 ");
		System.out.println();
		System.out.println("Check the success of the app by following the flow below :");
		System.out.println("1. Launch a BLE application on your mobile (nRF Connect recommended), start the scanning.");
		System.out.println("   You can filter the devices by name Ser.. to find ours.");
		System.out.println("2. Once found, connect to the device.");
		System.out.println("3. Discover the characteristics' names by reading their characteristic user description");
		System.out.println("   TX for the first characteristic and RX for the second one should appear.");
		System.out.println("4. Subscribe to the TX characteristic by clicking on the triple arrows");
		System.out.println("5. Write any value in the RX characteristic with the up-arrow button");
		System.out.println("6. The application automatically indicates in the TX characteristic what has been ");
		System.out.println("   written in the RX one");
		System.out.println("7. Check that you received the data sent in the 'Value' field of the TX characteristic");
	}
}
