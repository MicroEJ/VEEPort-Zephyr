/*
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */
#ifndef LLSERIAL_CONFIGURATION_H
#define LLSERIAL_CONFIGURATION_H

#include <osal.h>

#include "veeport_logger.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Log module shared by every source file of the Serial Abstraction Layer.
 *
 * One source file registers the module with VEEPORT_LOGGER_DEFINE_MODULE() and the
 * others declare it with VEEPORT_LOGGER_DECLARE_MODULE(). The verbosity is set with
 * CONFIG_MICROEJ_SERIAL_LOG_LEVEL.
 */
#define LLSERIAL_LOG_MODULE microej_serial, VEEPORT_LOGGER_MODULE_LEVEL(SERIAL)

#define LLSERIAL_OPERATION_NOT_IMPLEMENTED       "Operation not implemented"

#ifdef __cplusplus
}
#endif
#endif // LLSERIAL_CONFIGURATION_H
