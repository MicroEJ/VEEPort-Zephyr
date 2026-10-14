/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
 * Build: 7E4D1F7C
 */

/**
 * @file
 * @brief zephyr OS filesystem helper implementation.
 * @author @CCO_AUTHOR@
 * @version @CCO_VERSION@
 * @date @CCO_DATE@
 */

/* Includes ------------------------------------------------------------------*/

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "LLFS_impl.h"
#include "LLFS_File_impl.h"
#include "microej_async_worker.h"
#include "fs_helper.h"
#include "fs_configuration.h"
#include "fs_helper_configuration.h"

VEEPORT_LOGGER_DEFINE_MODULE(LLFS_LOG_MODULE)

/**
 * @brief Records a failed operation in the job. The exception is thrown on the VM thread, by
 * <code>LLFS_throw_job_error</code>.
 *
 * @param[in] params the job parameters, a struct with the result and error_code fields.
 * @param[in] zephyr_retcode the return value of the failed Zephyr call: a negative errno.
 */
#define FS_SET_ERROR(params, zephyr_retcode)          \
		do {                                          \
			(params)->result = LLFS_NOK;              \
			(params)->error_code = -(zephyr_retcode); \
		} while (0)

/**
 * Sanity check between the expected version of the configuration and the actual version of
 * the configuration.
 * If an error is raised here, it means that a new version of the CCO has been installed and
 * the configuration fs_configuration.h must be updated based on the one provided
 * by the new CCO version.
 */
#if FS_CONFIGURATION_VERSION != 1
  #error "Version of the configuration file fs_configuration.h is not compatible with this implementation."
#endif

#if (defined CONFIG_FILE_SYSTEM_FAT) || (defined CONFIG_FAT_FILESYSTEM_ELM)
#include <ff.h>

#ifdef CONFIG_MICROEJ_FS_MNT_POINT
#define FS_HELPER_MNT_POINT CONFIG_MICROEJ_FS_MNT_POINT
#else
#define FS_HELPER_MNT_POINT "/SD:"
#endif

#define FS_HELPER_FLASH_DEV     NULL
#define FS_HELPER_TYPE          FS_FATFS
#define FS_HELPER_NUM_FILES     CONFIG_FS_FATFS_NUM_FILES
#define FS_HELPER_NUM_DIRS      CONFIG_FS_FATFS_NUM_DIRS

static FATFS fs_data;
#elif (defined CONFIG_FILE_SYSTEM_LITTLEFS)
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>

#ifdef CONFIG_MICROEJ_FS_MNT_POINT
#define FS_HELPER_MNT_POINT CONFIG_MICROEJ_FS_MNT_POINT
#else
#define FS_HELPER_MNT_POINT "/lfs"
#endif

#define FS_HELPER_FLASH_DEV     PARTITION_ID(storage_partition)
#define FS_HELPER_TYPE          FS_LITTLEFS
#define FS_HELPER_NUM_FILES     CONFIG_FS_LITTLEFS_NUM_FILES
#define FS_HELPER_NUM_DIRS      CONFIG_FS_LITTLEFS_NUM_DIRS

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(fs_data);
#else
#error "Only FATFS or LITTLEFS are supported"
#endif

static struct fs_mount_t fs_mnt = {
	.type = FS_HELPER_TYPE,
	.fs_data = &fs_data,
	.storage_dev = (void *)FS_HELPER_FLASH_DEV,
	.mnt_point = FS_HELPER_MNT_POINT,
};

/* Memory pool for directory objects */
K_MEM_SLAB_DEFINE(fs_dirp_pool, sizeof(struct fs_dir_t),
                  FS_HELPER_NUM_DIRS, 4);

/* Memory pool for file objects */
K_MEM_SLAB_DEFINE(fs_filep_pool, sizeof(struct fs_file_t),
                  FS_HELPER_NUM_FILES, 4);

static int32_t FS_file_get_size(struct fs_file_t *fp, off_t *pos, off_t *size);

/**
 * @brief Tells whether a file or directory handle may be cast back to the object it designates.
 *
 * A handle is the address of an object allocated from one of the pools above, so a null address
 * designates nothing, and neither does any of the failure values an action reports in place of a
 * handle. Every other value is accepted: an address is not necessarily positive once cast to
 * <code>int32_t</code>, since a board may hold these pools in memory mapped above the 2 GB
 * boundary.
 *
 * @param[in] handle the handle received from the managed side.
 *
 * @return true when the handle designates an object, false otherwise.
 */
static bool FS_is_handle_valid(int32_t handle) {
	return ((0 != handle) &&
	        (LLFS_EOF != handle) &&
	        (LLFS_NOK != handle) &&
	        (LLFS_NOT_CREATED != handle));
}

static int32_t FS_file_get_size(struct fs_file_t *fp, off_t *pos, off_t *size) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	int32_t result = LLFS_NOK;
	int retcode = 0;
	*pos = fs_tell(fp);
	if (*pos >= 0) {
		retcode = fs_seek(fp, 0, SEEK_END);
		if (0 == retcode) {
			*size = fs_tell(fp);
			if (*size >= 0) {
				retcode = fs_seek(fp, *pos, SEEK_SET);
				if (0 == retcode) {
					result = LLFS_OK;
				} else {
				}
			} else {
			}
		} else {
		}
	} else {
	}

	VEEPORT_LOGGER_LOG_DBG("FS_file_get_size %d [%d]", (int)*size, retcode);
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);

	return result;
}

int LLFS_init(void) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	int retcode = 0;
	retcode = fs_mount(&fs_mnt);

	VEEPORT_LOGGER_LOG_DBG("fs_mount %s [%d]", FS_HELPER_MNT_POINT, retcode);

	if (0 == retcode) {
#if VEEPORT_LOGGER_MODULE_LEVEL(FS) >= VEEPORT_LOGGER_LEVEL_DBG
		// Report the occupancy of the freshly mounted volume: a filesystem left almost full by a
		// previous run makes later operations fail in ways that depend on the media, not on the code.
		// Gated because counting the free clusters costs a scan of the allocation table.
		struct fs_statvfs statfs = { 0 };
		int statvfs_retcode = fs_statvfs(FS_HELPER_MNT_POINT, &statfs);

		if (0 == statvfs_retcode) {
			VEEPORT_LOGGER_LOG_DBG("volume %s: %lu blocks of %lu bytes, %lu free", FS_HELPER_MNT_POINT,
			                       (unsigned long)statfs.f_blocks, (unsigned long)statfs.f_frsize,
			                       (unsigned long)statfs.f_bfree);
		} else {
			VEEPORT_LOGGER_LOG_DBG("fs_statvfs %s [%d]", FS_HELPER_MNT_POINT, statvfs_retcode);
		}
#endif // VEEPORT_LOGGER_MODULE_LEVEL(FS) >= VEEPORT_LOGGER_LEVEL_DBG
	} else {
		VEEPORT_ERRORS_throw_runtime_exception_message(RUNTIME_EXCEPTION_UNDEFINED, "fs_mount %s: %d",
		                                               FS_HELPER_MNT_POINT, retcode);
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);

	return retcode;
}

void LLFS_IMPL_get_last_modified_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_last_modified_t *params = (FS_last_modified_t *)job->params;

	params->result = LLFS_NOK;
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, 1);
}

void LLFS_IMPL_set_read_only_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;

	params->result = LLFS_NOK;
	VEEPORT_LOGGER_LOG_DBG("<< line %d unsupported", __LINE__);
}

void LLFS_IMPL_create_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_create_t *params = (FS_create_t *)job->params;

	struct fs_dirent entry;

	int retcode = fs_stat(params->path, &entry);

	VEEPORT_LOGGER_LOG_DBG("fs_stat %s [%d]", params->path, retcode);

	switch (retcode) {
	case 0:
		params->result = LLFS_NOT_CREATED;
		break;
	case -ENOENT:
	{
		params->result = LLFS_NOK;

		struct fs_file_t fp;

		fs_file_t_init(&fp);

		int retcode = fs_open(&fp, params->path, FS_O_CREATE);

		VEEPORT_LOGGER_LOG_DBG("fs_open %s [%d]", params->path, retcode);

		if (0 == retcode) {
			retcode = fs_close(&fp);

			VEEPORT_LOGGER_LOG_DBG("fs_close [%d]", retcode);

			if (0 == retcode) {
				params->result = LLFS_OK;
			} else {
				FS_SET_ERROR(params, retcode);
			}
		} else {
			FS_SET_ERROR(params, retcode);
		}
	}
	break;
	default:
		FS_SET_ERROR(params, retcode);
		break;
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_IMPL_open_directory_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;
	struct fs_dir_t *dp;

	int slab_retcode = k_mem_slab_alloc(&fs_dirp_pool, (void **)&dp, K_NO_WAIT);

	VEEPORT_LOGGER_LOG_DBG("k_mem_slab_alloc [%d]", slab_retcode);

	if (0 == slab_retcode) {
		fs_dir_t_init(dp);

		int retcode = fs_opendir(dp, params->path);

		VEEPORT_LOGGER_LOG_DBG("fs_opendir %s [%d]", params->path, retcode);

		if (0 == retcode) {
			params->result = (int32_t)dp;
		} else {
			params->result = LLFS_NOK;

			k_mem_slab_free(&fs_dirp_pool, (void *)dp);
		}
	} else {
		params->result = LLFS_NOK;
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, slab_retcode);
}

void LLFS_IMPL_read_directory_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_read_directory_t *params = (FS_read_directory_t *)job->params;
	struct fs_dirent entry;
	int retcode = -EBADF;

	if (FS_is_handle_valid(params->directory_ID)) {
		struct fs_dir_t *dp = (struct fs_dir_t *)params->directory_ID;

		retcode = fs_readdir(dp, &entry);

		VEEPORT_LOGGER_LOG_DBG("fs_readdir [%d]", retcode);

		if (0 != retcode) {
			params->result = LLFS_NOK;
		} else if (0 == entry.name[0]) {
			params->result = LLFS_EOF;
		} else {
			strcpy(params->path, entry.name);
			params->result = LLFS_OK;
		}
	} else {
		VEEPORT_LOGGER_LOG_DBG("invalid directory handle %d", params->directory_ID);

		params->result = LLFS_NOK;
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_IMPL_close_directory_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_close_directory_t *params = (FS_close_directory_t *)job->params;
	int retcode = -EBADF;

	if (FS_is_handle_valid(params->directory_ID)) {
		struct fs_dir_t *dp = (struct fs_dir_t *)params->directory_ID;

		retcode = fs_closedir(dp);

		VEEPORT_LOGGER_LOG_DBG("fs_closedir [%d]", retcode);

		// Reported as a success even when fs_closedir failed, because the block is released just
		// below: the caller lets go of the handle only on LLFS_OK, and reporting the failure would
		// leave it holding the address of a free block to close a second time. Nothing consumes
		// this failure anyway, the trace above being its only reader.
		params->result = LLFS_OK;

		k_mem_slab_free(&fs_dirp_pool, (void *)dp);
	} else {
		VEEPORT_LOGGER_LOG_DBG("invalid directory handle %d", params->directory_ID);

		params->result = LLFS_NOK;
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_IMPL_rename_to_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_rename_to_t *params = (FS_rename_to_t *)job->params;

	int retcode = fs_rename(params->path, params->new_path);

	VEEPORT_LOGGER_LOG_DBG("fs_rename %s -> %s [%d]", params->path, params->new_path, retcode);

	if (0 == retcode) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_IMPL_get_length_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_path64_operation_t *params = (FS_path64_operation_t *)job->params;
	struct fs_dirent entry;

	int retcode = fs_stat(params->path, &entry);

	VEEPORT_LOGGER_LOG_DBG("fs_stat %s [%d]", params->path, retcode);

	if (0 == retcode) {
		params->result = (int64_t)entry.size;
	} else {
		params->result = LLFS_NOK;
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_IMPL_exist_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;
	struct fs_dirent entry;

	int retcode = fs_stat(params->path, &entry);

	VEEPORT_LOGGER_LOG_DBG("fs_stat %s [%d]", params->path, retcode);

	if (0 == retcode) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_IMPL_get_space_size_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_get_space_size *params = (FS_get_space_size *)job->params;
	struct fs_statvfs statfs;

	int retcode = fs_statvfs(params->path, &statfs);

	VEEPORT_LOGGER_LOG_DBG("fs_statvfs %s [%d]", params->path, retcode);

	if (0 == retcode) {
		params->result = LLFS_OK;

		switch (params->space_type) {
		case LLFS_FREE_SPACE:
			params->result = ((int64_t)statfs.f_bfree) * ((int64_t)statfs.f_frsize);
			break;
		case LLFS_TOTAL_SPACE:
			params->result = ((int64_t)statfs.f_blocks) * ((int64_t)statfs.f_frsize);
			break;
		case LLFS_USABLE_SPACE:
			params->result = ((int64_t)(statfs.f_blocks - statfs.f_bfree)) * ((int64_t)statfs.f_frsize);
			break;
		default:
			params->result = LLFS_NOK;
			break;
		}
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_IMPL_make_directory_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;

	int retcode = fs_mkdir(params->path);

	VEEPORT_LOGGER_LOG_DBG("fs_mkdir %s [%d]", params->path, retcode);

	if (0 == retcode) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_IMPL_is_hidden_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;

	VEEPORT_LOGGER_LOG_DBG("file hidden %s", params->path);

	if (params->path[0] == '.') {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, params->result == LLFS_OK);
}

void LLFS_IMPL_is_directory_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;
	struct fs_dirent entry;

	int retcode = fs_stat(params->path, &entry);

	VEEPORT_LOGGER_LOG_DBG("fs_stat %s [%d]", params->path, retcode);

	if ((0 == retcode) && (FS_DIR_ENTRY_DIR == entry.type)) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_IMPL_is_file_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;
	struct fs_dirent entry;

	int retcode = fs_stat(params->path, &entry);

	VEEPORT_LOGGER_LOG_DBG("fs_stat %s [%d]", params->path, retcode);

	if ((0 == retcode) && (FS_DIR_ENTRY_FILE == entry.type)) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_IMPL_set_last_modified_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_last_modified_t *params = (FS_last_modified_t *)job->params;

	params->result = LLFS_NOK;
	VEEPORT_LOGGER_LOG_DBG("<< line %d unsupported", __LINE__);
}

void LLFS_IMPL_delete_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;

	int retcode = fs_unlink(params->path);

	VEEPORT_LOGGER_LOG_DBG("fs_unlink %s [%d]", params->path, retcode);

	if (0 == retcode) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_IMPL_is_accessible_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_is_accessible_t *params = (FS_is_accessible_t *)job->params;
	struct fs_dirent entry;

	int retcode = fs_stat(params->path, &entry);

	VEEPORT_LOGGER_LOG_DBG("fs_stat %s [%d]", params->path, retcode);

	if ((0 == retcode) && (FS_DIR_ENTRY_FILE == entry.type)) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_IMPL_set_permission_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_set_permission_t *params = (FS_set_permission_t *)job->params;

	params->result = LLFS_NOK;
	VEEPORT_LOGGER_LOG_DBG("<< line %d unsupported", __LINE__);
}

void LLFS_File_IMPL_open_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	int retcode = -EBADF;
	FS_open_t *params = (FS_open_t *)job->params;

	params->result = LLFS_OK;

	fs_mode_t flags = 0;
	switch (params->mode) {
	case LLFS_FILE_MODE_READ:
		flags = FS_O_READ;
		break;
	case LLFS_FILE_MODE_WRITE:
		flags = FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC;
		break;
	case LLFS_FILE_MODE_APPEND:
		flags = FS_O_CREATE | FS_O_APPEND | FS_O_WRITE;
		break;
	case LLFS_FILE_MODE_READ_WRITE:
		flags = FS_O_CREATE | FS_O_RDWR;
		break;
	case LLFS_FILE_MODE_READ_WRITE_DATA_SYNC:
	case LLFS_FILE_MODE_READ_WRITE_SYNC:
		FS_SET_ERROR(params, -EINVAL);
		break;
	default:
		FS_SET_ERROR(params, -EINVAL);
		break;
	}

	if (LLFS_OK == params->result) {
		struct fs_file_t *fp;

		int slab_retcode = k_mem_slab_alloc(&fs_filep_pool, (void **)&fp, K_NO_WAIT);

		VEEPORT_LOGGER_LOG_DBG("k_mem_slab_alloc %s [%d]", params->path, slab_retcode);

		if (0 == slab_retcode) {
			fs_file_t_init(fp);

			retcode = fs_open(fp, params->path, flags);

			VEEPORT_LOGGER_LOG_DBG("fs_open %s %c [%d]", params->path, params->mode, retcode);

			if (0 == retcode) {
				params->result = (int32_t)fp;
			} else {
				FS_SET_ERROR(params, retcode);

				k_mem_slab_free(&fs_filep_pool, (void *)fp);
			}
		} else {
			FS_SET_ERROR(params, slab_retcode);
		}
	} else {
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_File_IMPL_write_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_write_read_t *param = (FS_write_read_t *)job->params;
	ssize_t res = 0;

	if (FS_is_handle_valid(param->file_id)) {
		struct fs_file_t *fd = (struct fs_file_t *)param->file_id;
		uint8_t *data = param->data;
		int32_t length = param->length;

		VEEPORT_LOGGER_LOG_DBG(">>WRITE '%s' %d", data, length);

		res = fs_write(fd, (void *)data, length);
		if (res < 0) {
			FS_SET_ERROR(param, res);
		} else if (res == 0) {
			// If res is null, this must be due to a full disk
			FS_SET_ERROR(param, -ENOSPC);
		} else {
			param->result = (int32_t)res;
		}
	} else {
		FS_SET_ERROR(param, -EBADF);
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, res);
}

void LLFS_File_IMPL_read_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_write_read_t *param = (FS_write_read_t *)job->params;
	ssize_t res = 0;

	if (FS_is_handle_valid(param->file_id)) {
		struct fs_file_t *fd = (struct fs_file_t *)param->file_id;
		uint8_t *data = param->data;
		size_t length = (size_t)param->length;
		VEEPORT_LOGGER_LOG_DBG("line %u: reading %d bytes from file %p", __LINE__, length, fd);

		res = fs_read(fd, (void *)data, length);
		if (res < 0) {
			FS_SET_ERROR(param, res);
		} else {
			if (res == (unsigned int)0) {
				param->result = LLFS_EOF;
			} else {
				param->result = res;
			}
		}
	} else {
		FS_SET_ERROR(param, -EBADF);
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, res);
}

void LLFS_File_IMPL_close_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_close_t *params = (FS_close_t *)job->params;
	int retcode = -EBADF;

	if (FS_is_handle_valid(params->file_id)) {
		struct fs_file_t *fp = (struct fs_file_t *)params->file_id;

		retcode = fs_close(fp);

		VEEPORT_LOGGER_LOG_DBG("fs_close [%d]", retcode);

		if (0 == retcode) {
			params->result = LLFS_OK;
		} else {
			FS_SET_ERROR(params, retcode);
		}

		k_mem_slab_free(&fs_filep_pool, (void *)fp);
	} else {
		FS_SET_ERROR(params, -EBADF);
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_File_IMPL_seek_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_seek_t *params = (FS_seek_t *)job->params;
	off_t new_pos = (off_t)params->n;
	off_t cur_pos;
	off_t size;
	int32_t retcode = -EBADF;

	if (sizeof(off_t) == sizeof(int32_t)) {
		if (params->n > INT_MAX) {
			new_pos = INT_MAX;
		} else if (params->n < INT_MIN) {
			new_pos = INT_MIN;
		} else {
		}
	} else {
	}

	if (FS_is_handle_valid(params->file_id)) {
		struct fs_file_t *fp = (struct fs_file_t *)params->file_id;

		retcode = FS_file_get_size(fp, &cur_pos, &size);
		if (LLFS_OK == retcode) {
			if (new_pos > size) {
				new_pos = size;
			} else {
			}

			if (new_pos >= 0) {
				retcode = fs_seek(fp, new_pos, FS_SEEK_SET);

				VEEPORT_LOGGER_LOG_DBG("fs_seek %d [%d]", (int)new_pos, retcode);

				if (0 == retcode) {
					if (new_pos == size) {
						params->result = LLFS_EOF;
					} else {
						params->result = LLFS_OK;
					}
				} else {
					FS_SET_ERROR(params, retcode);
				}
			} else {
				FS_SET_ERROR(params, -EINVAL);
			}
		} else {
			FS_SET_ERROR(params, retcode);
		}
	} else {
		FS_SET_ERROR(params, -EBADF);
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_File_IMPL_get_file_pointer_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_getfp_t *params = (FS_getfp_t *)job->params;
	off_t pos = -EBADF;

	if (FS_is_handle_valid(params->file_id)) {
		struct fs_file_t *fp = (struct fs_file_t *)params->file_id;

		pos = fs_tell(fp);

		if (pos >= 0) {
			params->result = (int64_t)pos;
		} else {
			FS_SET_ERROR(params, pos);
		}
	} else {
		FS_SET_ERROR(params, -EBADF);
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %ld", __LINE__, pos);
}

void LLFS_File_IMPL_set_length_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_set_length_t *params = (FS_set_length_t *)job->params;
	off_t len = (off_t)params->length;

	if (FS_is_handle_valid(params->file_id)) {
		struct fs_file_t *fp = (struct fs_file_t *)params->file_id;

		// Check for file permision to see if the file has write permission
		if ((fp->flags & FS_O_WRITE) == 0) {
			FS_SET_ERROR(params, -EINVAL);
		} else {
			off_t current_pos = fs_tell(fp);
			if (current_pos < 0) {
				FS_SET_ERROR(params, current_pos);
			} else {
				int retcode = fs_truncate(fp, len);
				VEEPORT_LOGGER_LOG_DBG("fs_truncate [%d]", retcode);

				if (0 == retcode) {
					retcode = fs_seek(fp, current_pos > len ? len : current_pos, FS_SEEK_SET);
					if (0 == retcode) {
						params->result = LLFS_OK;
					} else {
						FS_SET_ERROR(params, retcode);
					}
				} else {
					FS_SET_ERROR(params, retcode);
				}
			}
		}
	} else {
		FS_SET_ERROR(params, -EBADF);
	}
	VEEPORT_LOGGER_LOG_DBG("<< ret: %d", params->result);
}

void LLFS_File_IMPL_get_length_with_fd_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_get_length_with_fd_t *params = (FS_get_length_with_fd_t *)job->params;
	off_t cur_pos;
	off_t size;
	int32_t retcode = -EBADF;

	if (FS_is_handle_valid(params->file_id)) {
		struct fs_file_t *fp = (struct fs_file_t *)params->file_id;

		retcode = FS_file_get_size(fp, &cur_pos, &size);
		if (LLFS_OK == retcode) {
			params->result = (int64_t)size;
		} else {
			FS_SET_ERROR(params, retcode);
		}
	} else {
		FS_SET_ERROR(params, -EBADF);
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_File_IMPL_available_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_available_t *params = (FS_available_t *)job->params;
	off_t cur_pos;
	off_t size;
	int32_t retcode = -EBADF;

	if (FS_is_handle_valid(params->file_id)) {
		struct fs_file_t *fp = (struct fs_file_t *)params->file_id;

		retcode = FS_file_get_size(fp, &cur_pos, &size);
		if (LLFS_OK == retcode) {
			params->result = (int64_t)(size - cur_pos);
		} else {
			FS_SET_ERROR(params, retcode);
		}
	} else {
		FS_SET_ERROR(params, -EBADF);
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

void LLFS_File_IMPL_flush_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	FS_flush_t *params = (FS_flush_t *)job->params;
	int retcode = -EBADF;

	if (FS_is_handle_valid(params->file_id)) {
		struct fs_file_t *fp = (struct fs_file_t *)params->file_id;

		retcode = fs_sync(fp);

		VEEPORT_LOGGER_LOG_DBG("fs_sync [%d]", retcode);

		if (0 == retcode) {
			params->result = LLFS_OK;
		} else {
			FS_SET_ERROR(params, retcode);
		}
	} else {
		VEEPORT_LOGGER_LOG_DBG("invalid file handle %d", params->file_id);

		FS_SET_ERROR(params, -EBADF);
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
}

int32_t LLFS_copy_path_param(uint8_t *path, uint8_t *path_param) {
	VEEPORT_LOGGER_LOG_DBG(">> line %d", __LINE__);
	int32_t retcode;

	strcpy(path_param, FS_HELPER_MNT_POINT);

	int32_t path_len = strlen(path);
	int32_t path_param_len = strlen(path_param);
	VEEPORT_LOGGER_LOG_DBG("path: %s path_param: %s", path, path_param);
	if (path_len > (FS_PATH_LENGTH - path_param_len - 1)) {
		VEEPORT_LOGGER_LOG_DBG("path length %d is too long", path_len);
		retcode = LLFS_NOK;
	} else {
		memcpy(&path_param[path_param_len], path, path_len);
		path_param[path_len + path_param_len] = 0;
		VEEPORT_LOGGER_LOG_DBG("path_param: %s", path_param);

		retcode = LLFS_OK;
	}
	VEEPORT_LOGGER_LOG_DBG("<< line %d ret %d", __LINE__, retcode);
	return retcode;
}

void LLFS_throw_job_error(int32_t fs_errno) {
	if (0 != fs_errno) {
		VEEPORT_ERRORS_throw_caught_exception_message(LLFS_NOK, "errno %d: %s", fs_errno, strerror(fs_errno));
	} else {
		VEEPORT_ERRORS_throw_caught_exception(LLFS_NOK);
	}
}
