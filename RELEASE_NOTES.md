# VEE Port Release Notes for Zephyr

## Description

This is the release notes of the VEE Port for Zephyr.

## Versions

### VEE Port

4.0.0

### Dependencies

This VEE Port contains the following dependencies:

| Pack Name                            | Version |
|--------------------------------------|---------|
| Architecture (default: flopi7G26)    | 8.6.0   |
| BLUETOOTH                            | 3.0.0   |
| CRYPTO                               | 2.1.0   |
| DEVICE                               | 1.4.0   |
| EVENT                                | 3.0.3   |
| FS                                   | 6.0.4   |
| MICROAI                              | 2.2.0   |
| NET                                  | 15.0.1  |
| SERIAL                               | 3.0.0   |


Please refer to the VEE Port [module description file](./vee-port/build.gradle.kts)
and the [version catalogs file](./gradle/libs.versions.toml) for more details.

### Board Support Package

- BSP provider and name: Zephyr
- BSP version: v4.4.2

Please refer to the Zephyr git repository
available [here](https://github.com/zephyrproject-rtos/zephyr).

## Features

### Architecture

The Multi-Sandbox Application download implementation is based on Best Fit Allocator backend in RAM, so without reboot persistence.
Features are allocated in a fixed-size dedicated buffer.

### Crypto Pack

The Crypto Abstraction Layer is implemented on top of the PSA Crypto API of Mbed TLS 4.1 and TF-PSA-Crypto.

The sources (`bsp/zephyr-app/vee/port/crypto/src` and `inc`) also build against Mbed TLS 3.6, the version shipped by Zephyr 4.3.
With Mbed TLS 3, add `MBEDTLS_USE_PSA_CRYPTO` to the `Kconfig` of the port.
The algorithms removed by Mbed TLS 4 are available again once their PSA options are enabled: `PSA_WANT_KEY_TYPE_DES` for DESede, `PSA_WANT_ECC_SECP_R1_192` and `PSA_WANT_ECC_SECP_R1_224` for the secp192r1 and secp224r1 curves.

### Device Pack

The Device UID is based on the Zephyr [Hardware Information](https://docs.zephyrproject.org/latest/hardware/peripherals/hwinfo.html) API.

### Network Pack

This VEE Port features a network interface with ethernet as an underlying hardware media.

### SSL Pack

The SSL Abstraction Layer is implemented on top of Mbed TLS 4.1 and TF-PSA-Crypto.
It supports the TLS 1.2 protocol.

The sources (`bsp/zephyr-app/vee/port/ssl/src` and `inc`) also build against Mbed TLS 3.6, the version shipped by Zephyr 4.3.
With Mbed TLS 3, add the following options to the `Kconfig` of the port:

- `MBEDTLS_USE_PSA_CRYPTO` and `MBEDTLS_ENTROPY_C`: the TLS stack takes its random generator from PSA, seeded by the entropy module
- `MBEDTLS_PEM_CERTIFICATE_FORMAT`: PEM certificates
- `MBEDTLS_KEY_EXCHANGE_ALL_ENABLED`, `MBEDTLS_ECDH_C`, `MBEDTLS_ECJPAKE_C` and `PSA_WANT_ALG_JPAKE`: the ECDHE and PSK key exchanges (the two ECJPAKE options satisfy a dependency of `MBEDTLS_KEY_EXCHANGE_ALL_ENABLED`)
- `MBEDTLS_CIPHER_AES_ENABLED` and `MBEDTLS_CIPHER_MODE_CBC_ENABLED`: AES-CBC, used to decrypt encrypted PKCS#8 private keys

### File System Pack

This VEE Port features a file system interface.
RAM, Flash or SD card could be used for the storage (previously formated to FatFS or LittleFS file system) depending on the VEE Port configurations.

### MicroAI Pack

MicroAI implementation is based on TensorFlow Lite for Microcontrollers.

### Serial Pack

This VEE Port features a serial foundation library via UART.

## Known Issues/Limitations

Limitations:

- Wi-Fi not supported by NET implementation. The board needs an Ethernet connector.

- The Bluetooth implementation handles a single connection at a time, whatever the role.
  [BluetoothDevice.connect()](https://repository.microej.com/javadoc/microej_5.x/apis/ej/bluetooth/BluetoothDevice.html)
  fails while a connection is ongoing, and an application should not advertise while connected.

- The following features of the file system are not available:
  - Setting last modified date
  - Getting last modified date
  - Synchronized I/O file integrity completion (FS_O_SYNC not supported by Zephyr)
  - Classes impacted by these limitations are the following:
    - [File](https://repository.microej.com/javadoc/microej_5.x/apis/java/io/File.html)
    - [RandomAccessFile](https://repository.microej.com/javadoc/microej_5.x/apis/java/io/RandomAccessFile.html)

- The crypto test suite can't pass fully on RAM constrained platforms, such as FRDM-MCXN947. However you can either:
  - disable other Foundation libraries (ex: MicroAI, KF, etc.)
    in [frdm_mcxn947_mcxn947_cpu0.conf](bsp/zephyr-app/main/boards/frdm_mcxn947_mcxn947_cpu0.conf)
    ```
    CONFIG_MICROEJ_MICROAI=n
    ```
  - or reduce the RSA key sizes to 2048 bits at most, as the per-board overlay
    [microej-testsuite-mcxn947.properties](vee-port/validation/crypto/boards/microej-testsuite-mcxn947.properties) does:
    ```
    microej.java.property.com.microej.crypto.test.keypairgen.rsa.keysizes=1024,2048
    microej.java.property.com.microej.crypto.test.cipher.rsa.keysizes=1024,2048
    ```

- Algorithms removed by Mbed TLS 4 / TF-PSA-Crypto, which Zephyr 4.4 ships, are not available in the Crypto and SSL
  Foundation Libraries: DES and DESede (including the PBEWithSHA1AndDESede password-based cipher and PKCS#12
  key derivation), the secp192r1/k1 and secp224r1/k1 curves, and the TLS 1.2 RSA, RSA-PSK, DHE and static ECDH key
  exchanges. TLS 1.2 uses ECDHE-RSA, ECDHE-ECDSA or PSK key exchanges. Encrypted PKCS#8 private keys given to
  `SSLContext` must use PBES2 with AES-CBC; keys encrypted with 3DES (the OpenSSL 1.0 default) or a PKCS#12 PBE scheme are
  rejected.

- On FRDM-MCXN947 the PSA random generator is seeded by Zephyr's test random generator
  (`CONFIG_TEST_RANDOM_GENERATOR=y`), which is not cryptographically secure. This configuration is for evaluation only.

---
_Markdown_  
_Copyright 2025-2026 MicroEJ Corp. All rights reserved._  
_Use of this source code is governed by a BSD-style license that can be found with this software._  
_Build: 7E4D1F7C_  
