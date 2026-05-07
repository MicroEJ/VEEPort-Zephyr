/*
 * Java
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */
package com.example.ble_minimal_scope_test;

import ej.bluetooth.*;
import ej.bluetooth.util.AdvertisementData;
import ej.bluetooth.listeners.impl.DefaultConnectionListener;

/**
 * BLE minimal scope application This app advertises its device name "minimal_scope_test" and is connectable
 */
public class Main extends DefaultConnectionListener {

	private static final String DEVICE_NAME = "minimal_scope_test";

	public static void main(String[] args) {
		System.out.println("Hello World!"); //$NON-NLS-1$
		// Enable adapter
		BluetoothAdapter adapter = BluetoothAdapter.getAdapter();
		adapter.enable();

		System.out.println("Start advertising");
		AdvertisementData data = new AdvertisementData();
		data.setDeviceName(DEVICE_NAME);
		BluetoothAdapter.getAdapter().startAdvertising(data.serialize());

		Main obj = new Main();
		obj.start();

		System.out.println(
				"*****************************************************************************************************");
		System.out.println(
				"*                           MicroEJ ble_minimal_scope_test Validation 1.0.0                         *");
		System.out.println(
				"*****************************************************************************************************");
		System.out.println(
				"* Copyright 2013-2025 MicroEJ Corp.                                                                 *");
		System.out.println(
				"* Use of this source code is governed by a BSD-style license that can be found with this software.  *");
		System.out.println(
				"*****************************************************************************************************");
		System.out.println();
		System.out.println("Advertising name = minimal_scope_test");
		System.out.println("Advertising interval = 100ms");
		System.out.println();
		System.out.println(
				"Launch a BLE application on your mobile (nRF Connect, ST BLE Toolbox...), start the scanning.");
		System.out.println("You can filter the devices by name minim... to find ours.");
		System.out.println("Check that our device correctly advertises its name with an interval of 100ms.");
	}

	public void start() {
		BluetoothAdapter.getAdapter().setConnectionListener(this);
	}

	@Override
	public void onConnected(BluetoothConnection connection) {
		System.out.println("Connected");
	}

	@Override
	public void onDisconnected(BluetoothConnection connection) {
		System.out.println("Disconnected");
	}
}