/*
 * Copyright 2014-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "x_ram_checks.h"
#include "bsp_util.h"

#define ZONES_NUMBER (1)

static X_RAM_CHECKS_zone_t zones[ZONES_NUMBER];

#define RAM_BUFFER_SIZE (CONFIG_MICROEJ_VALIDATION_RAM_CHECK_BUFFER_SIZE * 1024)
uint8_t ram_buffer[RAM_BUFFER_SIZE];

/* Lazy initialization flag */
static bool initialize;

static void initialize_ram_zone(void) {
	if(initialize == false) {
		initialize = true;
		zones[0].start_address = (uint32_t)ram_buffer;
		zones[0].end_address = (uint32_t)ram_buffer + RAM_BUFFER_SIZE - 1;
	}
}

/**
 *
 * @brief  this function provide a definition array of  memory zones to test with 32-bit accesses.
 *
 * @return array of X_RAM_CHECKS_zone_t
 */

X_RAM_CHECKS_zone_t * X_RAM_CHECKS_get32bitZones(void) {
	initialize_ram_zone();
	return &zones[0];
}

/**
 * @brief this function provide a definition array of  memory zones to test with 16-bit accesses.
 *
 * @return array of X_RAM_CHECKS_zone_t
 */

X_RAM_CHECKS_zone_t * X_RAM_CHECKS_get16bitZones(void) {
	initialize_ram_zone();
	return &zones[0];
}

/**
 * @brief
 *
 * @return array of X_RAM_CHECKS_zone_t
 */

X_RAM_CHECKS_zone_t * X_RAM_CHECKS_get8bitZones(void) {
	initialize_ram_zone();
	return &zones[0];
}

/**
 * @brief
 *
 * @return number of zones to test
 */

uint8_t X_RAM_CHECKS_get32bitZoneNumber(void) {
	initialize_ram_zone();
	return ZONES_NUMBER;
}

/**
 * @brief
 *
 * @return
 */

uint8_t X_RAM_CHECKS_get16bitZoneNumber(void) {
	initialize_ram_zone();
	return ZONES_NUMBER;
}

/**
 * @brief
 *
 * @return
 */

uint8_t X_RAM_CHECKS_get8bitZoneNumber(void) {
	initialize_ram_zone();
	return ZONES_NUMBER;
}

/**
 * @brief
 *
 * @return
 */

X_RAM_CHECKS_zone_t * X_RAM_CHECKS_get32bitSourceZone(void) {
	initialize_ram_zone();
	return &zones[0];
}

/**
 * @brief
 *
 * @return
 */

X_RAM_CHECKS_zone_t * X_RAM_CHECKS_get16bitSourceZone(void) {
	initialize_ram_zone();
	return &zones[0];
}

/**
 * @brief
 *
 * @return
 */

X_RAM_CHECKS_zone_t * X_RAM_CHECKS_get8bitSourceZone(void) {
	initialize_ram_zone();
	return &zones[0];
}
