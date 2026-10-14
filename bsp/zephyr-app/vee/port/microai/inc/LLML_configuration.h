/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

#ifndef  LLML_CONFIGURATION_H
#define  LLML_CONFIGURATION_H

/**
 * @file
 * @brief MicroAI implementation over TensorFlow Lite.
 * @author MicroEJ Developer Team
 * @version 2.1.0
 */

// Included before the "C" linkage block: the VEE Port configuration pulls in headers
// that declare C++ templates and overloads, which cannot be given C linkage.
#if defined(__has_include)
#if __has_include(<veeport_configuration.h>)
#include <veeport_configuration.h>
#endif
#endif

#include "veeport_logger.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Configure ML Inference Engine Backend
 */
#define LLML_USE_TFLITE_MICRO 1
#define LLML_USE_TFLITE 2

#define LLML_BACKEND LLML_USE_TFLITE_MICRO

#if !(LLML_BACKEND == LLML_USE_TFLITE_MICRO) && !(LLML_BACKEND == LLML_USE_TFLITE)
#error "LLML: No backend selected -> Chose TensorFlow Lite / TensorFlow Lite for Microcontrollers"
#endif

/**
 * @brief Log module shared by every source file of the MicroAI Abstraction Layer.
 *
 * One source file registers the module with VEEPORT_LOGGER_DEFINE_MODULE() and the
 * others declare it with VEEPORT_LOGGER_DECLARE_MODULE(). The verbosity is set with
 * CONFIG_MICROEJ_MICROAI_LOG_LEVEL.
 */
#define LLML_LOG_MODULE microej_microai, VEEPORT_LOGGER_MODULE_LEVEL(MICROAI)

#ifdef __cplusplus
}
#endif

#endif // LLML_CONFIGURATION_H
