/*
 * Java
 *
 * Copyright 2024-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

package com.example.ble_throughput_server;

import ej.bluetooth.BluetoothPermissions;
import ej.bluetooth.BluetoothProperties;
import ej.bluetooth.BluetoothServiceDefinition;
import ej.bluetooth.BluetoothUuid;
import ej.bluetooth.util.DescriptorHelper;
import ej.bluetooth.util.ServiceHelper;

public class BLEThroughputConstants {

    /** The UUID of the serial port service. */
    public static final BluetoothUuid SERVICE_UUID = new BluetoothUuid("e9258360-be81-4b2a-906d-c6c9b503f293");

    /** The UUID of the NOTIFY characteristic. */
    public static final BluetoothUuid NOTIFY_UUID = new BluetoothUuid("e9258360-be81-4b2a-906d-c6c9b503f294");

    private BLEThroughputConstants(){

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
        service.addCharacteristic(NOTIFY_UUID, BluetoothProperties.NOTIFY, BluetoothPermissions.NONE);
        service.addDescriptor(DescriptorHelper.CUD_UUID, readPermission);
        service.addDescriptor(DescriptorHelper.CCC_UUID, readPermission | writePermission);
        return service;
    }


}
