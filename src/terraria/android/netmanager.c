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
 * Created: 2026/4/6
 *******************************************************************************/

#include "internal/terraria/netmanager.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "patchlib/method.h"
#include "dobby.h"
#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/struct/array.h"
#include "patchlib/struct/string.h"

typedef struct il2cpp_array_t {
    // ReSharper disable once CppDeclaratorNeverUsed
    void *m_class;
    // ReSharper disable once CppDeclaratorNeverUsed
    void *m_monitor;
    // ReSharper disable once CppDeclaratorNeverUsed
    void *m_bounds;
    // ReSharper disable once CppDeclaratorNeverUsed
    uint32_t m_length;
    // T *m_values;
} il2cpp_array_t;

// 函数指针
static void(*orig_send_data)(int msgType, int remoteClient, int ignoreClient,
                             void* text, int number, float number2, float number3, float number4,
                             int number5, int number6, int number7);
static void(*orig_process_data)(patch_handle_t instance, il2cpp_array_t* messageData, int length, int* messageType);
static bool(*orig_client_send)(patch_handle_t* this, void* data, int length);

// 钩子函数
static void send_data_hook(int msgType, int remoteClient, int ignoreClient,
                          void* text, int number, float number2, float number3, float number4,
                          int number5, int number6, int number7);
static void process_data_hook(patch_handle_t instance, il2cpp_array_t* messageData, int length, int* messageType);
static bool client_send_hook(patch_handle_t* this, void* data, int length);

// 全局状态
typedef enum {
    CONNECTION_TYPE_NONE = 0,      // 无连接
    CONNECTION_TYPE_VANILLA = 1,   // 原版客户端
    CONNECTION_TYPE_TEFKERNEL = 2, // TEFKernel客户端
    CONNECTION_TYPE_BAD_HASH = 3,  // Hash错误
    CONNECTION_TYPE_VERSION_MISMATCH = 4, // 版本不匹配
} connection_type_t;

typedef enum {
    ERROR_NONE = 0,
    ERROR_KICK = 2,      // Lang.mp[1] - "Invalid operation at this state."
    ERROR_BANNED = 3,    // Lang.mp[3] - "Banned."
    ERROR_VERSION = 4,   // Lang.mp[4] - "Wrong version."
    ERROR_MOD_REQUIRED = 5, // 自定义错误：需要TEFKernel
} error_type_t;

static connection_type_t g_connection_type = CONNECTION_TYPE_NONE;
static bool g_send_connection = false;
static error_type_t g_error_message_id = ERROR_NONE;
static patch_handle_t network_text_from_literal; // void* (string text)

// 配置
#define TEFKERNEL_MAGIC_STRING "Terraria With TEFKernel"
#define TEFKERNEL_VERSION_CODE 100ULL
#define MODS_HASH 0x123456789ABCDEF0ULL
#define MODLOADERS_HASH 0xFEDCBA9876543210ULL
#define MODULES_HASH 0x0A1B2C3D4E5F6A7BULL
#define PLUGINS_HASH 0x7B6A5F4E3D2C1B0AULL

// 解析TEFKernel连接包
static connection_type_t parse_tefconnection_packet(const uint8_t* data, const int data_len) {
    int offset = 1;
    if (offset + 1 >= data_len) {
        return CONNECTION_TYPE_NONE;
    }

    // 读取字符串长度
    const uint8_t str_len = data[offset++];
    if (offset + str_len + 4 + (5 * 8) > data_len) {
        return CONNECTION_TYPE_NONE;
    }

    // 检查魔数字符串
    if (str_len != strlen(TEFKERNEL_MAGIC_STRING)) {
        return CONNECTION_TYPE_NONE;
    }

    if (memcmp(&data[offset], TEFKERNEL_MAGIC_STRING, str_len) != 0) {
        return CONNECTION_TYPE_NONE;
    }
    offset += str_len;

    // 读取gameRelease
    offset += 4;  // 跳过gameRelease

    // 读取版本号
    uint64_t version = 0;
    for (int i = 0; i < 8; i++) {
        version |= (uint64_t)data[offset++] << (i * 8);
    }

    // 检查版本
    if (version != TEFKERNEL_VERSION_CODE) {
        g_error_message_id = ERROR_VERSION;
        return CONNECTION_TYPE_VERSION_MISMATCH;
    }

    // 验证哈希
    uint64_t mods_hash = 0;
    uint64_t modloaders_hash = 0;
    uint64_t modules_hash = 0;
    uint64_t plugins_hash = 0;

    for (int i = 0; i < 8; i++) mods_hash |= (uint64_t)data[offset++] << (i * 8);
    for (int i = 0; i < 8; i++) modloaders_hash |= (uint64_t)data[offset++] << (i * 8);
    for (int i = 0; i < 8; i++) modules_hash |= (uint64_t)data[offset++] << (i * 8);
    for (int i = 0; i < 8; i++) plugins_hash |= (uint64_t)data[offset++] << (i * 8);

    if (mods_hash != MODS_HASH || modloaders_hash != MODLOADERS_HASH ||
        modules_hash != MODULES_HASH || plugins_hash != PLUGINS_HASH) {
        g_error_message_id = ERROR_MOD_REQUIRED;
        return CONNECTION_TYPE_BAD_HASH;
    }

    return CONNECTION_TYPE_TEFKERNEL;
}

// 检查是否是原版连接包
static bool is_vanilla_connection_packet(const uint8_t* data, const int data_len) {
    int offset = 1;
    if (offset + 1 >= data_len) {
        return false;
    }

    const uint8_t str_len = data[offset++];
    if (offset + str_len > data_len) {
        return false;
    }

    const char* str = (char*)&data[offset];
    const char* terraria_prefix = "Terraria";

    if (str_len < strlen(terraria_prefix)) {
        return false;
    }

    return strncmp(str, terraria_prefix, strlen(terraria_prefix)) == 0;
}

void terraria_netmanager_init() {
    // Hook ProcessData 方法
    patch_handle_t message_buffer_class = patchlib_type_get_type("Terraria", "MessageBuffer");
    if (message_buffer_class == PATCH_NULL) {
        TEKLOG_ERROR("Failed to get MessageBuffer class");
        return;
    }

    // 获取 ProcessData 方法
    patch_handle_t process_data_method = patchlib_type_get_method_by_param_count(message_buffer_class, "ProcessData", 3);
    if (process_data_method == PATCH_NULL) {
        TEKLOG_ERROR("Failed to get ProcessData method");
        return;
    }

    void* process_data_target = patchlib_method_get_pointer(process_data_method);
    if (process_data_target == NULL) {
        TEKLOG_ERROR("Failed to get ProcessData method pointer");
        return;
    }

    const int ret = DobbyHook(process_data_target, (void*)process_data_hook, (void**)&orig_process_data);
    if (ret != 0) {
        TEKLOG_ERROR("Failed to hook ProcessData: %d", ret);
        return;
    }

    // Hook SendData
    DobbyHook(patchlib_method_get_pointer(patchlib_type_get_method_by_param_count(
        patchlib_type_get_type("Terraria", "NetMessage"), "SendData", 11)),
        (void*)send_data_hook, (void**)&orig_send_data);

    // Hook Client.Send
    patch_handle_t client_class = patchlib_type_get_type("Telepathy", "Client");
    if (client_class == PATCH_NULL) {
        TEKLOG_ERROR("Failed to get Client class");
        return;
    }

    patch_handle_t send_method = patchlib_type_get_method_by_param_count(client_class, "Send", 2);
    if (send_method == PATCH_NULL) {
        TEKLOG_ERROR("Failed to get Client.Send method");
        return;
    }

    void* send_target = patchlib_method_get_pointer(send_method);
    if (send_target == NULL) {
        TEKLOG_ERROR("Failed to get Client.Send method pointer");
        return;
    }

    const int ret2 = DobbyHook(send_target, (void*)client_send_hook, (void**)&orig_client_send);
    if (ret2 != 0) {
        TEKLOG_ERROR("Failed to hook Client.Send: %d", ret2);
        return;
    }

    network_text_from_literal = patchlib_type_get_method_by_param_count(patchlib_type_get_type("Terraria.Localization", "NetworkText"),
    "FromLiteral", 1
    );

    TEKLOG_INFO("Terraria netmanager initialized successfully");
}

// ProcessData 钩子 - 处理接收到的连接包
void process_data_hook(patch_handle_t instance, il2cpp_array_t* messageData, int length, int* messageType) {
    if (!messageData || length <= 0) {
        orig_process_data(instance, messageData, length, messageType);
        return;
    }

    // 获取数据指针
    const uint8_t* data = (uint8_t*)((uintptr_t)messageData + sizeof(il2cpp_array_t));

    // 获取消息类型
    const uint8_t msg_type = data[0];
    *messageType = msg_type;

    // 只处理连接包 (类型1)
    if (msg_type == 1) {
        // 重置连接状态
        g_connection_type = CONNECTION_TYPE_NONE;
        g_error_message_id = ERROR_NONE;

        // 检查是否是TEFKernel包
        const connection_type_t conn_type = parse_tefconnection_packet(data, length);

        if (conn_type != CONNECTION_TYPE_NONE) {
            g_connection_type = conn_type;
            TEKLOG_INFO("Received TEFKernel connection packet, type: %d", conn_type);

            if (conn_type == CONNECTION_TYPE_TEFKERNEL) {
                // TEFKernel客户端，创建原版包格式以通过连接
                int game_release = 0;
                patchlib_field_get_value(patchlib_type_get_field(
                    patchlib_type_get_type("Terraria", "Main"), "curRelease"),
                    PATCH_NULL, &game_release);

                // 创建原版连接包
                char vanilla_string[32];
                const int vanilla_str_len = snprintf(vanilla_string, sizeof(vanilla_string), "Terraria%d", game_release);

                // 原版包大小：2(总长度) + 1(类型) + 1(字符串长度) + 字符串
                const int vanilla_length = 1 + 1 + vanilla_str_len;

                // 创建原版格式数组
                il2cpp_array_t* vanilla_array = patchlib_array_create(vanilla_length,
                    patchlib_get_basic_type(PATCH_UINT8));
                if (!vanilla_array) {
                    TEKLOG_ERROR("Failed to create vanilla connection packet");
                    orig_process_data(instance, messageData, length, messageType);
                    return;
                }

                uint8_t* vanilla_data = (uint8_t*)((uintptr_t)vanilla_array + sizeof(il2cpp_array_t));
                int vanilla_offset = 0;

                // 写入消息类型
                vanilla_data[vanilla_offset++] = 1;

                // 写入字符串长度
                vanilla_data[vanilla_offset++] = vanilla_str_len;

                // 写入原版字符串
                memcpy(&vanilla_data[vanilla_offset], vanilla_string, vanilla_str_len);

                TEKLOG_INFO("Converted TEFKernel connection to vanilla format: %s", vanilla_string);

                // 调用原始函数处理原版包
                orig_process_data(instance, vanilla_array, vanilla_length, messageType);
                return;
            }

            // 版本不匹配或哈希错误，让原始函数处理错误
            orig_process_data(instance, messageData, length, messageType);
            return;
        }

        // 检查是否是原版包
        if (is_vanilla_connection_packet(data, length)) {
            g_connection_type = CONNECTION_TYPE_VANILLA;
            TEKLOG_INFO("Received vanilla connection packet");

            // 新包大小：2(总长度) + 1(类型) + 1(字符串长度) + 字符串
            const int new_length = 2 + 1 + 1;

            // 创建新数组
            il2cpp_array_t* new_array = patchlib_array_create(new_length,
                patchlib_get_basic_type(PATCH_UINT8));
            if (!new_array) {
                TEKLOG_ERROR("Failed to create modified connection packet");
                orig_process_data(instance, messageData, length, messageType);
                return;
            }

            uint8_t* new_data = (uint8_t*)((uintptr_t)new_array + sizeof(il2cpp_array_t));
            int new_offset = 0;


            new_data[new_offset++] = (uint8_t)(new_length & 0xFF);
            new_data[new_offset++] = (uint8_t)((new_length >> 8) & 0xFF);

            // 写入消息类型
            new_data[new_offset++] = 1;

            // 写入字符串长度
            new_data[new_offset++] = 1;

            new_data[new_offset] = 'a';

            TEKLOG_INFO("Converted vanilla connection to Null format");


            // 既不是TEFKernel包也不是原版包
            TEKLOG_WARN("Received unknown connection packet format");
            g_connection_type = CONNECTION_TYPE_NONE;
            g_error_message_id = ERROR_MOD_REQUIRED;

            // 调用原始函数处理修改后的包
            orig_process_data(instance, new_array, new_length, messageType);
            return;
        }

        // 既不是TEFKernel包也不是原版包
        TEKLOG_WARN("Received unknown connection packet format");
        g_connection_type = CONNECTION_TYPE_NONE;
        g_error_message_id = ERROR_MOD_REQUIRED;
    }

    // 其他包类型或非连接包，正常处理
    orig_process_data(instance, messageData, length, messageType);
}

// SendData 钩子 - 处理发送错误消息
void send_data_hook(int msgType, int remoteClient, int ignoreClient,
                   void* text, int number, float number2, float number3, float number4,
                   int number5, int number6, int number7) {

    // 检查是否是发送连接包
    if (msgType == 1) {
        TEKLOG_INFO("Send connection packet");
        g_send_connection = true;
    }

    // 检查是否发送错误消息
    if (msgType == 2 && g_error_message_id != ERROR_NONE) {
        TEKLOG_DEBUG("SendData: msgType=2, g_error_message_id=%d", g_error_message_id);

        void* error_text = NULL;

        if (g_error_message_id == ERROR_MOD_REQUIRED)
            error_text = ((void*(*)(void*))patchlib_method_get_pointer(network_text_from_literal))(patchlib_string_create("非TEFKernel客户端"));

        if (error_text) {
            orig_send_data(msgType, remoteClient, ignoreClient,
                              error_text, number, number2, number3, number4,
                              number5, number6, number7);
            return;
        }
        // 这里可以修改错误消息
        // 例如，如果g_error_message_id是ERROR_MOD_REQUIRED，可以发送自定义消息
    }

    orig_send_data(msgType, remoteClient, ignoreClient,
                  text, number, number2, number3, number4,
                  number5, number6, number7);
}

// Client.Send 钩子 - 修改发送的连接包
bool client_send_hook(patch_handle_t* this, void* data, int length) {
    bool result = false;

    if (g_send_connection) {
        TEKLOG_INFO("Creating TEFKernel connection packet");

        // 定义版本和哈希值
        int32_t game_release = 0;
        patchlib_field_get_value(patchlib_type_get_field(
            patchlib_type_get_type("Terraria", "Main"), "curRelease"),
            PATCH_NULL, &game_release);

        // 计算新包的总长度
        const int str_len = strlen(TEFKERNEL_MAGIC_STRING);
        const uint16_t new_total_length = 2 + 1 + 1 + str_len + 4 + (5 * 8);

        TEKLOG_INFO("New TEFKernel packet:");
        TEKLOG_INFO("  Magic: %s", TEFKERNEL_MAGIC_STRING);
        TEKLOG_INFO("  Game release: %u", game_release);
        TEKLOG_INFO("  Total length: %u bytes", new_total_length);

        // 创建新的byte[]数组
        il2cpp_array_t* new_array = patchlib_array_create(new_total_length,
            patchlib_get_basic_type(PATCH_UINT8));
        if (!new_array) {
            TEKLOG_ERROR("Failed to create new array!");
            g_send_connection = false;
            return orig_client_send(this, data, length);
        }

        // 计算新数组的数据指针
        uint8_t* new_array_data = (uint8_t*)((uintptr_t)new_array + sizeof(il2cpp_array_t));
        size_t offset = 0;

        // 写入总长度 (小端序)
        new_array_data[offset++] = (uint8_t)(new_total_length & 0xFF);
        new_array_data[offset++] = (uint8_t)((new_total_length >> 8) & 0xFF);

        // 写入包类型 (0x01 = 连接包)
        new_array_data[offset++] = 0x01;

        // 写入字符串长度
        new_array_data[offset++] = (uint8_t)str_len;

        // 写入字符串内容
        memcpy(&new_array_data[offset], TEFKERNEL_MAGIC_STRING, str_len);
        offset += str_len;

        // 写入gameRelease (uint32, 小端序)
        for (int i = 0; i < 4; i++) {
            new_array_data[offset++] = game_release >> (i * 8) & 0xFF;
        }

        // 写入所有uint64值 (小端序)

        for (int v = 0; v < 5; v++) {
            for (int i = 0; i < 8; i++) {
                const uint64_t values[] = {
                    TEFKERNEL_VERSION_CODE,
                    MODS_HASH,
                    MODLOADERS_HASH,
                    MODULES_HASH,
                    PLUGINS_HASH
                };
                new_array_data[offset++] = (values[v] >> (i * 8)) & 0xFF;
            }
        }

        // 验证写入的字节数
        if (offset == new_total_length) {
            TEKLOG_INFO("TEFKernel packet created successfully (%lu bytes)", offset);
            result = orig_client_send(this, (void*)new_array, new_total_length);
            TEKLOG_INFO("Send result: %s", result ? "success" : "failed");
        } else {
            TEKLOG_ERROR("Byte count mismatch: wrote %lu, expected %u", offset, new_total_length);
            result = orig_client_send(this, data, length);
        }

        g_send_connection = false;
        return result;
    }

    // 对于非连接包，使用原始数据
    result = orig_client_send(this, data, length);
    return result;
}