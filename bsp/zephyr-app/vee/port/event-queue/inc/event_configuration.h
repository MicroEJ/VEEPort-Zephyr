/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
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

#include "veeport_logger.h"
#include "veeport_errors.h"

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
 * @brief Log module shared by every source file of the Event Queue Abstraction Layer.
 *
 * One source file registers the module with VEEPORT_LOGGER_DEFINE_MODULE() and the
 * others declare it with VEEPORT_LOGGER_DECLARE_MODULE(). The verbosity is set with
 * CONFIG_MICROEJ_EVENT_QUEUE_LOG_LEVEL.
 */
#define LLEVENT_LOG_MODULE microej_event_queue, VEEPORT_LOGGER_MODULE_LEVEL(EVENT_QUEUE)

// ----------------------------------------------------------------------------
// End
// ----------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif // EVENT_CONFIGURATION_H
