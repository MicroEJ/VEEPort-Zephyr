/*
 * Java
 *
 * Copyright 2024-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

package com.example.ble_throughput_server;

import ej.bluetooth.BluetoothAdapter;
import ej.bluetooth.BluetoothConnection;
import ej.bluetooth.util.AdvertisementData;
import ej.bluetooth.util.AttributeNotFoundException;
import ej.bluetooth.listeners.impl.DefaultConnectionListener;
import ej.bluetooth.BluetoothService;
import ej.bluetooth.BluetoothServiceDefinition;
import ej.bluetooth.util.ServiceHelper;

import java.util.Arrays;

public class BLEThroughputApp extends DefaultConnectionListener {

	private final BLEThroughputServer BleThroughputServer;
	private static final String DEVICE_NAME = "BLEThroughputServer";

	static BluetoothConnection connection;

	public BLEThroughputApp(BluetoothService service) {
		this.BleThroughputServer = new BLEThroughputServer(service);
	}

	public void start() throws AttributeNotFoundException {
		BluetoothAdapter.getAdapter().setConnectionListener(this);
		this.BleThroughputServer.start();

		startAdvertising();
	}

	private void startAdvertising() {
		System.out.println("Advertising as \"" + DEVICE_NAME + "\"");
		AdvertisementData data = new AdvertisementData();
		data.setDeviceName(DEVICE_NAME);
		BluetoothAdapter.getAdapter().startAdvertising(data.serialize());
	}

	@Override
	public void onConnected(BluetoothConnection connection) {
		System.out.println("New device connected");
		this.connection = connection;
	}

	@Override
	public void onDisconnected(BluetoothConnection connection) {
		System.out.println("Device disconnected");
		this.BleThroughputServer.stopTest();
		startAdvertising();
	}

	public static BluetoothConnection getConnection() {
		return connection;
	}
}
