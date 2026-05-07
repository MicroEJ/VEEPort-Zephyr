/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

#ifndef  EVENT_CONFIGURATION_H
#define  EVENT_CONFIGURATION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * @brief LLEVENT configuration file for OSAL
 * @author MicroEJ Developer Team
 * @version 1.0.0
 */

// ----------------------------------------------------------------------------
// Includes
// ----------------------------------------------------------------------------

// Include VEE Port User configuration file
#if defined __has_include
	#if __has_include("veeport_configuration.h")
		#include "veeport_configuration.h"
	#endif // __has_include("veeport_configuration.h")
#else
// Ensure 'veeport_configuration.h' exists in your project for custom configurations.
	#include "veeport_configuration.h"
#endif // defined __has_include

// ----------------------------------------------------------------------------
// Macros and defines
// ----------------------------------------------------------------------------

/**
 * @brief Sets the Max number of events in the queue. (default is 100)
 */
#ifndef LLEVENT_QUEUE_SIZE
#define LLEVENT_QUEUE_SIZE (100U)
#endif // LLEVENT_QUEUE_SIZE

/**
 * @brief Sets function used to print LLEVENT error logs. (default is printf)
 */
#if !defined(LLEVENT_ERROR_TRACE)
// cppcheck-suppress [misra-c2012-21.6]: The Standard Library input/output is only used for debug purpose.
#include <stdio.h>
#define LLEVENT_ERROR_TRACE  printf("[Event] Error, "); printf
#endif // LLEVENT_ERROR_TRACE

// ----------------------------------------------------------------------------
// End
// ----------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif // EVENT_CONFIGURATION_H
