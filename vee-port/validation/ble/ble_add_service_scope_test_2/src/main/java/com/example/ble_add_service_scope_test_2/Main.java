/*
 * Java
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */
package com.example.ble_add_service_scope_test_2;

import ej.bluetooth.*;
import ej.bluetooth.listeners.impl.DefaultConnectionListener;
import ej.bluetooth.util.AdvertisementData;
import ej.bluetooth.util.DescriptorHelper;
import ej.bluetooth.util.ServiceHelper;
import ej.bluetooth.util.services.sps.SerialPortServer;

/**
 * BLE add service scope 2 This app acts as a peripheral role and provides 4 services with 2 characteristics each, it's
 * another app to test the add service scope.
 */
public class Main extends DefaultConnectionListener {

	private static final String DEVICE_NAME = "add_service_scope_2";

	public static final BluetoothUuid SERVICE1_UUID = new BluetoothUuid("0783b03e-8535-b5a0-7140-a304d2495cb7");
	public static final BluetoothUuid SVC1_CHAR1_UUID = new BluetoothUuid("0783b03e-8535-b5a0-7140-a304d2495cb8");
	public static final BluetoothUuid SVC1_CHAR2_UUID = new BluetoothUuid("0783b03e-8535-b5a0-7140-a304d2495cba");

	public static final BluetoothUuid SERVICE2_UUID = new BluetoothUuid("d58c8541-0113-4e37-b3e3-d3229512694b");
	public static final BluetoothUuid SVC2_CHAR1_UUID = new BluetoothUuid("d58c8541-0113-4e37-b3e3-d3229512694c");
	public static final BluetoothUuid SVC2_CHAR2_UUID = new BluetoothUuid("d58c8541-0113-4e37-b3e3-d3229512694d");

	public static final BluetoothUuid SERVICE3_UUID = new BluetoothUuid("6675126c-94ba-46dc-afb6-7bb3db955be3");
	public static final BluetoothUuid SVC3_CHAR1_UUID = new BluetoothUuid("6675126c-94ba-46dc-afb6-7bb3db955be4");
	public static final BluetoothUuid SVC3_CHAR2_UUID = new BluetoothUuid("6675126c-94ba-46dc-afb6-7bb3db955be5");

	public static final BluetoothUuid SERVICE4_UUID = new BluetoothUuid("8dc871e7-671a-48fa-803a-962d9bb39af1");
	public static final BluetoothUuid SVC4_CHAR1_UUID = new BluetoothUuid("8dc871e7-671a-48fa-803a-962d9bb39af2");
	public static final BluetoothUuid SVC4_CHAR2_UUID = new BluetoothUuid("8dc871e7-671a-48fa-803a-962d9bb39af3");

	public static void main(String[] args) {
		System.out.println("Hello World!");
		Main m = new Main();
		m.launch();
	}

	@Override
	public void onConnected(BluetoothConnection connection) {
		System.out.println("Connected");
	}

	@Override
	public void onDisconnected(BluetoothConnection connection) {
		System.out.println("Disconnected");
	}

	public void launch() {
		// Enable adapter
		BluetoothAdapter adapter = BluetoothAdapter.getAdapter();
		adapter.enable();

		BluetoothServiceDefinition service1 = new BluetoothServiceDefinition(SERVICE1_UUID);
		service1.addCharacteristic(SVC1_CHAR1_UUID, BluetoothProperties.NOTIFY, BluetoothPermissions.NONE);
		service1.addDescriptor(DescriptorHelper.CUD_UUID, BluetoothPermissions.READ);
		service1.addDescriptor(DescriptorHelper.CCC_UUID, BluetoothPermissions.READ | BluetoothPermissions.WRITE);
		service1.addCharacteristic(SVC1_CHAR2_UUID, BluetoothProperties.WRITE, BluetoothPermissions.WRITE);
		service1.addDescriptor(DescriptorHelper.CUD_UUID, BluetoothPermissions.READ);

		BluetoothServiceDefinition service2 = new BluetoothServiceDefinition(SERVICE2_UUID);
		service2.addCharacteristic(SVC2_CHAR1_UUID, BluetoothProperties.READ, BluetoothPermissions.READ);
		service2.addCharacteristic(SVC2_CHAR2_UUID, BluetoothProperties.WRITE, BluetoothPermissions.WRITE);

		BluetoothServiceDefinition service3 = new BluetoothServiceDefinition(SERVICE3_UUID);
		service3.addCharacteristic(SVC3_CHAR1_UUID, BluetoothProperties.INDICATE, BluetoothPermissions.NONE);
		service3.addDescriptor(DescriptorHelper.CCC_UUID, BluetoothPermissions.READ | BluetoothPermissions.WRITE);
		service3.addCharacteristic(SVC3_CHAR2_UUID, BluetoothProperties.READ, BluetoothPermissions.READ);

		BluetoothServiceDefinition service4 = new BluetoothServiceDefinition(SERVICE4_UUID);
		service4.addCharacteristic(SVC4_CHAR1_UUID, BluetoothProperties.READ | BluetoothProperties.WRITE,
				BluetoothPermissions.READ | BluetoothPermissions.WRITE);
		service4.addCharacteristic(SVC4_CHAR2_UUID, BluetoothProperties.READ, BluetoothPermissions.READ);

		BluetoothService svc1 = adapter.addService(service1);
		BluetoothService svc2 = adapter.addService(service2);
		BluetoothService svc3 = adapter.addService(service3);
		BluetoothService svc4 = adapter.addService(service4);

		BluetoothAdapter.getAdapter().setConnectionListener(this);
		startAdvertising();

		System.out.println(
				"*****************************************************************************************************");
		System.out.println(
				"*                       MicroEJ ble_add_service_scope_test_2 Validation 1.0.0                       *");
		System.out.println(
				"*****************************************************************************************************");
		System.out.println(
				"* Copyright 2013-2025 MicroEJ Corp.                                                                 *");
		System.out.println(
				"* Use of this source code is governed by a BSD-style license that can be found with this software.  *");
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
		System.out.println(
				"    Service                |     d58c8541-0113-4e37-b3e3-d3229512694b       |                        ");
		System.out.println(
				"        Characteristic     |     d58c8541-0113-4e37-b3e3-d3229512694c       |   READ                 ");
		System.out.println(
				"        Characteristic     |     d58c8541-0113-4e37-b3e3-d3229512694d       |   WRITE                ");
		System.out.println();
		System.out.println(
				"    Service                |     6675126c-94ba-46dc-afb6-7bb3db955be3       |                        ");
		System.out.println(
				"        Characteristic     |     6675126c-94ba-46dc-afb6-7bb3db955be4       |   INDICATE             ");
		System.out.println(
				"            Descriptor     |     2902                                       |   READ | WRITE         ");
		System.out.println(
				"        Characteristic     |     6675126c-94ba-46dc-afb6-7bb3db955be5       |   READ                 ");
		System.out.println();
		System.out.println(
				"    Service                |     8dc871e7-671a-48fa-803a-962d9bb39af1       |                        ");
		System.out.println(
				"        Characteristic     |     8dc871e7-671a-48fa-803a-962d9bb39af2       |   READ | WRITE         ");
		System.out.println(
				"        Characteristic     |     8dc871e7-671a-48fa-803a-962d9bb39af3       |   READ                 ");
		System.out.println();
		System.out.println("Check the success of the app by following the flow below :");
		System.out.println("Launch a BLE application on your mobile (nRF Connect recommended), start the scanning.");
		System.out.println("You can filter the devices by name add_... to find ours.");
		System.out.println("Once found, connect to the device.");
		System.out.println("If the connection is done, on nRF Connect, click on client tab and check that the ");
		System.out.println("attribute table has been created with the desired values.");

	}

	private void startAdvertising() {
		System.out.println("Start advertising");
		AdvertisementData data = new AdvertisementData();
		data.setDeviceName(DEVICE_NAME);
		BluetoothAdapter.getAdapter().startAdvertising(data.serialize());
	}

}