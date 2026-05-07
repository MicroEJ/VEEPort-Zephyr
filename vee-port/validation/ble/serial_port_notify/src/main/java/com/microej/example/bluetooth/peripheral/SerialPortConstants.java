/*
 * Java
 *
 * Copyright 2018-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */
package com.microej.example.bluetooth.peripheral;

import ej.bluetooth.BluetoothPermissions;
import ej.bluetooth.BluetoothProperties;
import ej.bluetooth.BluetoothServiceDefinition;
import ej.bluetooth.BluetoothUuid;
import ej.bluetooth.util.DescriptorHelper;
import ej.bluetooth.util.ServiceHelper;

/**
 * Provides constants and static methods related to the serial port service.
 */
public class SerialPortConstants {

	/** The UUID of the serial port service. */
	public static final BluetoothUuid SERVICE_UUID = new BluetoothUuid("0783b03e-8535-b5a0-7140-a304d2495cb7");

	/** The UUID of the TX characteristic. */
	public static final BluetoothUuid TX_UUID = new BluetoothUuid("0783b03e-8535-b5a0-7140-a304d2495cb8");

	/** The UUID of the RX characteristic. */
	public static final BluetoothUuid RX_UUID = new BluetoothUuid("0783b03e-8535-b5a0-7140-a304d2495cba");

	private SerialPortConstants() {
		// private constructor
	}

	/**
	 * Gets the definition of the serial port service.
	 *
	 * @param securityLevel
	 *            the security level (see {@link ej.bluetooth.util.SecurityLevels}).
	 * @return the definition of the serial port service.
	 */
	public static BluetoothServiceDefinition getServiceDefinition(int securityLevel) {
		int readPermission = ServiceHelper.getReadPermission(securityLevel);
		int writePermission = ServiceHelper.getWritePermission(securityLevel);

		BluetoothServiceDefinition service = new BluetoothServiceDefinition(SERVICE_UUID);
		service.addCharacteristic(TX_UUID, BluetoothProperties.NOTIFY, BluetoothPermissions.NONE);
		service.addDescriptor(DescriptorHelper.CUD_UUID, readPermission);
		service.addDescriptor(DescriptorHelper.CCC_UUID, readPermission | writePermission);
		service.addCharacteristic(RX_UUID, BluetoothProperties.WRITE, writePermission);
		service.addDescriptor(DescriptorHelper.CUD_UUID, readPermission);
		return service;
	}
}
