/*******************************************************************************
 * tefkernel - android_test
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
 * Created: 2025/12/28
 *******************************************************************************/

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <sstream>

#include "internal/kernel_state.h"
#include "internal/log.h"
#include "patchlib/method.h"
#include "patchlib/type.h"
#include "patchlib/android/il2cpp_api.h"


// 辅助函数: 创建并填充纹理
patch_handle_t CreateAndFillTexture2D(int width, int height, int format, const char* prefix) {
    patch_handle_t texture2d_class = patchlib_type_get_type("UnityEngine", "Texture2D");

    // 获取Texture2D构造函数
    patch_handle_t texture2d_ctor = patchlib_type_get_method_by_param_count(texture2d_class, ".ctor", 4);
    if (!texture2d_ctor) {
        TEKLOG_ERROR("%s: 未找到 Texture2D 构造函数", prefix);
        return 0;
    }

    typedef void (*Texture2DCtorFunc)(void* texture, int width, int height, int textureFormat, bool mipChain);
    Texture2DCtorFunc pTexture2DCtor = (Texture2DCtorFunc)patchlib_method_get_pointer(texture2d_ctor);

    // 创建Texture2D实例
    patch_handle_t texture = patchlib_type_new_instance(texture2d_class);
    if (!texture) {
        TEKLOG_ERROR("%s: 创建 Texture2D 实例失败", prefix);
        return 0;
    }

    // 直接调用构造函数
    pTexture2DCtor(texture, width, height, format, false);
    TEKLOG_INFO("%s: 创建Texture2D: %dx%d", prefix, width, height);

    // 使用SetPixel填充
    patch_handle_t setPixel_method = patchlib_type_get_method_by_param_count(texture2d_class, "SetPixel", 3);
    if (setPixel_method) {
        typedef void (*SetPixelFunc)(void* texture, int x, int y, void* color);
        SetPixelFunc pSetPixel = (SetPixelFunc)patchlib_method_get_pointer(setPixel_method);

        patch_handle_t color_type = patchlib_type_get_type("UnityEngine", "Color");
        if (!color_type) {
            TEKLOG_ERROR("%s: 未找到 Color 类型", prefix);
            return texture;
        }

        // 获取Color构造函数
        patch_handle_t color_ctor = patchlib_type_get_method_by_param_count(color_type, ".ctor", 4);
        if (!color_ctor) {
            TEKLOG_ERROR("%s: 未找到 Color 构造函数", prefix);
            return texture;
        }

        typedef void (*ColorCtorFunc)(void* color, float r, float g, float b, float a);
        ColorCtorFunc pColorCtor = (ColorCtorFunc)patchlib_method_get_pointer(color_ctor);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // 创建Color实例
                patch_handle_t color = patchlib_type_new_instance(color_type);
                if (!color) {
                    TEKLOG_ERROR("%s: 创建 Color 实例失败", prefix);
                    continue;
                }

                // 直接调用Color构造函数
                float r = 0.0f;  // 左上角红色
                float g = 0.0f;  // 右上角绿色
                float b = 0.0f;  // 左下角蓝色
                float a = 0.0f;  // 右下角半透明白色

                pColorCtor(color, r, g, b, a);

                // 直接调用SetPixel
                pSetPixel(texture, x, y, color);
            }
        }
        TEKLOG_INFO("%s: SetPixel填充完成", prefix);
    }

    // 应用纹理
    patch_handle_t apply_method = patchlib_type_get_method_by_param_count(texture2d_class, "Apply", 0);
    if (apply_method) {
        typedef void (*Texture2DApplyFunc)(void* texture);
        Texture2DApplyFunc pApply = (Texture2DApplyFunc)patchlib_method_get_pointer(apply_method);
        pApply(texture);
        TEKLOG_INFO("%s: 纹理Apply完成", prefix);
    }

    return texture;
}

// 辅助函数: 导出纹理数据
void ExportTextureData(patch_handle_t texture, const char* prefix, int width, int height) {
    typedef void* (*GetWritableImageDataFunc)(void* texture, int frame);
    typedef long (*GetRawImageDataSizeFunc)(void* texture);

    // 获取方法
    patch_handle_t texture2d_class = patchlib_type_get_type("UnityEngine", "Texture2D");

    patch_handle_t getWritableImageData_method = patchlib_type_get_method_by_param_count(
        texture2d_class, "GetWritableImageData", 1);
    patch_handle_t getRawImageDataSize_method = patchlib_type_get_method_by_param_count(
        texture2d_class, "GetRawImageDataSize", 0);

    if (!getWritableImageData_method || !getRawImageDataSize_method) {
        TEKLOG_ERROR("%s: 未找到纹理数据方法", prefix);
        return;
    }

    GetWritableImageDataFunc pGetWritableImageData = (GetWritableImageDataFunc)patchlib_method_get_pointer(getWritableImageData_method);
    GetRawImageDataSizeFunc pGetRawImageDataSize = (GetRawImageDataSizeFunc)patchlib_method_get_pointer(getRawImageDataSize_method);

    if (!pGetWritableImageData || !pGetRawImageDataSize) {
        TEKLOG_ERROR("%s: 获取函数指针失败", prefix);
        return;
    }

    // 获取数据大小
    long data_size = pGetRawImageDataSize(texture);
    TEKLOG_INFO("%s: GetRawImageDataSize返回: %ld字节", prefix, data_size);

    // 获取数据指针
    int frame = 0;
    void* pixel_data = pGetWritableImageData(texture, frame);
    TEKLOG_INFO("%s: GetWritableImageData返回指针: %p", prefix, pixel_data);

    if (pixel_data && data_size > 0) {
        // 输出前几个字节
        unsigned char* data = (unsigned char*)pixel_data;
        TEKLOG_INFO("%s: 前16字节数据:", prefix);
        for (int i = 0; i < 16 && i < data_size; i++) {
            TEKLOG_INFO("  [%d]: 0x%02X (%d)", i, data[i], data[i]);
        }

        // 保存文件
        char filename[128];
        snprintf(filename, sizeof(filename),
            "/sdcard/%stexture_%dx%d_%ld.raw",
            prefix, width, height, time(NULL));

        FILE* file = fopen(filename, "wb");
        if (file) {
            size_t written = fwrite(pixel_data, 1, data_size, file);
            fclose(file);

            if (written == data_size) {
                TEKLOG_INFO("%s: 文件已保存: %s (%ld字节)", prefix, filename, data_size);
            } else {
                TEKLOG_ERROR("%s: 写入不完整: %zu/%ld字节", prefix, written, data_size);
            }
        } else {
            TEKLOG_ERROR("%s: 无法创建文件: %s", prefix, filename);
        }
    } else {
        TEKLOG_ERROR("%s: 无效的数据: pointer=%p, size=%ld", prefix, pixel_data, data_size);
    }
}

void Initialize_AlmostEverything_pre(patch_handle_t instance, void **args, const patch_method_signature_t *sig_info) {
    TEKLOG_INFO("使用 GetWritableImageData 导出纹理");

    // 1. 定义UnityEngine函数原型
    typedef void* (*GetWritableImageDataFunc)(void* texture, int frame);
    typedef long (*GetRawImageDataSizeFunc)(void* texture);
    typedef void (*Texture2DApplyFunc)(void* texture);
    typedef void (*SetPixelFunc)(void* texture, int x, int y, void* color);
    typedef void (*Texture2DCtorFunc)(void* texture, int width, int height, int textureFormat, bool mipChain);
    typedef void* (*GetRawTextureDataFunc)(void* texture);

    // 2. 获取函数指针
    patch_handle_t texture2d_class = patchlib_type_get_type("UnityEngine", "Texture2D");
    if (!texture2d_class) {
        TEKLOG_ERROR("未找到 Texture2D 类型");
        return;
    }

    // 获取GetRawTextureData方法
    patch_handle_t getRawTextureData_method = patchlib_type_get_method_by_param_count(
        texture2d_class, "GetRawTextureData", 0);
    if (getRawTextureData_method) {
        TEKLOG_INFO("找到 GetRawTextureData 方法");
    }

    // 获取LoadImage方法
    patch_handle_t loadImage_method = patchlib_type_get_method_by_param_count(
        texture2d_class, "LoadImage", 1);
    if (loadImage_method) {
        TEKLOG_INFO("找到 LoadImage 方法");
    }

    // 3. 尝试多种方法创建和填充纹理
    int texWidth = 2;
    int texHeight = 2;
    int expected_size = texWidth * texHeight * 4;

    // 方法1: 使用SetPixel填充
    TEKLOG_INFO("=== 方法1: 使用SetPixel填充 ===");
    patch_handle_t texture1 = CreateAndFillTexture2D(texWidth, texHeight, 4, "method1_");
    if (texture1) {
        ExportTextureData(texture1, "method1_", texWidth, texHeight);
    }
}

typedef struct Il2CppException
{
    void* a;
    void* b;
    void* className;
    void* message;
    void* _data;
    struct Il2CppException* inner_ex;
    void* _helpURL;
    void* trace_ips;
    void* stack_trace;
    void* remote_stack_trace;
    int remote_stack_index;
    void* _dynamicMethods;
    void* hresult;
    void* source;
    void* safeSerializationManager;
    void* captured_traces;
    void* native_trace_ips;
    int32_t caught_in_unmanaged;
} Il2CppException;

void Initialize_AlmostEverything_post(patch_handle_t instance, void **args, void *result, const patch_method_signature_t *sig_info) {

}

void SetDefaults_post(patch_handle_t instance, void **args, void *result, const patch_method_signature_t *sig_info) {
    TEKLOG_DEBUG("SetDefaults_post entered: instance=%p, args=%p, result=%p", instance, args, result);

    // 获取 buyPrice 方法
    patch_handle_t buyPrice_method = patchlib_type_get_method_by_param_count(
        patchlib_type_get_type("Terraria", "Item"),
        "buyPrice",
        4  // 4个参数：platinum, gold, silver, copper
    );

    if (!patchlib_is_valid(buyPrice_method)) {
        TEKLOG_ERROR("Failed to get buyPrice method");
        return;
    }

    TEKLOG_DEBUG("Found buyPrice method: %p", buyPrice_method);

    // 调用方法（修正：使用 buyPrice_args，不是外部函数的 args）
    int buyPrice_result = 0;
    bool success = patchlib_method_invoke(
        buyPrice_method,
        instance,           // 实例对象
        &buyPrice_result,   // 返回值指针
        1, 1, 1, 1
    );

    if (success) {
        TEKLOG_DEBUG("buyPrice called successfully, result=%d", buyPrice_result);
    } else {
        TEKLOG_ERROR("buyPrice method invocation failed");
    }

    TEKLOG_DEBUG("SetDefaults_post completed");
}

void start_test() {
    /*patch_handle_t method = patchlib_type_get_method_by_param_count(
        patchlib_type_get_type("Terraria", "Main"),
        "Initialize_AlmostEverything",
        0
    );
    */
    // patchlib_install_prepost_hook(method, (prefix_callback_t)Initialize_AlmostEverything_pre, (postfix_callback_t)Initialize_AlmostEverything_post);

    patch_handle_t method = patchlib_type_get_method_by_param_count(
            patchlib_type_get_type("Terraria", "Item"),
            "SetDefaults",
            2
        );
    patchlib_install_prepost_hook(method, NULL, SetDefaults_post);
}