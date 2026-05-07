/*
 * Java
 *
 * Copyright 2018-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */
package com.microej.example.bluetooth.peripheral;

import java.util.Map;
import java.util.WeakHashMap;

import ej.bluetooth.BluetoothAttribute;
import ej.bluetooth.BluetoothCharacteristic;
import ej.bluetooth.BluetoothConnection;
import ej.bluetooth.BluetoothDescriptor;
import ej.bluetooth.BluetoothService;
import ej.bluetooth.BluetoothStatus;
import ej.bluetooth.listeners.impl.DefaultLocalServiceListener;
import ej.bluetooth.util.AttributeNotFoundException;
import ej.bluetooth.util.DescriptorHelper;
import ej.bluetooth.util.ServiceHelper;

/**
 * The <code>SerialPortServer</code> class represents a serial port server.
 */
public abstract class SerialPortServer extends DefaultLocalServiceListener {

	private static final String TX_DESCRIPTION = "TX";
	private static final String RX_DESCRIPTION = "RX";

	private final BluetoothService service;
	private final BluetoothCharacteristic tx;
	private final BluetoothCharacteristic rx;
	private final BluetoothDescriptor txCud;
	private final BluetoothDescriptor txCcc;
	private final BluetoothDescriptor rxCud;

	private final Map<BluetoothConnection, Boolean> txSubscribers;

	/**
	 * Creates a serial port server, using the serial port service provided by this device.
	 *
	 * @param service
	 *            the serial port service provided by this device.
	 * @throws IllegalArgumentException
	 *             if one of the mandatory attributes of the service is missing.
	 */
	public SerialPortServer(BluetoothService service) {
		this.service = service;

		try {
			this.tx = ServiceHelper.getCharacteristic(service, SerialPortConstants.TX_UUID);
			this.rx = ServiceHelper.getCharacteristic(service, SerialPortConstants.RX_UUID);
			this.txCud = ServiceHelper.getDescriptor(this.tx, DescriptorHelper.CUD_UUID);
			this.txCcc = ServiceHelper.getDescriptor(this.tx, DescriptorHelper.CCC_UUID);
			this.rxCud = ServiceHelper.getDescriptor(this.rx, DescriptorHelper.CUD_UUID);
		} catch (AttributeNotFoundException e) {
			throw new IllegalArgumentException("Invalid serial port service");
		}

		this.txSubscribers = new WeakHashMap<>();
	}

	/**
	 * Starts this serial port server.
	 */
	public void start() {
		this.service.setLocalListener(this);
	}

	/**
	 * Stops this serial port server.
	 */
	public void stop() {
		this.service.setLocalListener(new DefaultLocalServiceListener());
	}

	/**
	 * Sends the given data to the given client. The {@link #onDataSent(BluetoothConnection, boolean)} methods is called
	 * once the data has been sent to the client.
	 *
	 * @param connection
	 *            the connection to the device to which the data should be sent.
	 * @param data
	 *            the data to send.
	 */
	public void sendData(BluetoothConnection connection, byte[] data) {
		if (this.txSubscribers.containsKey(connection)) {
			connection.sendNotification(this.tx, data, true);
		}
	}

	@Override
	public void onReadRequest(BluetoothConnection connection, BluetoothAttribute attribute) {
		if (attribute == this.txCud) {
			connection.sendReadResponse(attribute, BluetoothStatus.OK, TX_DESCRIPTION.getBytes());
		} else if (attribute == this.rxCud) {
			connection.sendReadResponse(attribute, BluetoothStatus.OK, RX_DESCRIPTION.getBytes());
		} else {
			super.onReadRequest(connection, attribute);
		}
	}

	@Override
	public void onWriteRequest(BluetoothConnection connection, BluetoothAttribute attribute, byte[] value) {
		if (attribute == this.rx) {
			connection.sendWriteResponse(attribute, BluetoothStatus.OK);
			onDataReceived(connection, value);
		} else if (attribute == this.txCcc) {
			if (DescriptorHelper.checkIndicationsEnabled(value)) {
				this.txSubscribers.put(connection, Boolean.TRUE);
			} else {
				this.txSubscribers.remove(connection);
			}
			connection.sendWriteResponse(attribute, BluetoothStatus.OK);
		} else {
			super.onWriteRequest(connection, attribute, value);
		}
	}

	@Override
	public void onNotificationSent(BluetoothConnection connection, BluetoothCharacteristic characteristic,
			boolean success) {
		if (characteristic == this.tx) {
			onDataSent(connection, success);
		} else {
			super.onNotificationSent(connection, characteristic, success);
		}
	}

	/**
	 * Called when data has been received from a client.
	 *
	 * @param connection
	 *            the connection to the device from which the data has been received.
	 * @param data
	 *            the data received from the client.
	 */
	protected abstract void onDataReceived(BluetoothConnection connection, byte[] data);

	/**
	 * Called when data has been sent to a client.
	 *
	 * @param connection
	 *            the connection to the device to which the data has been sent.
	 * @param success
	 *            true if the the data was sent successfully, false otherwise.
	 */
	protected abstract void onDataSent(BluetoothConnection connection, boolean success);
}
