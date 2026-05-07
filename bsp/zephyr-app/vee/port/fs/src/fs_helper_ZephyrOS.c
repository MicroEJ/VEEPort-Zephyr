/*
 * Copyright 2026 MicroEJ Corp. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be found with this software.
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

#ifdef CONFIG_MICROEJ_FS_MNT_POINT
#define FS_HELPER_MNT_POINT CONFIG_MICROEJ_FS_MNT_POINT
#else
#define FS_HELPER_MNT_POINT "/lfs"
#endif

#define FS_HELPER_FLASH_DEV     FIXED_PARTITION_ID(storage_partition)
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

static int32_t FS_file_get_size(struct fs_file_t *fp, off_t *pos, off_t *size) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
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

	LLFS_DEBUG_TRACE("FS_file_get_size %d [%d]\n", (int)*size, retcode);
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);

	return result;
}

int LLFS_init(void) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	int retcode = 0;
	retcode = fs_mount(&fs_mnt);

	LLFS_DEBUG_TRACE("fs_mount [%d]\n", retcode);

	if (0 == retcode) {
	} else {
		SNI_throwNativeException(retcode, "fs_mount");
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);

	return retcode;
}

void LLFS_IMPL_get_last_modified_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_last_modified_t *params = (FS_last_modified_t *)job->params;

	params->result = LLFS_NOK;
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, 1);
}

void LLFS_IMPL_set_read_only_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;

	params->result = LLFS_NOK;
}

void LLFS_IMPL_create_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_create_t *params = (FS_create_t *)job->params;

	struct fs_dirent entry;

	int retcode = fs_stat(params->path, &entry);

	LLFS_DEBUG_TRACE("fs_stat %s [%d]\n", params->path, retcode);

	switch (retcode) {
	case 0:
		params->result = LLFS_NOT_CREATED;
		break;
	case -ENOENT:
	{
		params->result = LLFS_NOK;

		struct fs_file_t fp;

		params->result = LLFS_NOK;
		params->error_message = NULL;

		fs_file_t_init(&fp);

		int retcode = fs_open(&fp, params->path, FS_O_CREATE);

		LLFS_DEBUG_TRACE("fs_open %s [%d]\n", params->path, retcode);

		if (0 == retcode) {
			retcode = fs_close(&fp);

			LLFS_DEBUG_TRACE("fs_close [%d]\n", retcode);

			if (0 == retcode) {
				params->result = LLFS_OK;
			} else {
				params->error_code = retcode;
				params->error_message = "fs_close";
			}
		} else {
			params->error_code = retcode;
			params->error_message = "fs_open";
		}
	}
	break;
	default:
		params->result = LLFS_NOK;
		params->error_code = retcode;
		params->error_message = "fs_stat";
		break;
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_IMPL_open_directory_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;
	struct fs_dir_t *dp;

	int slab_retcode = k_mem_slab_alloc(&fs_dirp_pool, (void **)&dp, K_NO_WAIT);

	LLFS_DEBUG_TRACE("k_mem_slab_alloc [%d]\n", slab_retcode);

	if (0 == slab_retcode) {
		fs_dir_t_init(dp);

		int retcode = fs_opendir(dp, params->path);

		LLFS_DEBUG_TRACE("fs_opendir %s [%d]\n", params->path, retcode);

		if (0 == retcode) {
			params->result = (int32_t)dp;
		} else {
			params->result = LLFS_NOK;

			k_mem_slab_free(&fs_dirp_pool, (void *)dp);
		}
	} else {
		params->result = LLFS_NOK;
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, slab_retcode);
}

void LLFS_IMPL_read_directory_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_read_directory_t *params = (FS_read_directory_t *)job->params;
	struct fs_dir_t *dp = (struct fs_dir_t *)params->directory_ID;
	struct fs_dirent entry;

	int retcode = fs_readdir(dp, &entry);

	LLFS_DEBUG_TRACE("fs_readdir [%d]\n", retcode);

	if ((0 == retcode) && (entry.name[0] != 0)) {
		strcpy(params->path, entry.name);
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_IMPL_close_directory_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_close_directory_t *params = (FS_close_directory_t *)job->params;
	struct fs_dir_t *dp = (struct fs_dir_t *)params->directory_ID;

	int retcode = fs_closedir(dp);

	LLFS_DEBUG_TRACE("fs_closedir [%d]\n", retcode);

	if (0 == retcode) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}

	k_mem_slab_free(&fs_dirp_pool, (void *)dp);
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_IMPL_rename_to_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_rename_to_t *params = (FS_rename_to_t *)job->params;

	int retcode = fs_rename(params->path, params->new_path);

	LLFS_DEBUG_TRACE("fs_rename %s -> %s [%d]\n", params->path, params->new_path, retcode);

	if (0 == retcode) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_IMPL_get_length_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_path64_operation_t *params = (FS_path64_operation_t *)job->params;
	struct fs_dirent entry;

	int retcode = fs_stat(params->path, &entry);

	LLFS_DEBUG_TRACE("fs_stat %s [%d]\n", params->path, retcode);

	if (0 == retcode) {
		params->result = (int64_t)entry.size;
	} else {
		params->result = LLFS_NOK;
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_IMPL_exist_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;
	struct fs_dirent entry;

	int retcode = fs_stat(params->path, &entry);

	LLFS_DEBUG_TRACE("fs_stat %s [%d]\n", params->path, retcode);

	if (0 == retcode) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_IMPL_get_space_size_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_get_space_size *params = (FS_get_space_size *)job->params;
	struct fs_statvfs statfs;

	int retcode = fs_statvfs(params->path, &statfs);

	LLFS_DEBUG_TRACE("fs_statvfs %s [%d]\n", params->path, retcode);

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
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_IMPL_make_directory_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;

	int retcode = fs_mkdir(params->path);

	LLFS_DEBUG_TRACE("fs_mkdir %s [%d]\n", params->path, retcode);

	if (0 == retcode) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_IMPL_is_hidden_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;

	LLFS_DEBUG_TRACE("file hidden %s\n", params->path);

	if (params->path[0] == '.') {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, params->result == LLFS_OK);
}

void LLFS_IMPL_is_directory_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;
	struct fs_dirent entry;

	int retcode = fs_stat(params->path, &entry);

	LLFS_DEBUG_TRACE("fs_stat %s [%d]\n", params->path, retcode);

	if ((0 == retcode) && (FS_DIR_ENTRY_DIR == entry.type)) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_IMPL_is_file_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;
	struct fs_dirent entry;

	int retcode = fs_stat(params->path, &entry);

	LLFS_DEBUG_TRACE("fs_stat %s [%d]\n", params->path, retcode);

	if ((0 == retcode) && (FS_DIR_ENTRY_FILE == entry.type)) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_IMPL_set_last_modified_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_last_modified_t *params = (FS_last_modified_t *)job->params;

	params->result = LLFS_NOK;
}

void LLFS_IMPL_delete_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_path_operation_t *params = (FS_path_operation_t *)job->params;

	int retcode = fs_unlink(params->path);

	LLFS_DEBUG_TRACE("fs_unlink %s [%d]\n", params->path, retcode);

	if (0 == retcode) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_IMPL_is_accessible_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_is_accessible_t *params = (FS_is_accessible_t *)job->params;
	struct fs_dirent entry;

	int retcode = fs_stat(params->path, &entry);

	LLFS_DEBUG_TRACE("fs_stat %s [%d]\n", params->path, retcode);

	if ((0 == retcode) && (FS_DIR_ENTRY_FILE == entry.type)) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
	}
}

void LLFS_IMPL_set_permission_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_set_permission_t *params = (FS_set_permission_t *)job->params;

	params->result = LLFS_NOK;
}

void LLFS_File_IMPL_open_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	int retcode;
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
		params->result = LLFS_NOK;
		params->error_code = params->mode;
		params->error_message = "Unsupported opening mode";
		break;
	default:
		params->result = LLFS_NOK;
		params->error_code = params->mode;
		params->error_message = "Invalid opening mode";
		break;
	}

	if (LLFS_OK == params->result) {
		struct fs_file_t *fp;

		int slab_retcode = k_mem_slab_alloc(&fs_filep_pool, (void **)&fp, K_NO_WAIT);

		LLFS_DEBUG_TRACE("k_mem_slab_alloc %s [%d]\n", params->path, slab_retcode);

		if (0 == slab_retcode) {
			fs_file_t_init(fp);

			retcode = fs_open(fp, params->path, flags);

			LLFS_DEBUG_TRACE("fs_open %s %c [%d]\n", params->path, params->mode, slab_retcode);

			if (0 == retcode) {
				params->result = (int32_t)fp;
			} else {
				params->result = LLFS_NOK;
				params->error_code = retcode;
				params->error_message = "fs_open";

				k_mem_slab_free(&fs_filep_pool, (void *)fp);
			}
		} else {
			params->result = LLFS_NOK;
			params->error_code = slab_retcode;
			params->error_message = "k_mem_slab_alloc";
		}
	} else {
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_File_IMPL_write_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_write_read_t *param = (FS_write_read_t *)job->params;
	ssize_t res = 0;

	struct fs_file_t *fd = (struct fs_file_t *)param->file_id;
	uint8_t *data = param->data;
	int32_t length = param->length;

	LLFS_DEBUG_TRACE(">>WRITE '%s' %d\n", data, length);

	res = fs_write(fd, (void *)data, length);
	if (res < 0) {
		param->result = LLFS_NOK;
		param->error_code = res;
		param->error_message = "f_write failed";
	} else if (res == 0) {
		// If res is null, this must be due to a full disk
		param->result = LLFS_NOK;
		param->error_code = -ENOSPC;     /* No space left on device */
		param->error_message = "f_write failed: Disk is full!";
	} else {
		param->result = (int32_t)res;
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, res);
}

void LLFS_File_IMPL_read_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_write_read_t *param = (FS_write_read_t *)job->params;
	ssize_t res = 0;

	struct fs_file_t *fd = (struct fs_file_t *)param->file_id;
	uint8_t *data = param->data;
	size_t length = (size_t)param->length;
	LLFS_DEBUG_TRACE("[%s:%u] reading %d bytes from file %p\n", __func__, __LINE__, length, fd);

	res = fs_read(fd, (void *)data, length);
	if (res < 0) {
		param->result = LLFS_NOK;
		param->error_code = res;
		param->error_message = "f_read failed";
	} else {
		if (res == (unsigned int)0) {
			param->result = LLFS_EOF;
		} else {
			param->result = res;
		}
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, res);
}

void LLFS_File_IMPL_close_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_close_t *params = (FS_close_t *)job->params;
	struct fs_file_t *fp = (struct fs_file_t *)params->file_id;

	int retcode = fs_close(fp);

	LLFS_DEBUG_TRACE("fs_close [%d]\n", retcode);

	if (0 == retcode) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
		params->error_code = retcode;
		params->error_message = "fs_close";
	}

	k_mem_slab_free(&fs_filep_pool, (void *)fp);
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_File_IMPL_seek_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_seek_t *params = (FS_seek_t *)job->params;
	struct fs_file_t *fp = (struct fs_file_t *)params->file_id;
	off_t new_pos = (off_t)params->n;
	off_t cur_pos;
	off_t size;

	if (sizeof(off_t) == sizeof(int32_t)) {
		if (params->n > INT_MAX) {
			new_pos = INT_MAX;
		} else if (params->n < INT_MIN) {
			new_pos = INT_MIN;
		} else {
		}
	} else {
	}

	int32_t retcode = FS_file_get_size(fp, &cur_pos, &size);
	if (LLFS_OK == retcode) {
		if (new_pos > size) {
			new_pos = size;
		} else {
		}

		if (new_pos >= 0) {
			retcode = fs_seek(fp, new_pos, FS_SEEK_SET);

			LLFS_DEBUG_TRACE("fs_seek %d [%d]\n", (int)new_pos, retcode);

			if (0 == retcode) {
				if (new_pos == size) {
					params->result = LLFS_EOF;
				} else {
					params->result = LLFS_OK;
				}
			} else {
				params->result = LLFS_NOK;
				params->error_code = retcode;
				params->error_message = "fs_seek";
			}
		} else {
			params->result = LLFS_NOK;
			params->error_code = new_pos;
			params->error_message = "fs_seek negative position";
		}
	} else {
		params->result = LLFS_NOK;
		params->error_code = retcode;
		params->error_message = "fs_file_get_size";
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_File_IMPL_get_file_pointer_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_getfp_t *params = (FS_getfp_t *)job->params;
	struct fs_file_t *fp = (struct fs_file_t *)params->file_id;

	off_t pos = fs_tell(fp);

	if (pos >= 0) {
		params->result = (int64_t)pos;
	} else {
		params->result = LLFS_NOK;
		params->error_code = pos;
		params->error_message = "fs_tell";
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %ld\n", __func__, __LINE__, pos);
}

void LLFS_File_IMPL_set_length_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_set_length_t *params = (FS_set_length_t *)job->params;
	struct fs_file_t *fp = (struct fs_file_t *)params->file_id;
	off_t len = (off_t)params->length;

	// Check for file permision to see if the file has write permission
	if ((fp->flags & FS_O_WRITE) == 0) {
		params->result = LLFS_NOK;
		params->error_code = 0;
		params->error_message = "Cannot change length of read-only file";
	} else {
		off_t current_pos = fs_tell(fp);
		if (current_pos < 0) {
			params->result = LLFS_NOK;
			params->error_code = current_pos;
			params->error_message = "fs_tell failed";
		} else {
			int retcode = fs_truncate(fp, len);
			LLFS_DEBUG_TRACE("fs_truncate [%d]\n", retcode);

			if (0 == retcode) {
				retcode = fs_seek(fp, current_pos > len ? len : current_pos, FS_SEEK_SET);
				if (0 == retcode) {
					params->result = LLFS_OK;
				} else {
					params->result = LLFS_NOK;
					params->error_code = retcode;
					params->error_message = "fs_seek";
				}
			} else {
				params->result = LLFS_NOK;
				params->error_code = retcode;
				params->error_message = "fs_tell";
			}
		}
	}
	LLFS_DEBUG_TRACE("<< %s ret: %d\n", __func__, params->result);
}

void LLFS_File_IMPL_get_length_with_fd_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_get_length_with_fd_t *params = (FS_get_length_with_fd_t *)job->params;
	struct fs_file_t *fp = (struct fs_file_t *)params->file_id;
	off_t cur_pos;
	off_t size;

	int32_t retcode = FS_file_get_size(fp, &cur_pos, &size);
	if (LLFS_OK == retcode) {
		params->result = (int64_t)size;
	} else {
		params->result = LLFS_NOK;
		params->error_code = retcode;
		params->error_message = "fs_file_get_size";
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_File_IMPL_available_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_available_t *params = (FS_available_t *)job->params;
	struct fs_file_t *fp = (struct fs_file_t *)params->file_id;
	off_t cur_pos;
	off_t size;

	int32_t retcode = FS_file_get_size(fp, &cur_pos, &size);
	if (LLFS_OK == retcode) {
		params->result = (int64_t)(size - cur_pos);
	} else {
		params->result = LLFS_NOK;
		params->error_code = retcode;
		params->error_message = "fs_file_get_size";
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
}

void LLFS_File_IMPL_flush_action(MICROEJ_ASYNC_WORKER_job_t *job) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	FS_flush_t *params = (FS_flush_t *)job->params;
	struct fs_file_t *fp = (struct fs_file_t *)params->file_id;

	int retcode = fs_sync(fp);

	LLFS_DEBUG_TRACE("fs_sync [%d]\n", retcode);

	if (0 == retcode) {
		params->result = LLFS_OK;
	} else {
		params->result = LLFS_NOK;
		params->error_code = retcode;
		params->error_message = "fs_flush";
	}
}

int32_t LLFS_set_path_param(uint8_t *path, uint8_t *path_param) {
	LLFS_DEBUG_TRACE(">> %s +%d\n", __func__, __LINE__);
	int32_t retcode;

	strcpy(path_param, FS_HELPER_MNT_POINT);

	int32_t path_len = strlen(path);
	int32_t path_param_len = strlen(path_param);
	LLFS_DEBUG_TRACE("path: %s path_param: %s\n", path, path_param);
	if (path_len > (FS_PATH_LENGTH - path_param_len - 1)) {
		SNI_throwNativeException(LLFS_NOK, "Path length is too long");

		retcode = LLFS_NOK;
	} else {
		memcpy(&path_param[path_param_len], path, path_len);
		path_param[path_len + path_param_len] = 0;
		LLFS_DEBUG_TRACE("path_param: %s\n", path_param);

		retcode = LLFS_OK;
	}
	LLFS_DEBUG_TRACE("<< %s +%d ret %d\n", __func__, __LINE__, retcode);
	return retcode;
}
