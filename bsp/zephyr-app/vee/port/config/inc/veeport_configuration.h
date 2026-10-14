/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

/**
 * @file veeport_configuration.h
 *
 * @brief Configures the C modules integrated for this port of MICROEJ VEE.
 *
 * This header file contains all the custom C modules configurations of the VEE Port.
 * Use this file to override the C module configurations in the VEE Port.
 * Please refer to the C module configuration files to learn about the available configurations.
 * This file should also be used to define VEE Port specific configurations.
 *
 * ***** HOW TO INTEGRATE THIS FILE WITH C MODULES *****
 *
 * This section explains how to integrate this file with C module configuration files.
 *
 * The configuration file of each C module (i.e. '[C MODULE NAME]_configuration.h') should include this
 * file as followed:
 *
 *  // Include VEE Port User configuration file
 *  #if defined __has_include
 *  	#if __has_include("veeport_configuration.h")
 *  		#include "veeport_configuration.h"
 *  	#endif // __has_include("veeport_configuration.h")
 *  #else
 *  // Ensure 'veeport_configuration.h' exists in your project for custom configurations.
 *  	#include "veeport_configuration.h"
 *  #endif // defined __has_include
 *
 * Then, in the '[C MODULE NAME]_configuration.h' file, default values for each parameter should be provided
 * as followed:
 *
 * #ifndef MY_CONFIGURATION
 * #define MY_CONFIGURATION (MY_CONFIGURATION_DEFAULT_VALUE)
 * #endif
 *
 */

#ifndef VEEPORT_CONFIGURATION_H
#define VEEPORT_CONFIGURATION_H

#include "veeport_logger_zephyr.h"

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------
// Includes
// ----------------------------------------------------------------------------
#include <stdio.h>

// ############################################################################
//                   VEE Port Essentials Critical Section
// ############################################################################

/**
 * @brief Enters the critical section that protects the shared state of VEE Port Essentials.
 *
 * The error helpers format their message in one shared buffer, and the abstraction layers throw from
 * the VM thread and log from their worker threads. Both functions map the section onto a Zephyr
 * mutex, see veeport_critical_section.c.
 */
void VEEPORT_CRITICAL_SECTION_enter(void);

/**
 * @brief Leaves the critical section entered by VEEPORT_CRITICAL_SECTION_enter().
 */
void VEEPORT_CRITICAL_SECTION_exit(void);

#define VEEPORT_ENTER_CRITICAL() VEEPORT_CRITICAL_SECTION_enter()
#define VEEPORT_EXIT_CRITICAL()  VEEPORT_CRITICAL_SECTION_exit()

// ############################################################################
//                        Core Engine Configuration
// ############################################################################

// ############################################################################
//                        KF Configuration
// ############################################################################

// ############################################################################
//                        Event Queue Configuration
// ############################################################################

// ############################################################################
//                        FS Configuration
// ############################################################################

// ############################################################################
//                        NET Configuration
// ############################################################################

// ############################################################################
//                        UI Configuration
// ############################################################################

// ############################################################################
//                        VG Configuration
// ############################################################################

// ----------------------------------------------------------------------------
// End
// ----------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif // VEEPORT_CONFIGURATION_H
