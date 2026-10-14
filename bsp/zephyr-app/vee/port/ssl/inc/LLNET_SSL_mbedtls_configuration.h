/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

#ifndef LLNET_SSL_MBEDTLS_CONFIGURATION_H
#define LLNET_SSL_MBEDTLS_CONFIGURATION_H

#include <mbedtls/build_info.h>

// Included outside of the "C" linkage block below: it reaches the Zephyr logging header,
// which declares C++ templates and overloads that cannot be given C linkage.
#include "veeport_logger.h"
#include "veeport_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Log module shared by every source file of the SSL Abstraction Layer.
 *
 * One source file registers the module with VEEPORT_LOGGER_DEFINE_MODULE() and the
 * others declare it with VEEPORT_LOGGER_DECLARE_MODULE(). The verbosity is set with
 * CONFIG_MICROEJ_SSL_LOG_LEVEL.
 */
#define LLNET_SSL_LOG_MODULE microej_ssl, VEEPORT_LOGGER_MODULE_LEVEL(SSL)

/**
 * @brief Compatibility sanity check value.
 * This define value is checked in the implementation to validate that the version of this configuration
 * is compatible with the implementation.
 *
 * This value must not be changed by the user of the CCO.
 * This value must be incremented by the implementor of the CCO when a configuration define is added, deleted or
 * modified.
 */
#define LLNET_SSL_MBEDTLS_CONFIGURATION_VERSION (2)

#ifdef __cplusplus
}
#endif

#endif //LLNET_SSL_MBEDTLS_CONFIGURATION_H
