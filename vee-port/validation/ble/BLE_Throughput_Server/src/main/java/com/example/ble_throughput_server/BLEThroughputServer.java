/*
 * Java
 *
 * Copyright 2024-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

package com.example.ble_throughput_server;

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
import ej.bon.Timer;
import ej.bon.TimerTask;

/**
 * The <code>SerialPortServer</code> class represents a serial port server.
 */
public class BLEThroughputServer extends DefaultLocalServiceListener {

    private static final Integer TIMEOUT_MS = 1000;
    /* This value shall be non-constant and depend on the MTU value in next bluetooth APIs */
    private static final int PACKET_SIZE = 244;

    private static final String NOTIFY_DESCRIPTION = "NOTIFY";

    private Timer timer;
    private static TimerTask secTimeout;
    private static int gattNotifyTxBytes = 0;
    NotifyThread notifyThread;
    volatile static boolean flagRun = true;
    private static int throughputMeasureCounter = 0;
    private final byte[] data = new byte[PACKET_SIZE];

    private BluetoothConnection conn;

    private final BluetoothService service;
    private final BluetoothCharacteristic notify;
    private final BluetoothDescriptor notifyCud;
    private final BluetoothDescriptor notifyCcc;

    private final Map<BluetoothConnection, Boolean> txSubscribers;

    /**
     * Creates a serial port server, using the serial port service provided by this device.
     *
     * @param service
     *            the serial port service provided by this device.
     * @throws IllegalArgumentException
     *             if one of the mandatory attributes of the service is missing.
     *             
     */
    public BLEThroughputServer(BluetoothService service) {
        this.service = service;

        try {
            this.notify = ServiceHelper.getCharacteristic(service, BLEThroughputConstants.NOTIFY_UUID);
            this.notifyCud = ServiceHelper.getDescriptor(this.notify, DescriptorHelper.CUD_UUID);
            this.notifyCcc = ServiceHelper.getDescriptor(this.notify, DescriptorHelper.CCC_UUID);
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

        timer = new Timer();
    }

    /**
     * Stops this serial port server.
     */
    public void stop() {
        this.service.setLocalListener(new DefaultLocalServiceListener());
    }

    /**
     * Sends the given data to the given client.
     *
     * @param connection
     *            the connection to the device to which the data should be sent.
     * @param data
     *            the data to send.
     */
    public void sendData(BluetoothConnection connection, byte[] data) {
        if (this.txSubscribers.containsKey(connection)) {
            connection.sendNotification(this.notify, data, false);
        }
    }

    @Override
    public void onReadRequest(BluetoothConnection connection, BluetoothAttribute attribute) {
        if (attribute == this.notifyCud) {
            connection.sendReadResponse(attribute, BluetoothStatus.OK, NOTIFY_DESCRIPTION.getBytes());
        } else {
            super.onReadRequest(connection, attribute);
        }
    }

    @Override
    public void onWriteRequest(BluetoothConnection connection, BluetoothAttribute attribute, byte[] value) {
        if (attribute == this.notifyCcc) {
            if (DescriptorHelper.checkNotificationsEnabled(value)) {
                starTest();
                this.txSubscribers.put(connection, Boolean.TRUE);
            } else {
                stopTest();
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
        if (characteristic == this.notify) {
            System.out.println("Data sent");
        } else {
            super.onNotificationSent(connection, characteristic, success);
        }
    }

    public void starTest(){
        secTimeout = new ThroughputMeasure();
        timer.schedule(secTimeout, TIMEOUT_MS, TIMEOUT_MS);

        conn = BLEThroughputApp.getConnection();

        notifyThread = new NotifyThread();
        notifyThread.start();

        flagRun = true;
    }

    public void stopTest(){
        if (secTimeout != null) {
            secTimeout.cancel();
            secTimeout = null;
        }
        flagRun = false;
        throughputMeasureCounter = 0;
    }

    private class ThroughputMeasure extends TimerTask {

        public void run(){
            gattNotifyTxBytes = (gattNotifyTxBytes * 8)/1000;
            System.out.println("GATT NOTIFICATION : Server Throughput <TX> " + gattNotifyTxBytes + " kbps");
            gattNotifyTxBytes = 0;
            throughputMeasureCounter += 1;

            if (throughputMeasureCounter == 30){
                System.out.println("END OF BLE THROUGHPUT SERVER TEST !");
                stopTest();
            }
        }
    }

    private class NotifyThread extends Thread {

        public NotifyThread(){

        }

        @Override
        public void run(){
            // int i = 0;
            while(flagRun) {
            // while(i < 10) {
                sendData(conn, data);
                gattNotifyTxBytes += PACKET_SIZE;
                // i++;
            }
        }
    }
}
