/*******************************************************************************
 * tefkernel - desktop_runtime_core
 * Copyright (C) 2026 eternalfuture-e38299
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2026/1/3
 *******************************************************************************/

#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "internal/tefplugin/tef_core_imp.h"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir(path, mode) _mkdir(path)
#define PATH_MAX MAX_PATH
#define SEPARATOR '\\'
#define SEPARATOR_STR "\\"
#else
#define SEPARATOR '/'
#define SEPARATOR_STR "/"
#endif


#include "tef_api.h"
#include "internal/log.h"

#include "internal/modloader/modloader_core_imp.h"
#include "memdl/memdl.h"

static const char* work_path = "./tefkernel";
static tef_vector_t modloaders; // ml_handle_t *

static int load_file_from_path(const char* file_path, ml_handle_t **loader, const char* private_dir) {
    TEKLOG_DEBUG("Loading modloader from file: %s", file_path);
    TEKLOG_DEBUG("Private directory: %s", private_dir);

    memdl_handle_t handle = memdl_open_file(file_path, MEMDL_NOW);
    if (!handle) {
        const char* error = memdl_error();
        TEKLOG_ERROR("Failed to open file with memdl: %s, error: %s",
                     file_path, error ? error : "Unknown error");
        return -1;
    }

    TEKLOG_DEBUG("File opened successfully, handle: %p", handle);

    if (!tefkernel_load_ml(handle, NULL, private_dir, loader)) {
        TEKLOG_ERROR("Failed to load modloader from file: %s", file_path);
        memdl_close(handle);
        return -2;
    }

    TEKLOG_INFO("Successfully loaded modloader from: %s", file_path);
    return 0;
}

static int load_tefklib_from_folder(const char* folder_path) {
    TEKLOG_DEBUG("Scanning folder for .tefklib files: %s", folder_path);

    DIR *dir = opendir(folder_path);
    if (!dir) {
        TEKLOG_ERROR("Failed to open folder: %s, error: %s",
                     folder_path, strerror(errno));
        return 0;
    }

    struct dirent *entry;
    int loaded_count = 0;
    int total_files = 0;
    int tefklib_files = 0;

    while ((entry = readdir(dir)) != NULL) {
        total_files++;

        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 构建完整路径
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s%c%s",
                 folder_path, SEPARATOR, entry->d_name);

        TEKLOG_TRACE("Checking entry: %s", full_path);

        // 检查文件类型
        struct stat st;
        if (stat(full_path, &st) < 0) {
            TEKLOG_WARN("Failed to stat file: %s, error: %s",
                        full_path, strerror(errno));
            continue;
        }

        // 只处理普通文件
        if (!S_ISREG(st.st_mode)) {
            TEKLOG_DEBUG("Skipping non-regular file: %s (mode: 0%o)",
                         entry->d_name, st.st_mode);
            continue;
        }

        // 检查文件后缀是否为 .tefklib
        const char* ext = strrchr(entry->d_name, '.');
        if (!ext) {
            TEKLOG_DEBUG("Skipping file without extension: %s", entry->d_name);
            continue;
        }

        if (strcmp(ext, ".tefklib") != 0) {
            TEKLOG_DEBUG("Skipping non-tefklib file: %s", entry->d_name);
            continue;
        }

        tefklib_files++;
        TEKLOG_INFO("Found tefklib file: %s (size: %ld bytes)",
                    entry->d_name, st.st_size);

        // 加载文件，私有目录就是当前文件夹
        ml_handle_t *loader = NULL;
        const int result = load_file_from_path(full_path, &loader, folder_path);

        if (result == 0 && loader != NULL) {
            if (tefstd_vector_push_back(&modloaders, &loader)) {
                TEKLOG_INFO("Modloader added to vector: %s (from folder: %s)",
                            entry->d_name, folder_path);
                loaded_count++;
            } else {
                TEKLOG_ERROR("Failed to add modloader to vector: %s", entry->d_name);
            }
        } else {
            TEKLOG_ERROR("Failed to load modloader from file: %s (result: %d)",
                         entry->d_name, result);
        }
    }

    closedir(dir);

    TEKLOG_DEBUG("Folder scan completed: %s", folder_path);
    TEKLOG_DEBUG("Statistics: total files: %d, tefklib files: %d, loaded: %d",
                 total_files, tefklib_files, loaded_count);

    return loaded_count;
}

static int traverse_modloader_folders(const char* modloaders_path) {
    TEKLOG_DEBUG("Traversing modloader folders in: %s", modloaders_path);

    DIR *dir = opendir(modloaders_path);
    if (!dir) {
        TEKLOG_ERROR("Failed to open modloaders directory: %s, error: %s",
                     modloaders_path, strerror(errno));
        return 0;
    }

    struct dirent *entry;
    int total_loaded = 0;
    int total_folders = 0;
    int processed_folders = 0;

    TEKLOG_INFO("Scanning modloaders directory: %s", modloaders_path);

    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 构建完整路径
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s%c%s",
                 modloaders_path, SEPARATOR, entry->d_name);

        total_folders++;
        TEKLOG_DEBUG("Checking entry: %s", full_path);

        // 检查是否为目录
        struct stat st;
        if (stat(full_path, &st) < 0) {
            TEKLOG_WARN("Failed to stat entry: %s, error: %s",
                        full_path, strerror(errno));
            continue;
        }

        if (!S_ISDIR(st.st_mode)) {
            TEKLOG_DEBUG("Skipping non-directory: %s", entry->d_name);
            continue;
        }

        processed_folders++;
        TEKLOG_INFO("Found modloader folder: %s", entry->d_name);

        // 加载这个文件夹中的.tefklib文件
        int loaded_in_folder = load_tefklib_from_folder(full_path);
        total_loaded += loaded_in_folder;

        if (loaded_in_folder > 0) {
            TEKLOG_INFO("Loaded %d tefklib file(s) from modloader folder: %s",
                        loaded_in_folder, entry->d_name);
        } else {
            TEKLOG_WARN("No tefklib files found in modloader folder: %s", entry->d_name);
        }
    }

    closedir(dir);

    TEKLOG_INFO("=== Modloader folders traversal completed ===");
    TEKLOG_INFO("Modloaders directory: %s", modloaders_path);
    TEKLOG_INFO("Total entries: %d, directories: %d, processed: %d",
                total_folders, processed_folders, processed_folders);
    TEKLOG_INFO("Total modloaders loaded: %d", total_loaded);

    return total_loaded;
}

API_EXPORT int init_tefkernel() {
    // Initialize logging system first
    tefkernel_log_init("tefkernel.log");

    tefstd_vector_init(&modloaders, sizeof(ml_handle_t *));

    char modloaders_path[PATH_MAX];
    snprintf(modloaders_path, sizeof(modloaders_path), "%s%c%s",
             work_path, SEPARATOR, "modloaders");

    traverse_modloader_folders(modloaders_path);
    tpf_initialize_all_plugins();

    for (int i = 0; i < tefstd_vector_size(&modloaders); ++i) {
        const ml_handle_t *loader = *(ml_handle_t **)tefstd_vector_at(&modloaders, i);

        loader->ml_entry->ops->initialize(loader->ml_entry);
        loader->ml_entry->ops->load_mods(loader->ml_entry);
        loader->ml_entry->ops->initialize_mods(loader->ml_entry);
    }

    /*
    ml_handle_t* entry;
    void* ml_handle = dlopen("/home/eternalfuture/CLionProjects/TerraLua/cmake-build-debug/libTerraLua.so", RTLD_LAZY);
    tefkernel_load_ml(ml_handle, NULL, "/home/eternalfuture/CLionProjects/TerraLua/test", &entry);

    if (!tpf_initialize_all_plugins()) {
        TEKLOG_ERROR("Initialize Plugins Failed");
    };

    entry->ml_entry->ops->initialize(entry->ml_entry);
    entry->ml_entry->ops->load_mods(entry->ml_entry);
    entry->ml_entry->ops->initialize_mods(entry->ml_entry);
    entry->ml_entry->ops->unload_mods(entry->ml_entry);
    entry->ml_entry->ops->shutdown(entry->ml_entry);
    */

    return 0;
}
