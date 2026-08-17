/*******************************************************************************
 * tefkernel - netmanager
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
 * Created: 2026/7/24
 *******************************************************************************/

#include <string.h>
#include <stdlib.h>

#include "internal/terraria/netmanager.h"

#include <stdint.h>
#include <stdio.h>

#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/struct/array.h"
#include "patchlib/struct/string.h"
#include "terraria/main.h"

#include "../../tefpackage/lz4.h"
#include "internal/mod_core.h"
#include "internal/modloader/modloader_core_imp.h"
#include "internal/module/module_core_imp.h"

// 前向声明
connection_type_t terraria_netmanager_client_connections[256];
error_type_t terraria_netmanager_client_errors[256];
char terraria_netmanager_error_details[256][1024];

static patch_handle_t network_text_from_literal = PATCH_NULL;
#if !defined(__ANDROID__)
static patch_handle_t read_buffer = PATCH_NULL;
static patch_handle_t read_buffer_max = PATCH_NULL;
#endif
static patch_handle_t who_am_i = PATCH_NULL;

static bool send_data_hook(patch_handle_t instance, void **args,
                           const patch_method_signature_t *sig_info, void *result);

static bool send_hook(patch_handle_t instance, void **args,
                      const patch_method_signature_t *sig_info, void *result);

static bool get_data_hook(patch_handle_t instance, void **args,
                          const patch_method_signature_t *sig_info, void *result);

/**
 * @brief 获取所有模块、ModLoader和Mod的ID和版本列表
 * @param out_data 输出的序列化数据（调用者需释放）
 * @param out_size 输出数据大小
 * @return 成功返回true
 */
static bool terraria_netmanager_get_all_versions(uint8_t **out_data, uint32_t *out_size) {
    if (!out_data || !out_size) return false;

    // 使用动态缓冲区
    size_t buffer_size = 4096;
    uint8_t *buffer = malloc(buffer_size);
    if (!buffer) return false;

    size_t offset = 0;

    // 写入版本号标识 (用于快速检查)
    buffer[offset++] = 0x01; // 协议版本

    // ===== 写入Module列表 =====
    const size_t module_count = tefkernel_get_module_count();

    // 写入数量 (2字节)
    if (offset + 2 > buffer_size) {
        buffer_size += 1024;
        buffer = realloc(buffer, buffer_size); // NOLINT(*-suspicious-realloc-usage)
    }
    buffer[offset++] = (module_count >> 8) & 0xFF;
    buffer[offset++] = module_count & 0xFF;

    for (size_t i = 0; i < module_count; i++) {
        module_handle_t *mod = tefkernel_get_module_by_index(i);
        if (!mod) continue;

        const module_info_t *info = tefkernel_get_module_info(mod);
        if (!info) continue;

        // 写入pkg_id (长度 + 字符串)
        const size_t id_len = strlen(info->pkg_id);
        if (offset + 1 + id_len + 4 > buffer_size) {
            buffer_size += id_len + 1024;
            buffer = realloc(buffer, buffer_size); // NOLINT(*-suspicious-realloc-usage)
        }
        buffer[offset++] = (uint8_t) id_len;
        memcpy(buffer + offset, info->pkg_id, id_len);
        offset += id_len;

        // 写入version_code (4字节)
        buffer[offset++] = (info->version_code >> 24) & 0xFF;
        buffer[offset++] = (info->version_code >> 16) & 0xFF;
        buffer[offset++] = (info->version_code >> 8) & 0xFF;
        buffer[offset++] = info->version_code & 0xFF;
    }

    // ===== 写入ModLoader列表 =====
    const size_t ml_count = tefkernel_get_ml_count();
    if (offset + 2 > buffer_size) {
        buffer_size += 1024;
        buffer = realloc(buffer, buffer_size); // NOLINT(*-suspicious-realloc-usage)
    }
    buffer[offset++] = (ml_count >> 8) & 0xFF;
    buffer[offset++] = ml_count & 0xFF;

    for (size_t i = 0; i < ml_count; i++) {
        ml_handle_t *ml = tefkernel_get_ml_by_index(i);
        if (!ml) continue;

        const ml_info_t *info = ml->ml_entry->info;
        if (!info) continue;

        const size_t id_len = strlen(info->pkg_id);
        if (offset + 1 + id_len + 4 > buffer_size) {
            buffer_size += id_len + 1024;
            buffer = realloc(buffer, buffer_size); // NOLINT(*-suspicious-realloc-usage)
        }
        buffer[offset++] = (uint8_t) id_len;
        memcpy(buffer + offset, info->pkg_id, id_len);
        offset += id_len;

        buffer[offset++] = (info->version_code >> 24) & 0xFF;
        buffer[offset++] = (info->version_code >> 16) & 0xFF;
        buffer[offset++] = (info->version_code >> 8) & 0xFF;
        buffer[offset++] = info->version_code & 0xFF;
    }

    // ===== 写入Mod列表 =====
    const size_t mod_count = tefkernel_get_mod_count();
    if (offset + 2 > buffer_size) {
        buffer_size += 1024;
        buffer = realloc(buffer, buffer_size); // NOLINT(*-suspicious-realloc-usage)
    }
    buffer[offset++] = (mod_count >> 8) & 0xFF;
    buffer[offset++] = mod_count & 0xFF;

    for (size_t i = 0; i < mod_count; i++) {
        mod_handle_t *mod = tefkernel_get_mod_by_index(i);
        if (!mod) continue;

        if (!mod->manifest || !mod->mod_id) continue;

        const size_t id_len = strlen(mod->mod_id);
        if (offset + 1 + id_len + 4 > buffer_size) {
            buffer_size += id_len + 1024;
            buffer = realloc(buffer, buffer_size); // NOLINT(*-suspicious-realloc-usage)
        }
        buffer[offset++] = (uint8_t) id_len;
        memcpy(buffer + offset, mod->mod_id, id_len);
        offset += id_len;

        // 写入mod的version_code (从manifest获取)
        const multiplayer_mod_info_t *mod_info = mod->owner_ml->ml_entry->ops->get_multiplayer_info(mod->manifest);
        const int version_code = mod_info->version_code ? mod_info->version_code : 0;
        buffer[offset++] = (version_code >> 24) & 0xFF;
        buffer[offset++] = (version_code >> 16) & 0xFF;
        buffer[offset++] = (version_code >> 8) & 0xFF;
        buffer[offset++] = version_code & 0xFF;
    }

    // ===== LZ4压缩 =====
    // 计算最大压缩大小
    const int max_compressed_size = LZ4_compressBound((int) offset);
    uint8_t *compressed = malloc(max_compressed_size + 4); // +4用于存储原始大小
    if (!compressed) {
        free(buffer);
        return false;
    }

    // 存储原始大小 (4字节)
    compressed[0] = (offset >> 24) & 0xFF;
    compressed[1] = (offset >> 16) & 0xFF;
    compressed[2] = (offset >> 8) & 0xFF;
    compressed[3] = offset & 0xFF;

    // 压缩
    const int compressed_size = LZ4_compress_default((char *) buffer, (char *) (compressed + 4), (int) offset,
                                                     max_compressed_size);
    if (compressed_size <= 0) {
        free(buffer);
        free(compressed);
        return false;
    }

    free(buffer);

    *out_data = compressed;
    *out_size = compressed_size + 4; // 包含原始大小头

    TEKLOG_INFO("Serialized data: %zu bytes, compressed to: %d bytes", offset, compressed_size);
    return true;
}

/**
 * @brief 比较本地和远程的模块/ModLoader/Mod列表（严格模式）
 * @param remote_data 远程发送的数据
 * @param remote_size 数据大小
 * @param error_msg 输出的错误消息
 * @param error_msg_size 错误消息缓冲区大小
 * @return 完全相同返回true，任何差异返回false
 */
static bool terraria_netmanager_compare_versions(const uint8_t *remote_data, const uint32_t remote_size,
                                                 char *error_msg, const size_t error_msg_size) {
    if (!remote_data || remote_size < 4) {
        snprintf(error_msg, error_msg_size, "Invalid data");
        return false;
    }

    // 解压
    const uint32_t original_size = (remote_data[0] << 24) | (remote_data[1] << 16) |
                                   (remote_data[2] << 8) | remote_data[3];

    if (original_size > 1024 * 1024) {
        snprintf(error_msg, error_msg_size, "Data too large: %u", original_size);
        return false;
    }

    uint8_t *decompressed = malloc(original_size);
    if (!decompressed) {
        snprintf(error_msg, error_msg_size, "Memory allocation failed");
        return false;
    }

    const int decompressed_size = LZ4_decompress_safe((char *) (remote_data + 4), (char *) decompressed,
                                                      (int) remote_size - 4, (int) original_size);
    if (decompressed_size < 0) {
        free(decompressed);
        snprintf(error_msg, error_msg_size, "Decompression failed");
        return false;
    }

    size_t offset = 0;
    char mismatch_details[2048] = {0};
    bool has_mismatch = false;
    int total_errors = 0;
    const int MAX_DISPLAY = 10;

    // 检查协议版本
    const uint8_t proto_version = decompressed[offset++];
    if (proto_version != 0x01) {
        snprintf(error_msg, error_msg_size, "Protocol version mismatch: local=1, remote=%d", proto_version);
        free(decompressed);
        return false;
    }

    typedef struct {
        char id[128];
        int version;
        bool matched;
    } version_entry_t;

    // ============================================================
    // 1. 比较 Module
    // ============================================================
    const uint16_t remote_module_count = (decompressed[offset] << 8) | decompressed[offset + 1];
    offset += 2;
    const size_t local_module_count = tefkernel_get_module_count();

    version_entry_t local_modules[512], remote_modules[512];
    size_t local_modules_found = 0, remote_modules_found = 0;

    for (size_t i = 0; i < local_module_count && local_modules_found < 512; i++) {
        module_handle_t *mod = tefkernel_get_module_by_index(i);
        if (!mod) continue;
        const module_info_t *info = tefkernel_get_module_info(mod);
        if (!info) continue;
        strncpy(local_modules[local_modules_found].id, info->pkg_id, 127);
        local_modules[local_modules_found].id[127] = '\0';
        local_modules[local_modules_found].version = info->version_code;
        local_modules[local_modules_found].matched = false;
        local_modules_found++;
    }

    for (uint16_t i = 0; i < remote_module_count && offset < decompressed_size && remote_modules_found < 512; i++) {
        uint8_t id_len = decompressed[offset++];
        if (offset + id_len + 4 > decompressed_size) break;
        char id[128];
        if (id_len >= 128) id_len = 127;
        memcpy(id, decompressed + offset, id_len);
        id[id_len] = '\0';
        offset += id_len;
        const int version = (decompressed[offset] << 24) | (decompressed[offset + 1] << 16) |
                            (decompressed[offset + 2] << 8) | decompressed[offset + 3];
        offset += 4;
        strncpy(remote_modules[remote_modules_found].id, id, 127);
        remote_modules[remote_modules_found].id[127] = '\0';
        remote_modules[remote_modules_found].version = version;
        remote_modules[remote_modules_found].matched = false;
        remote_modules_found++;
    }

    char module_errors[1024] = {0};
    int module_error_count = 0;

    if (local_modules_found != remote_modules_found) {
        snprintf(module_errors + strlen(module_errors), sizeof(module_errors) - strlen(module_errors),
                 "count: %zu vs %zu\n", local_modules_found, remote_modules_found);
        module_error_count++;
        total_errors++;
    }

    for (size_t i = 0; i < local_modules_found && module_error_count < MAX_DISPLAY; i++) {
        bool found = false;
        for (size_t j = 0; j < remote_modules_found; j++) {
            if (strcmp(local_modules[i].id, remote_modules[j].id) == 0) {
                found = true;
                remote_modules[j].matched = true;
                if (local_modules[i].version != remote_modules[j].version) {
                    snprintf(module_errors + strlen(module_errors), sizeof(module_errors) - strlen(module_errors),
                             "%s : v%d != v%d\n", local_modules[i].id,
                             local_modules[i].version, remote_modules[j].version);
                    module_error_count++;
                    total_errors++;
                }
                break;
            }
        }
        if (!found) {
            snprintf(module_errors + strlen(module_errors), sizeof(module_errors) - strlen(module_errors),
                     "%s : missing (v%d)\n", local_modules[i].id, local_modules[i].version);
            module_error_count++;
            total_errors++;
        }
    }

    for (size_t i = 0; i < remote_modules_found && module_error_count < MAX_DISPLAY; i++) {
        if (!remote_modules[i].matched) {
            snprintf(module_errors + strlen(module_errors), sizeof(module_errors) - strlen(module_errors),
                     "%s : extra (v%d)\n", remote_modules[i].id, remote_modules[i].version);
            module_error_count++;
            total_errors++;
        }
    }

    // ============================================================
    // 2. 比较 ModLoader
    // ============================================================
    char ml_errors[1024] = {0};

    if (offset + 2 <= decompressed_size) {
        int ml_error_count = 0;
        const uint16_t remote_ml_count = (decompressed[offset] << 8) | decompressed[offset + 1];
        offset += 2;
        const size_t local_ml_count = tefkernel_get_ml_count();

        version_entry_t local_mls[128], remote_mls[128];
        size_t local_ml_found = 0, remote_ml_found = 0;

        for (size_t i = 0; i < local_ml_count && local_ml_found < 128; i++) {
            ml_handle_t *ml = tefkernel_get_ml_by_index(i);
            if (!ml) continue;
            const ml_info_t *info = ml->ml_entry->info;
            if (!info) continue;
            strncpy(local_mls[local_ml_found].id, info->pkg_id, 127);
            local_mls[local_ml_found].id[127] = '\0';
            local_mls[local_ml_found].version = info->version_code;
            local_mls[local_ml_found].matched = false;
            local_ml_found++;
        }

        for (uint16_t i = 0; i < remote_ml_count && offset < decompressed_size && remote_ml_found < 128; i++) {
            if (offset + 1 > decompressed_size) break;
            uint8_t id_len = decompressed[offset++];
            if (offset + id_len + 4 > decompressed_size) break;
            char id[128];
            if (id_len >= 128) id_len = 127;
            memcpy(id, decompressed + offset, id_len);
            id[id_len] = '\0';
            offset += id_len;
            const int version = (decompressed[offset] << 24) | (decompressed[offset + 1] << 16) |
                                (decompressed[offset + 2] << 8) | decompressed[offset + 3];
            offset += 4;
            strncpy(remote_mls[remote_ml_found].id, id, 127);
            remote_mls[remote_ml_found].id[127] = '\0';
            remote_mls[remote_ml_found].version = version;
            remote_mls[remote_ml_found].matched = false;
            remote_ml_found++;
        }

        if (local_ml_found != remote_ml_found) {
            snprintf(ml_errors + strlen(ml_errors), sizeof(ml_errors) - strlen(ml_errors),
                     "count: %zu vs %zu\n", local_ml_found, remote_ml_found);
            ml_error_count++;
            total_errors++;
        }

        for (size_t i = 0; i < local_ml_found && ml_error_count < MAX_DISPLAY; i++) {
            bool found = false;
            for (size_t j = 0; j < remote_ml_found; j++) {
                if (strcmp(local_mls[i].id, remote_mls[j].id) == 0) {
                    found = true;
                    remote_mls[j].matched = true;
                    if (local_mls[i].version != remote_mls[j].version) {
                        snprintf(ml_errors + strlen(ml_errors), sizeof(ml_errors) - strlen(ml_errors),
                                 "%s : v%d != v%d\n", local_mls[i].id,
                                 local_mls[i].version, remote_mls[j].version);
                        ml_error_count++;
                        total_errors++;
                    }
                    break;
                }
            }
            if (!found) {
                snprintf(ml_errors + strlen(ml_errors), sizeof(ml_errors) - strlen(ml_errors),
                         "%s : missing (v%d)\n", local_mls[i].id, local_mls[i].version);
                ml_error_count++;
                total_errors++;
            }
        }

        for (size_t i = 0; i < remote_ml_found && ml_error_count < MAX_DISPLAY; i++) {
            if (!remote_mls[i].matched) {
                snprintf(ml_errors + strlen(ml_errors), sizeof(ml_errors) - strlen(ml_errors),
                         "%s : extra (v%d)\n", remote_mls[i].id, remote_mls[i].version);
                ml_error_count++;
                total_errors++;
            }
        }

        // ============================================================
        // 3. 比较 Mod（按 ModLoader 分组）
        // ============================================================
        if (offset + 2 <= decompressed_size) {
            const uint16_t remote_mod_count = (decompressed[offset] << 8) | decompressed[offset + 1];
            offset += 2;
            const size_t local_mod_count = tefkernel_get_mod_count();

            typedef struct {
                char id[128];
                int version;
                char ml_id[128];
                bool matched;
            } mod_entry_t;

            mod_entry_t local_mods[512];
            size_t local_mod_found = 0;

            for (size_t i = 0; i < local_mod_count && local_mod_found < 512; i++) {
                const mod_handle_t *mod = tefkernel_get_mod_by_index(i);
                if (!mod || !mod->mod_id || !mod->owner_ml) continue;
                strncpy(local_mods[local_mod_found].id, mod->mod_id, 127);
                local_mods[local_mod_found].id[127] = '\0';
                const multiplayer_mod_info_t *mod_info = mod->owner_ml->ml_entry->ops->get_multiplayer_info(mod->manifest);
                local_mods[local_mod_found].version = mod_info ? mod_info->version_code : 0;
                // 获取 ModLoader ID
                const ml_info_t *ml_info = mod->owner_ml->ml_entry->info;
                if (ml_info) {
                    strncpy(local_mods[local_mod_found].ml_id, ml_info->pkg_id, 127);
                } else {
                    strcpy(local_mods[local_mod_found].ml_id, "unknown");
                }
                local_mods[local_mod_found].ml_id[127] = '\0';
                local_mods[local_mod_found].matched = false;
                local_mod_found++;
            }

            mod_entry_t remote_mods[512];
            size_t remote_mod_found = 0;

            for (uint16_t i = 0; i < remote_mod_count && offset < decompressed_size && remote_mod_found < 512; i++) {
                if (offset + 1 > decompressed_size) break;
                uint8_t id_len = decompressed[offset++];
                if (offset + id_len + 4 > decompressed_size) break;
                char id[128];
                if (id_len >= 128) id_len = 127;
                memcpy(id, decompressed + offset, id_len);
                id[id_len] = '\0';
                offset += id_len;
                const int version = (decompressed[offset] << 24) | (decompressed[offset + 1] << 16) |
                                    (decompressed[offset + 2] << 8) | decompressed[offset + 3];
                offset += 4;
                strncpy(remote_mods[remote_mod_found].id, id, 127);
                remote_mods[remote_mod_found].id[127] = '\0';
                remote_mods[remote_mod_found].version = version;
                strcpy(remote_mods[remote_mod_found].ml_id, "?"); // 远程数据没有 ModLoader 信息
                remote_mods[remote_mod_found].matched = false;
                remote_mod_found++;
            }

            // 按 ModLoader 分组收集错误
            typedef struct {
                char ml_id[128];
                char errors[1024];
                int count;
            } ml_group_t;

            ml_group_t groups[64];
            int group_count = 0;

            // 检查本地 Mod（缺失或版本不匹配）
            for (size_t i = 0; i < local_mod_found; i++) {
                bool found = false;
                for (size_t j = 0; j < remote_mod_found; j++) {
                    if (strcmp(local_mods[i].id, remote_mods[j].id) == 0) {
                        found = true;
                        remote_mods[j].matched = true;
                        if (local_mods[i].version != remote_mods[j].version) {
                            int gidx = -1;
                            for (int k = 0; k < group_count; k++) {
                                if (strcmp(groups[k].ml_id, local_mods[i].ml_id) == 0) {
                                    gidx = k;
                                    break;
                                }
                            }
                            if (gidx == -1 && group_count < 64) {
                                gidx = group_count;
                                strncpy(groups[gidx].ml_id, local_mods[i].ml_id, 127);
                                groups[gidx].ml_id[127] = '\0';
                                groups[gidx].errors[0] = '\0';
                                groups[gidx].count = 0;
                                group_count++;
                            }
                            if (gidx != -1 && groups[gidx].count < MAX_DISPLAY) {
                                snprintf(groups[gidx].errors + strlen(groups[gidx].errors),
                                         sizeof(groups[gidx].errors) - strlen(groups[gidx].errors),
                                         "%s : v%d != v%d\n", local_mods[i].id,
                                         local_mods[i].version, remote_mods[j].version);
                                groups[gidx].count++;
                                total_errors++;
                            }
                        }
                        break;
                    }
                }
                if (!found) {
                    int gidx = -1;
                    for (int k = 0; k < group_count; k++) {
                        if (strcmp(groups[k].ml_id, local_mods[i].ml_id) == 0) {
                            gidx = k;
                            break;
                        }
                    }
                    if (gidx == -1 && group_count < 64) {
                        gidx = group_count;
                        strncpy(groups[gidx].ml_id, local_mods[i].ml_id, 127);
                        groups[gidx].ml_id[127] = '\0';
                        groups[gidx].errors[0] = '\0';
                        groups[gidx].count = 0;
                        group_count++;
                    }
                    if (gidx != -1 && groups[gidx].count < MAX_DISPLAY) {
                        snprintf(groups[gidx].errors + strlen(groups[gidx].errors),
                                 sizeof(groups[gidx].errors) - strlen(groups[gidx].errors),
                                 "%s : missing (v%d)\n", local_mods[i].id, local_mods[i].version);
                        groups[gidx].count++;
                        total_errors++;
                    }
                }
            }

            // 检查远程多余的 Mod（放到 "?" 组）
            for (size_t i = 0; i < remote_mod_found; i++) {
                if (!remote_mods[i].matched) {
                    int gidx = -1;
                    for (int k = 0; k < group_count; k++) {
                        if (strcmp(groups[k].ml_id, "?") == 0) {
                            gidx = k;
                            break;
                        }
                    }
                    if (gidx == -1 && group_count < 64) {
                        gidx = group_count;
                        strcpy(groups[gidx].ml_id, "?");
                        groups[gidx].errors[0] = '\0';
                        groups[gidx].count = 0;
                        group_count++;
                    }
                    if (gidx != -1 && groups[gidx].count < MAX_DISPLAY) {
                        snprintf(groups[gidx].errors + strlen(groups[gidx].errors),
                                 sizeof(groups[gidx].errors) - strlen(groups[gidx].errors),
                                 "%s : extra (v%d)\n", remote_mods[i].id, remote_mods[i].version);
                        groups[gidx].count++;
                        total_errors++;
                    }
                }
            }

            // 构建最终错误信息
            snprintf(mismatch_details, sizeof(mismatch_details),
                     "%d error(s)\n", total_errors);

            if (module_error_count > 0) {
                strncat(mismatch_details, "[Module] ", sizeof(mismatch_details) - strlen(mismatch_details) - 1);
                strncat(mismatch_details, module_errors, sizeof(mismatch_details) - strlen(mismatch_details) - 1);
            }

            if (ml_error_count > 0) {
                strncat(mismatch_details, "[ModLoader] ", sizeof(mismatch_details) - strlen(mismatch_details) - 1);
                strncat(mismatch_details, ml_errors, sizeof(mismatch_details) - strlen(mismatch_details) - 1);
            }

            // 按顺序输出每个 ModLoader 的错误
            for (int i = 0; i < group_count; i++) {
                if (groups[i].count > 0) {
                    char header[256];
                    snprintf(header, sizeof(header), "[%s] ", groups[i].ml_id);
                    strncat(mismatch_details, header, sizeof(mismatch_details) - strlen(mismatch_details) - 1);
                    strncat(mismatch_details, groups[i].errors, sizeof(mismatch_details) - strlen(mismatch_details) - 1);
                }
            }

            has_mismatch = (total_errors > 0);
        }
    }

    free(decompressed);

    if (has_mismatch) {
        snprintf(error_msg, error_msg_size, "%s", mismatch_details);
        return false;
    }

    snprintf(error_msg, error_msg_size, "OK");
    return true;
}

static patch_handle_t terraria_netmanager_create_tefkernel_pack() {
    uint8_t *serialized_data = NULL;
    uint32_t serialized_size = 0;

    if (!terraria_netmanager_get_all_versions(&serialized_data, &serialized_size)) {
        TEKLOG_ERROR("Failed to get versions data");
        return NULL;
    }

    const int game_release = terraria_main_get_cur_release();
    const int str_len = strlen(TEFKERNEL_MAGIC_STRING);

    // 包结构: 总长度(2) + 类型(1) + 魔数字符串(长度+内容) + gameRelease(4) + versionCode(8) + 压缩数据
    const uint32_t total_length = 2 + 1 + 1 + str_len + 4 + 8 + serialized_size;

    TEKLOG_INFO("New TEFKernel packet: total=%u bytes (data=%u bytes)", total_length, serialized_size);

    patch_handle_t uint8_type = patchlib_get_basic_type(PATCH_UINT8);
    patch_handle_t new_array = patchlib_array_create(total_length, uint8_type);
    patchlib_free(uint8_type);

    if (!new_array) {
        TEKLOG_ERROR("Failed to create array");
        free(serialized_data);
        return NULL;
    }

    uint8_t *new_array_c = malloc(total_length);
    if (!new_array_c) {
        TEKLOG_ERROR("Failed to allocate memory");
        patchlib_free(new_array);
        free(serialized_data);
        return NULL;
    }

    size_t offset = 0;

    // 总长度 (小端序)
    new_array_c[offset++] = (uint8_t) (total_length & 0xFF);
    new_array_c[offset++] = (uint8_t) ((total_length >> 8) & 0xFF);

    // 包类型
    new_array_c[offset++] = 0x01;

    // 魔数字符串
    new_array_c[offset++] = (uint8_t) str_len;
    memcpy(&new_array_c[offset], TEFKERNEL_MAGIC_STRING, str_len);
    offset += str_len;

    // gameRelease
    for (int i = 0; i < 4; i++) {
        new_array_c[offset++] = (uint8_t) ((game_release >> (i * 8)) & 0xFF);
    }

    // versionCode (用于快速检查)
    for (int i = 0; i < 8; i++) {
        new_array_c[offset++] = (uint8_t) ((TEFKERNEL_VERSION_CODE >> (i * 8)) & 0xFF);
    }

    // 压缩数据
    memcpy(&new_array_c[offset], serialized_data, serialized_size);
    offset += serialized_size;

    free(serialized_data);

    patchlib_array_copy_from_c(new_array, new_array_c, total_length);
    free(new_array_c);

    TEKLOG_INFO("TEFKernel packet created successfully (%zu bytes)", offset);
    return new_array;
}

static patch_handle_t terraria_netmanager_create_vanilla_connection_packet(int *out_size) {
    patch_handle_t uint8_type = patchlib_get_basic_type(PATCH_UINT8);
    const int game_release = terraria_main_get_cur_release();

    char vanilla_string[32];
    const int vanilla_str_len = snprintf(vanilla_string, sizeof(vanilla_string), "Terraria%d", game_release);

    if (vanilla_str_len > 255) {
        TEKLOG_ERROR("Vanilla string too long: %d", vanilla_str_len);
        patchlib_free(uint8_type);
        return NULL;
    }

    const int packet_length = 1 + 1 + vanilla_str_len;
    patch_handle_t new_array = patchlib_array_create(packet_length, uint8_type);
    patchlib_free(uint8_type);
    if (!new_array) return NULL;

    uint8_t *buffer = malloc(packet_length);
    if (!buffer) {
        patchlib_free(new_array);
        return NULL;
    }

    int offset = 0;
    buffer[offset++] = 1;
    buffer[offset++] = (uint8_t) vanilla_str_len;
    memcpy(&buffer[offset], vanilla_string, vanilla_str_len);
    offset += vanilla_str_len;

    patchlib_array_copy_from_c(new_array, buffer, packet_length);
    free(buffer);

    if (out_size) *out_size = packet_length;
    return new_array;
}

static patch_handle_t terraria_netmanager_create_null_connection_packet() {
    patch_handle_t uint8_type = patchlib_get_basic_type(PATCH_UINT8);
    const uint16_t total_length = 5;
    const int packet_length = 5;

    patch_handle_t new_array = patchlib_array_create(packet_length, uint8_type);
    patchlib_free(uint8_type);
    if (!new_array) return NULL;

    uint8_t *buffer = malloc(packet_length);
    if (!buffer) {
        patchlib_free(new_array);
        return NULL;
    }

    int offset = 0;
    buffer[offset++] = (uint8_t) (total_length & 0xFF);
    buffer[offset++] = (uint8_t) ((total_length >> 8) & 0xFF);
    buffer[offset++] = 1;
    buffer[offset++] = 1;
    buffer[offset++] = 'a';

    patchlib_array_copy_from_c(new_array, buffer, packet_length);
    free(buffer);

    return new_array;
}

static connection_type_t terraria_netmanager_parse_tefconnection_packet(const uint8_t *data, const int data_len,
                                                                        const uint8_t client) {
    int offset = 1;
    if (offset + 1 >= data_len) {
        terraria_netmanager_client_errors[client] = ERROR_VERSION;
        snprintf(terraria_netmanager_error_details[client], sizeof(terraria_netmanager_error_details[client]),
                 "Invalid packet: too short");
        return CONNECTION_TYPE_VERSION_MISMATCH;
    }

    const uint8_t str_len = data[offset++];
    if (offset + str_len + 4 + 8 > data_len) {
        terraria_netmanager_client_errors[client] = ERROR_VERSION;
        snprintf(terraria_netmanager_error_details[client], sizeof(terraria_netmanager_error_details[client]),
                 "Invalid packet: incomplete data");
        return CONNECTION_TYPE_VERSION_MISMATCH;
    }

    // 检查魔数字符串
    if (str_len != strlen(TEFKERNEL_MAGIC_STRING) ||
        memcmp(&data[offset], TEFKERNEL_MAGIC_STRING, str_len) != 0) {
        terraria_netmanager_client_errors[client] = ERROR_VERSION;
        snprintf(terraria_netmanager_error_details[client], sizeof(terraria_netmanager_error_details[client]),
                 "Invalid magic string");
        return CONNECTION_TYPE_VERSION_MISMATCH;
    }
    offset += str_len;

    // 读取gameRelease (跳过)
    offset += 4;

    // 读取versionCode
    uint64_t version = 0;
    for (int i = 0; i < 8; i++) {
        version |= (uint64_t) data[offset++] << (i * 8);
    }

    if (version != TEFKERNEL_VERSION_CODE) {
        terraria_netmanager_client_errors[client] = ERROR_VERSION;
        snprintf(terraria_netmanager_error_details[client], sizeof(terraria_netmanager_error_details[client]),
                 "Version mismatch: local=%llu, remote=%llu",
                 (unsigned long long) TEFKERNEL_VERSION_CODE, (unsigned long long) version);
        return CONNECTION_TYPE_VERSION_MISMATCH;
    }

    // 解析压缩数据
    const uint32_t compressed_size = data_len - offset;
    if (compressed_size < 4) {
        terraria_netmanager_client_errors[client] = ERROR_VERSION;
        snprintf(terraria_netmanager_error_details[client], sizeof(terraria_netmanager_error_details[client]),
                 "Invalid compressed data");
        return CONNECTION_TYPE_VERSION_MISMATCH;
    }

    char error_msg[512] = {0};
    if (!terraria_netmanager_compare_versions(data + offset, compressed_size, error_msg, sizeof(error_msg))) {
        terraria_netmanager_client_errors[client] = ERROR_MOD_REQUIRED;
        snprintf(terraria_netmanager_error_details[client], sizeof(terraria_netmanager_error_details[client]),
                 "%s", error_msg);
        return CONNECTION_TYPE_BAD_HASH;
    }

    // 所有检查通过
    snprintf(terraria_netmanager_error_details[client], sizeof(terraria_netmanager_error_details[client]),
             "All modules/ModLoaders/Mods match");
    return CONNECTION_TYPE_TEFKERNEL;
}

static bool terraria_netmanager_is_vanilla_connection_packet(const uint8_t *data, const int data_len) {
    int offset = 1;
    if (offset + 1 >= data_len) return false;

    const uint8_t str_len = data[offset++];
    if (offset + str_len > data_len) return false;

    const char *str = (char *) &data[offset];
    const char *terraria_prefix = "Terraria";

    if (str_len < strlen(terraria_prefix)) return false;
    return strncmp(str, terraria_prefix, strlen(terraria_prefix)) == 0;
}

void terraria_netmanager_init() {
    if (tefkernel_get_module_count() == 0 && tefkernel_get_ml_count() == 0) {
        TEKLOG_INFO("No modules or ModLoaders loaded - version isolation disabled, using vanilla networking logic\n"
                    "TEFKernel version isolation is skipped, all clients can connect without version checking");
        return;
    }

    // 初始化错误信息
    for (int i = 0; i < 256; i++)
        terraria_netmanager_error_details[i][0] = '\0';

    patch_handle_t message_buffer_class = patchlib_type_get_type("Terraria", "MessageBuffer");
    patch_handle_t net_message_class = patchlib_type_get_type("Terraria", "NetMessage");
    patch_handle_t network_text_class = patchlib_type_get_type("Terraria.Localization", "NetworkText");
    patch_handle_t send_data = patchlib_type_get_method(net_message_class, "SendData");
    patch_handle_t send = PATCH_NULL;
    patch_handle_t process_data = PATCH_NULL;
    network_text_from_literal = patchlib_type_get_method_by_param_count(network_text_class, "FromLiteral", 1);
    who_am_i = patchlib_type_get_field(message_buffer_class, "whoAmI");

#if defined(__ANDROID__)
    send = patchlib_type_get_method_by_param_count(patchlib_type_get_type("Telepathy", "Client"), "Send", 2);
    process_data = patchlib_type_get_method_by_param_count(message_buffer_class, "ProcessData", 3);
#else
    send = patchlib_type_get_method(net_message_class, "SendPacket");
    read_buffer = patchlib_type_get_field(message_buffer_class, "readBuffer");
    read_buffer_max = patchlib_type_get_field(message_buffer_class, "readBufferMax");
    process_data = patchlib_type_get_method(message_buffer_class, "GetData");
#endif

    patchlib_install_prepost_hook(send_data, send_data_hook, NULL);
    patchlib_install_prepost_hook(send, send_hook, NULL);
    patchlib_install_prepost_hook(process_data, get_data_hook, NULL);

    patchlib_free(message_buffer_class);
    patchlib_free(net_message_class);
    patchlib_free(send_data);
    patchlib_free(send);
    patchlib_free(process_data);
    patchlib_free(network_text_class);
}

#if __ANDROID__
typedef struct il2cpp_array_t {
    void *m_class;
    void *m_monitor;
    void *m_bounds;
    uint32_t m_length;
} il2cpp_array_t;
#endif

static bool get_data_hook(patch_handle_t instance, void **args,
                          const patch_method_signature_t *sig_info, void *result) {
    uint8_t msg_type = 0;
#if defined(__ANDROID__)
    uint8_t *buffer = *(patch_handle_t *) args[0] + sizeof(il2cpp_array_t);
    msg_type = buffer[0];
    const int start = 0;
    const int length = *(int *) args[1];
#else
    const int start = *(int *) args[0];
    const int length = *(int *) args[1];
    int buffer_max_size = -1;
    patch_handle_t read_buffer_array = PATCH_NULL;
    patchlib_field_get_value(read_buffer, instance, &read_buffer_array);
    patchlib_field_get_value(read_buffer_max, NULL, &buffer_max_size);
    patchlib_array_at(read_buffer_array, start, &msg_type);
#endif

    if (msg_type == 1) {
#if !defined(__ANDROID__)
        uint8_t *buffer = malloc(buffer_max_size);
        patchlib_array_copy_to_c(buffer, read_buffer_array, buffer_max_size);
#endif

        int client = -1;
        patchlib_field_get_value(who_am_i, instance, &client);

        terraria_netmanager_client_connections[client] = CONNECTION_TYPE_NONE;
        terraria_netmanager_client_errors[client] = ERROR_NONE;
        terraria_netmanager_error_details[client][0] = '\0';

        terraria_netmanager_client_connections[client] =
                terraria_netmanager_parse_tefconnection_packet(buffer + start, length, client);

        if (terraria_netmanager_client_connections[client] != CONNECTION_TYPE_NONE) {
            TEKLOG_INFO("GetData: TEFKernel connection packet from client %d, converting to vanilla", client);
            if (terraria_netmanager_client_errors[client] != ERROR_NONE) {
                TEKLOG_WARN("Rejecting client %d: %s", client,
                             terraria_netmanager_error_details[client]);

                // 清理资源
                patchlib_free(read_buffer_array);
                free(buffer);

                // 返回 false 让原始方法继续执行
                // 这会导致连接包不被识别，客户端收到错误而断开
                return false;
            }

#if __ANDROID__
            patch_handle_t vanilla_array = terraria_netmanager_create_vanilla_connection_packet(args[1]);
            *(patch_handle_t *) args[0] = vanilla_array;
#else
            patch_handle_t vanilla_array = terraria_netmanager_create_vanilla_connection_packet(args[1]);
            for (int i = 0; i < *(int *) args[1]; ++i) {
                uint8_t byte = 0;
                patchlib_array_at(vanilla_array, i, &byte);
                patchlib_array_set(read_buffer_array, start + i, &byte);
            }
            patchlib_free(vanilla_array);
            patchlib_free(read_buffer_array);
            free(buffer);
#endif
            return false;
        }

        if (terraria_netmanager_is_vanilla_connection_packet(buffer + start, length)) {
            terraria_netmanager_client_connections[client] = CONNECTION_TYPE_VANILLA;
            terraria_netmanager_client_errors[client] = ERROR_MOD_REQUIRED;
            snprintf(terraria_netmanager_error_details[client],
                     sizeof(terraria_netmanager_error_details[client]),
                     "Vanilla client detected - TEFKernel required");
            TEKLOG_INFO("Received vanilla connection packet from client %d", client);

            patch_handle_t array_null = terraria_netmanager_create_null_connection_packet();
#if __ANDROID__
            *(patch_handle_t *) args[0] = array_null;
            *(int *) args[1] = 5;
#else
            *(int *) args[1] = 5;
            for (int i = 0; i < 5; ++i) {
                uint8_t byte = 0;
                patchlib_array_at(array_null, i, &byte);
                patchlib_array_set(read_buffer_array, start + i, &byte);
            }
            patchlib_free(array_null);
            patchlib_free(read_buffer_array);
            free(buffer);
#endif
            return false;
        }
    }
    return false;
}

static bool send_data_hook(patch_handle_t instance, void **args,
                           const patch_method_signature_t *sig_info, void *result) {
    if (!args[3]) return false;

    const int msg_type = *(int *) args[0];
    const int remote_client = *(int *) args[1];
    patch_handle_t error_text = *(patch_handle_t *) args[3];

    if (msg_type == 2) {
        TEKLOG_DEBUG("SendData: msgType=2, g_error_message_id=%d, details=%s",
                     terraria_netmanager_client_errors[remote_client],
                     terraria_netmanager_error_details[remote_client]);
        patchlib_free(error_text);

        if (terraria_netmanager_client_errors[remote_client] == ERROR_MOD_REQUIRED ||
            terraria_netmanager_client_errors[remote_client] == ERROR_VERSION) {
            char error_msg[512];
            const char *error_type = terraria_netmanager_client_errors[remote_client] == ERROR_VERSION
                                         ? "Version mismatch"
                                         : "Mod mismatch";
            snprintf(error_msg, sizeof(error_msg), "[TEFKernel] %s: %s",
                     error_type, terraria_netmanager_error_details[remote_client]);

            patch_handle_t error_text_str = patchlib_string_create(error_msg);
            void *iargs[1] = {&error_text_str};
            patchlib_method_invoke_args(network_text_from_literal, PATCH_NULL, &error_text, iargs);
        }

        if (error_text) {
            *(patch_handle_t *) args[3] = error_text;
        }
    }
    return false;
}

static bool send_hook(patch_handle_t instance, void **args,
                      const patch_method_signature_t *sig_info, void *result) {
    patch_handle_t array = *(patch_handle_t *) args[0];
    uint8_t pack_id = 0;
    patchlib_array_at(array, 2, &pack_id);

    if (pack_id == 1) {
        patchlib_free(array);
        patch_handle_t pack = terraria_netmanager_create_tefkernel_pack();
        *(patch_handle_t *) args[0] = pack;
#if defined(__ANDROID__)
        *(int *) args[1] = (int) patchlib_array_length(pack);
#endif
    }
    return false;
}
