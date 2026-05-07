/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

/* Includes ------------------------------------------------------------------*/

#include <stdint.h>
#include <string.h>
#include "sni.h"
#include "LLDEVICE_impl.h"
#include <zephyr/drivers/hwinfo.h>
#include "zephyr/sys/reboot.h"
#include "zephyr/sys/poweroff.h"

/* Defines -------------------------------------------------------------------*/

#define VERSION_BOARD_REVISION "Hardware revision: "

/* Public functions ----------------------------------------------------------*/

/**
 * Fills the given buffer with the name of the architecture. The buffer is filled with a null-terminated string.
 * @param buffer the buffer to fill with the name of the architecture.
 * @param length the size of the buffer.
 * @return 1 on success or 0 on error.
 */
uint8_t LLDEVICE_IMPL_getArchitecture(uint8_t *buffer, int32_t length) {
	int architecture_length = strlen(CONFIG_BOARD) + 1; // include '\0'
	if (length >= architecture_length) {
		strncpy(buffer, CONFIG_BOARD, length - 1);
		buffer[length - 1] = '\0';  // manually ensure null termination
		return 1;
	} else {
		// buffer too small
		return 0;
	}
}

/**
 * Fills the given buffer with the ID of the device.
 * @param buffer the buffer to fill with the ID.
 * @param length the size of the buffer.
 * @return the number of bytes filled or 0 on error.
 */
uint32_t LLDEVICE_IMPL_getId(uint8_t *buffer, int32_t length) {
	ssize_t id_len = hwinfo_get_device_id(buffer, length);

	if (id_len < 0) {
		return 0;
	}

	return (uint32_t)id_len;
}

/**
 * Fills the given buffer with the Version of the device as
 * a null-terminated string.
 *
 * @param buffer the buffer to fill with the Version.
 */
void LLDEVICE_IMPL_getVersion(uint8_t *buffer) {
#ifdef CONFIG_BOARD_REVISION
	int length = SNI_getArrayLength(buffer);
	int architecture_length = strlen(VERSION_BOARD_REVISION) + strlen(CONFIG_BOARD_REVISION);
	if (length > architecture_length) {
		strncpy(buffer, VERSION_BOARD_REVISION, length - 1);
		strncat(buffer, CONFIG_BOARD_REVISION, length - strlen((char *)buffer) - 1);
		buffer[length - 1] = '\0';
	} else {
		// buffer too small
		(void)SNI_throwNativeException(-1, "Failed to retrieve device revision: buffer is too small.");
	}
#else
	(void)SNI_throwNativeException(-1, "Zephyr board revision is not implemented.");
#endif
}

/**
 * Reboots the device.
 */
void LLDEVICE_IMPL_reboot(void) {
#ifdef CONFIG_REBOOT
	sys_reboot(SYS_REBOOT_COLD);
#else
	(void)SNI_throwNativeException(-1, "Zephyr reboot functionality is not implemented.");
#endif
}

/**
 * Shutdowns the device.
 */
void LLDEVICE_IMPL_shutdown(void) {
#ifdef CONFIG_HAS_POWEROFF
	sys_poweroff();
#else
	(void)SNI_throwNativeException(-1, "Zephyr power off functionality is not implemented.");
#endif
}
