// Copyright 2026 MicroEJ Corp. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found with this software.

/*
 * C
 *
 * Copyright 2021-2025 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 */

/**
 * @file
 * @brief zephyr OS filesystem helper configuration header.
 * @author @CCO_AUTHOR@
 * @version @CCO_VERSION@
 * @date @CCO_DATE@
 */

#ifndef  HELPER_FS_CONFIGURATION_H
#define  HELPER_FS_CONFIGURATION_H

#include <zephyr/fs/fs.h>

/**
 * @brief Macros defining the different file system types/locations.
 */
#define MICROEJ_FS_LITTLEFS_FLASH       (0)
#define MICROEJ_FS_FATFS_FLASH          (1)
#define MICROEJ_FS_FATFS_SDMMC          (2)
#define MICROEJ_FS_FATFS_RAM            (3)

/**
 * Initializes the file system.
 *
 * @return fs error code.
 */
int LLFS_init(void);

#endif
