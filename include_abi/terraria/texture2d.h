/*******************************************************************************
 * File: texture2d
 * Project: tefkernel
 * Created: 2026/4/12
 * Author: eternalfuture-e38299
 * Github: https://github.com/eternalfuture-e38299
 *
 * MIT License
 *
 * Copyright (c) 2026 eternalfuture-e38299
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *******************************************************************************/

#ifndef TEFKERNEL_TEXTURE2D_H
#define TEFKERNEL_TEXTURE2D_H

#include "../patchlib/type.h"

#ifdef __cplusplus
extern "C" {

#endif


// 平台特定的格式值定义
#ifdef __ANDROID__

typedef enum {
    TEXTURE_FORMAT_RGBA32 = 4, // Unity: RGBA32
    TEXTURE_FORMAT_BGR565 = 7, // Unity: RGB565
    TEXTURE_FORMAT_BGRA4444 = 13, // Unity: RGBA4444
    TEXTURE_FORMAT_DXT1 = 10, // Unity: DXT1
    TEXTURE_FORMAT_DXT5 = 12, // Unity: DXT5
    TEXTURE_FORMAT_ALPHA8 = 1, // Unity: Alpha8
    TEXTURE_FORMAT_SINGLE = 18, // Unity: RFloat
    TEXTURE_FORMAT_VECTOR2 = 19, // Unity: RGFloat
    TEXTURE_FORMAT_VECTOR4 = 20, // Unity: RGBAFloat
    TEXTURE_FORMAT_HALF_SINGLE = 15, // Unity: RHalf
    TEXTURE_FORMAT_HALF_VECTOR2 = 16, // Unity: RGHalf
    TEXTURE_FORMAT_HALF_VECTOR4 = 17, // Unity: RGBAHalf
    TEXTURE_FORMAT_COLOR_BGRA_EXT = 14, // Unity: BGRA32
    TEXTURE_FORMAT_BC7_EXT = 25, // Unity: BC7
    TEXTURE_FORMAT_BYTE_EXT = 63, // Unity: R8
    TEXTURE_FORMAT_USHORT_EXT = 9, // Unity: R16
    TEXTURE_FORMAT_RG32 = 72, // Unity: RG32
    TEXTURE_FORMAT_RGBA64 = 74 // Unity: RGBA64
} texture_format_t;

#else

typedef enum {
    TEXTURE_FORMAT_RGBA32 = 0,
    TEXTURE_FORMAT_BGR565 = 1,
    TEXTURE_FORMAT_BGRA4444 = 3,
    TEXTURE_FORMAT_DXT1 = 4,
    TEXTURE_FORMAT_DXT5 = 6,
    TEXTURE_FORMAT_RG32 = 10,
    TEXTURE_FORMAT_RGBA64 = 11,
    TEXTURE_FORMAT_ALPHA8 = 12,
    TEXTURE_FORMAT_SINGLE = 13,
    TEXTURE_FORMAT_VECTOR2 = 14,
    TEXTURE_FORMAT_VECTOR4 = 15,
    TEXTURE_FORMAT_HALF_SINGLE = 16,
    TEXTURE_FORMAT_HALF_VECTOR2 = 17,
    TEXTURE_FORMAT_HALF_VECTOR4 = 18,
    TEXTURE_FORMAT_COLOR_BGRA_EXT = 20,
    TEXTURE_FORMAT_BC7_EXT = 23,
    TEXTURE_FORMAT_BYTE_EXT = 25,
    TEXTURE_FORMAT_USHORT_EXT = 26
} texture_format_t;
#endif

/**
 * @brief 创建纹理
 * @param width 宽度
 * @param height 高度
 * @param texture_format 纹理类型
 * @param data 纹理数据
 * @param data_size 数据大小
 * @return 纹理实例
 * @warning 该函数不会为你处理纹理数据(自动反转y)
 */
DEFINE_FUNCTION(patch_handle_t, terraria_texture2d_create, int width, int height, texture_format_t texture_format, void* data, size_t data_size)

/**
 * @brief 获取纹理宽度
 * @param texture2d 纹理实例
 * @return 宽度
 */
DEFINE_FUNCTION(int, terraria_texture2d_get_width, patch_handle_t texture2d)

/**
 * @brief 获取纹理高度
 * @param texture2d 纹理实例
 * @return 高度
 */
DEFINE_FUNCTION(int, terraria_texture2d_get_height, patch_handle_t texture2d)

/**
 * @brief 获取texture2d类句柄
 * @return 类句柄
 */
DEFINE_FUNCTION(patch_handle_t, terraria_texture2d_get_class)

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_TEXTURE2D_H
