/*******************************************************************************
 * tefkernel - modloader_core
 * Copyright (C) 2025 eternalfuture-e38299
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
 * Created: 2025/12/20
 *******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "internal/log.h"
#include "internal/modloader/modloader_core_imp.h"
#include "internal/tefplugin/tef_core_imp.h"
#include "memdl/memdl.h"
#include "tefpackage/tefpkg.h"

struct {
    tef_vector_t handle; ///< 句柄(ml_handle_t*)
    bool initialized;    ///< 初始化状态
} g_ml_list;

static void free_modloader(ml_handle_t *ml_handle) {
    if (!ml_handle) return;

    TEKLOG_DEBUG("Freeing modloader handle %p", (void*)ml_handle);

    // 先卸载所有模组，然后关闭模组加载器
    if (ml_handle->ml_entry && ml_handle->ml_entry->ops) {
        if (ml_handle->ml_entry->ops->unload_mods) {
            TEKLOG_DEBUG("Unloading all mods from modloader");
            ml_handle->ml_entry->ops->unload_mods(ml_handle->ml_entry);
        }

        if (ml_handle->ml_entry->ops->shutdown) {
            TEKLOG_DEBUG("Shutting down modloader");
            ml_handle->ml_entry->ops->shutdown(ml_handle->ml_entry);
        }
    }

    // 关闭包句柄
    if (ml_handle->ml_entry && ml_handle->ml_entry->pkg_handle) {
        TEKLOG_DEBUG("Closing package handle");
        tefpkg_close(ml_handle->ml_entry->pkg_handle);
        ml_handle->ml_entry->pkg_handle = NULL;
    }

    // 关闭动态库句柄
    if (ml_handle->handle) {
        TEKLOG_DEBUG("Closing dynamic library handle");
        memdl_close(ml_handle->handle);
        ml_handle->handle = NULL;
    }

    // 释放分配的内存
    if (ml_handle->ml_entry) {
        free((void*)ml_handle->ml_entry->private_dir);
        free(ml_handle->ml_entry);
        ml_handle->ml_entry = NULL;
    }

    free(ml_handle);
}

bool tefkernel_load_ml(void *handle, tefpkg_handle_t* pkg_handle, const char* private_dir, ml_handle_t **out_ml) {
    if (!handle || !out_ml) {
        TEKLOG_ERROR("Invalid handle provided to tefkernel_load_ml");
        return false;
    }

    if (!g_ml_list.initialized) {
        TEKLOG_DEBUG("Initializing modloader handles vector");
        tefstd_vector_init(&g_ml_list.handle, sizeof(ml_handle_t*));
        g_ml_list.initialized = true;
    }

    ml_ops_t* (*create_method)() = memdl_sym(handle, "ml_create");
    if (!create_method) {
        TEKLOG_ERROR("Failed to find ml_create symbol in modloader");
        memdl_close(handle);
        return false;
    }

    ml_ops_t* ml_ops = create_method();
    if (!ml_ops) {
        TEKLOG_ERROR("Failed to create modloader instance");
        memdl_close(handle);
        return false;
    }

    ml_handle_t* new_ml = malloc(sizeof(ml_handle_t));
    if (!new_ml) {
        TEKLOG_ERROR("Failed to allocate memory for modloader handle");
        memdl_close(handle);
        return false;
    }

    ml_entry_t* new_ml_entry = malloc(sizeof(ml_entry_t));
    if (!new_ml_entry) {
        TEKLOG_ERROR("Failed to allocate memory for modloader entry");
        free(new_ml);
        memdl_close(handle);
        return false;
    }

    char* private_dir_copy = NULL;
    if (private_dir) {
        private_dir_copy = strdup(private_dir);
        if (!private_dir_copy) {
            TEKLOG_ERROR("Failed to duplicate private directory string");
            free(new_ml_entry);
            free(new_ml);
            memdl_close(handle);
            return false;
        }
    }

    // 初始化模组加载器句柄
    new_ml->handle = handle;
    new_ml_entry->ops = ml_ops;
    new_ml_entry->pkg_handle = pkg_handle;
    new_ml_entry->private_dir = private_dir_copy;

    // 获取模组加载器信息
    if (ml_ops->get_ml_info) {
        new_ml_entry->info = ml_ops->get_ml_info();
    } else {
        TEKLOG_WARN("Modloader does not provide get_ml_info method");
        new_ml_entry->info = NULL;
    }

    new_ml->ml_entry = new_ml_entry;
    new_ml->index = tefstd_vector_size(&g_ml_list.handle);

    // 添加到全局列表
    if (!tefstd_vector_push_back(&g_ml_list.handle, &new_ml)) {
        TEKLOG_ERROR("Failed to add modloader to global list");
        free_modloader(new_ml);
        return false;
    }

    tpf_register_shared_plugin_library(new_ml->handle);
    *out_ml = new_ml;
    TEKLOG_INFO("Successfully loaded modloader %p (index: %zu)", (void*)new_ml, new_ml->index);
    return true;
}

void tefkernel_cleanup_ml(ml_handle_t *ml_handle) {
    if (!ml_handle) {
        TEKLOG_WARN("Attempted to cleanup NULL modloader handle");
        return;
    }

    TEKLOG_INFO("Cleaning up modloader %p (index: %zu)", (void*)ml_handle, ml_handle->index);

    if (!g_ml_list.initialized) {
        TEKLOG_DEBUG("Modloader list not initialized, directly freeing handle");
        free_modloader(ml_handle);
        return;
    }

    // 从全局列表中移除
    const size_t ml_count = tefstd_vector_size(&g_ml_list.handle);
    TEKLOG_DEBUG("Searching for modloader in global list of %zu entries", ml_count);

    bool found = false;
    for (size_t i = 0; i < ml_count; ++i) {
        ml_handle_t** ml_ptr = tefstd_vector_at(&g_ml_list.handle, i);
        if (ml_ptr && *ml_ptr == ml_handle) {
            TEKLOG_DEBUG("Removing modloader from global list at index %zu", i);
            tefstd_vector_erase(&g_ml_list.handle, i, NULL);
            found = true;

            // 更新后续元素的索引
            for (size_t j = i; j < tefstd_vector_size(&g_ml_list.handle); ++j) {
                ml_handle_t** subsequent_ml = tefstd_vector_at(&g_ml_list.handle, j);
                if (subsequent_ml && *subsequent_ml) {
                    (*subsequent_ml)->index = j;
                }
            }
            break;
        }
    }

    if (!found) {
        TEKLOG_WARN("Modloader handle %p not found in global list", (void*)ml_handle);
    }

    // 执行实际的清理工作
    free_modloader(ml_handle);

    TEKLOG_INFO("Modloader cleanup completed successfully");
}

void tefkernel_cleanup_all_ml(void) {
    if (!g_ml_list.initialized) {
        TEKLOG_DEBUG("Modloader list not initialized, nothing to cleanup");
        return;
    }

    const size_t ml_count = tefstd_vector_size(&g_ml_list.handle);
    TEKLOG_INFO("Cleaning up all %zu modloaders", ml_count);

    // 反向清理以避免索引问题
    for (size_t i = ml_count; i > 0; --i) {
        ml_handle_t** ml_ptr = tefstd_vector_at(&g_ml_list.handle, i - 1);
        if (ml_ptr && *ml_ptr) {
            TEKLOG_DEBUG("Cleaning up modloader at index %zu", i - 1);
            free_modloader(*ml_ptr);
        }
    }

    // 清空向量但保持初始化状态
    tefstd_vector_clear(&g_ml_list.handle);

    TEKLOG_INFO("All modloaders cleaned up successfully");
}

ml_handle_t* tefkernel_get_ml_by_index(const size_t index) {
    if (!g_ml_list.initialized) {
        TEKLOG_DEBUG("Modloader list not initialized");
        return NULL;
    }

    if (index >= tefstd_vector_size(&g_ml_list.handle)) {
        TEKLOG_WARN("Modloader index %zu out of bounds", index);
        return NULL;
    }

    ml_handle_t** ml_ptr = tefstd_vector_at(&g_ml_list.handle, index);
    return ml_ptr ? *ml_ptr : NULL;
}

size_t tefkernel_get_ml_count(void) {
    if (!g_ml_list.initialized) {
        return 0;
    }
    return tefstd_vector_size(&g_ml_list.handle);
}