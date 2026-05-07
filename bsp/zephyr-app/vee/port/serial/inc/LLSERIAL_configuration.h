/*
 * Copyright 2025-2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */
#ifndef LLSERIAL_CONFIGURATION_H
#define LLSERIAL_CONFIGURATION_H

#include <osal.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Serial ABLA log macros */
#define LLIMPL_LOGLEVEL_NONE                     (0)
#define LLIMPL_LOGLEVEL_ERROR                    (LLIMPL_LOGLEVEL_NONE + 1)
#define LLIMPL_LOGLEVEL_WARNING                  (LLIMPL_LOGLEVEL_ERROR + 1)
#define LLIMPL_LOGLEVEL_INFO                     (LLIMPL_LOGLEVEL_WARNING + 1)
#define LLIMPL_LOGLEVEL_DEBUG                    (LLIMPL_LOGLEVEL_INFO + 1)

#define LLIMPL_ERROR_TRACE(...)                  (void)printk("[ERR] "); (void)printk(__VA_ARGS__)
#define LLIMPL_WARNING_TRACE(...)                (void)printk("[WRN] "); (void)printk(__VA_ARGS__)
#define LLIMPL_DEBUG_TRACE(...)                  (void)printk("[DEB] "); (void)printk(__VA_ARGS__)
#define LLIMPL_INFO_TRACE(...)                   (void)printk("[INF] "); (void)printk(__VA_ARGS__)

/** @brief Serial ABLA Log level */
#ifndef LLSERIAL_LOGLEVEL
#define LLSERIAL_LOGLEVEL                        LLIMPL_LOGLEVEL_ERROR
#endif

/**
 * @brief Set Serial ABLA log level
 */
#if LLSERIAL_LOGLEVEL >= LLIMPL_LOGLEVEL_DEBUG
#define LLSERIAL_DEBUG_TRACE(...)                LLIMPL_DEBUG_TRACE(__VA_ARGS__);
#else
#define LLSERIAL_DEBUG_TRACE(...)                ((void)0)
#endif
#if LLSERIAL_LOGLEVEL >= LLIMPL_LOGLEVEL_INFO
#define LLSERIAL_INFO_TRACE(...)                 LLIMPL_INFO_TRACE(__VA_ARGS__);
#else
#define LLSERIAL_INFO_TRACE(...)                 ((void)0)
#endif
#if LLSERIAL_LOGLEVEL >= LLIMPL_LOGLEVEL_ERROR
#define LLSERIAL_ERROR_TRACE(...)                LLIMPL_ERROR_TRACE(__VA_ARGS__);
#else
#define LLSERIAL_ERROR_TRACE(...)                ((void)0)
#endif

#define LLSERIAL_OPERATION_NOT_IMPLEMENTED       "Operation not implemented"

#ifdef __cplusplus
}
#endif
#endif // LLSERIAL_CONFIGURATION_H
