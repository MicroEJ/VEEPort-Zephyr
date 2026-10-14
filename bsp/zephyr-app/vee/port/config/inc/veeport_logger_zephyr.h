/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

/**
 * @file veeport_logger_zephyr.h
 *
 * @brief Maps the VEE Port Essentials logger API onto the Zephyr logging subsystem.
 *
 * veeport_logger.h defines its macros only when the VEE Port does not provide its own, so the
 * definitions below take precedence and the Essentials logger implementation (message pool and
 * backends) stays unused. Logs then go through Zephyr, which already provides the levels, the
 * runtime filtering, the deferred mode and the backends, all configurable with Kconfig.
 *
 * This header is included by veeport_configuration.h, which every abstraction layer reaches
 * through veeport_logger.h. It must be included outside of any "C" linkage block, because the
 * Zephyr logging header it pulls in declares C++ templates and overloads.
 *
 * Here is how an abstraction layer uses it. The module macro pairs the module name with its
 * verbosity, so that a source file names the module only once:
 *
 * @code
 * // in the header shared by every source file of the layer
 * #include "veeport_logger.h"
 * #define LLFOO_LOG_MODULE  microej_foo, VEEPORT_LOGGER_MODULE_LEVEL(FOO)
 *
 * // in one source file of the layer
 * VEEPORT_LOGGER_DEFINE_MODULE(LLFOO_LOG_MODULE)
 *
 * // in every other source file that logs
 * VEEPORT_LOGGER_DECLARE_MODULE(LLFOO_LOG_MODULE)
 * @endcode
 */

#ifndef VEEPORT_LOGGER_ZEPHYR_H
#define VEEPORT_LOGGER_ZEPHYR_H

#include <zephyr/logging/log.h>

/*
 * Both scales run from 0 (none) to 4 (debug), but Zephyr pastes the level into a token to tell a
 * given level from an omitted one, so it only accepts the bare numbers its own LOG_LEVEL_* macros
 * expand to, and not the parenthesized values of the Essentials levels.
 */
#define VEEPORT_LOGGER_LEVEL_NONE           LOG_LEVEL_NONE
#define VEEPORT_LOGGER_LEVEL_ERR            LOG_LEVEL_ERR
#define VEEPORT_LOGGER_LEVEL_WRN            LOG_LEVEL_WRN
#define VEEPORT_LOGGER_LEVEL_INF            LOG_LEVEL_INF
#define VEEPORT_LOGGER_LEVEL_DBG            LOG_LEVEL_DBG

/**
 * @brief Verbosity of an abstraction layer, from the log level its Kconfig declares.
 *
 * @param suffix  The layer's Kconfig suffix, so FS for CONFIG_MICROEJ_FS_LOG_LEVEL.
 */
#define VEEPORT_LOGGER_MODULE_LEVEL(suffix) CONFIG_MICROEJ_ ## suffix ## _LOG_LEVEL

/*
 * A log module is registered in a single compilation unit and declared in the others, as required
 * by Zephyr. Both macros take the module name as an identifier, not as a string, because Zephyr
 * builds symbol names from it.
 *
 * They are variadic so that a single macro pairing a name with a level can be passed as one
 * argument, while a name and a level can still be passed separately. The trailing semicolon
 * belongs to the definitions: the Zephyr macros expand to a declaration that needs one, while the
 * Essentials logger is called without.
 */
#ifndef VEEPORT_LOGGER_DEFINE_MODULE
#define VEEPORT_LOGGER_DEFINE_MODULE(...)   LOG_MODULE_REGISTER(__VA_ARGS__);
#endif
#ifndef VEEPORT_LOGGER_DECLARE_MODULE
#define VEEPORT_LOGGER_DECLARE_MODULE(...)  LOG_MODULE_DECLARE(__VA_ARGS__);
#endif

#ifndef VEEPORT_LOGGER_LOG_ERR
#define VEEPORT_LOGGER_LOG_ERR(...)         LOG_ERR(__VA_ARGS__)
#endif
#ifndef VEEPORT_LOGGER_LOG_WRN
#define VEEPORT_LOGGER_LOG_WRN(...)         LOG_WRN(__VA_ARGS__)
#endif
#ifndef VEEPORT_LOGGER_LOG_INF
#define VEEPORT_LOGGER_LOG_INF(...)         LOG_INF(__VA_ARGS__)
#endif
#ifndef VEEPORT_LOGGER_LOG_DBG
#define VEEPORT_LOGGER_LOG_DBG(...)         LOG_DBG(__VA_ARGS__)
#endif

/*
 * The VEE Port Essentials sources name their log module with a string, which
 * LOG_MODULE_REGISTER() cannot take. Override the names with identifiers.
 */
#define VEEPORT_MAIN_MODULE_NAME            veeport_main
#define VEEPORT_ERRORS_MODULE_NAME          veeport_errors

#endif // VEEPORT_LOGGER_ZEPHYR_H
