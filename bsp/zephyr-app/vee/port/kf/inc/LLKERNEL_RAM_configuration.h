/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

/**
 * @file
 * @brief LLKERNEL RAM implementation.
 * @author MicroEJ Development Team
 * @version 3.0.0
 */

#ifndef  LLKERNEL_RAM_CONFIGURATION_H
#define  LLKERNEL_RAM_CONFIGURATION_H

#include <stdint.h>

// Included outside of any "C" linkage block: it reaches the Zephyr logging header,
// which declares C++ templates and overloads that cannot be given C linkage.
#include "veeport_logger.h"

/**
 * @brief Log module shared by every source file of the KF Abstraction Layer.
 *
 * One source file registers the module with VEEPORT_LOGGER_DEFINE_MODULE() and the
 * others declare it with VEEPORT_LOGGER_DECLARE_MODULE(). The verbosity is set with
 * CONFIG_MICROEJ_KF_LOG_LEVEL.
 */
#define LLKERNEL_LOG_MODULE microej_kf, VEEPORT_LOGGER_MODULE_LEVEL(KF)

/**
 * @brief Kernel RAM allocation mode:
 * - KERNEL_RAM_IMPL_MALLOC with malloc()/free(): recommended configuration on Linux systems.
 * - KERNEL_RAM_IMPL_BESTFIT with best fit allocator: recommended configuration on MCUs.
 *
 */
#ifndef KERNEL_RAM_IMPL_BESTFIT
#define KERNEL_RAM_IMPL_MALLOC
#else
#define KERNEL_RAM_IMPL_BESTFIT
#endif

/**
 * @brief Uncomment this macro to set {@link #LLKERNEL_MAX_NB_DYNAMIC_FEATURES}
 * value to a custom value.
 */
//#define LLKERNEL_MAX_NB_DYNAMIC_FEATURES	XX

#endif // LLKERNEL_RAM_CONFIGURATION_H
