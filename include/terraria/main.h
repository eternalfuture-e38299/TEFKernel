/*******************************************************************************
 * File: main
 * Project: tefkernel
 * Created: 2026/7/24
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

#ifndef TEFKERNEL_MAIN_H
#define TEFKERNEL_MAIN_H

#include "../tef_api.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取游戏版本号
 * @return 当前版本号
 */
DEFINE_FUNCTION(int, terraria_main_get_cur_release)

/**
 * @brief 获取屏幕实例
 * @return 屏幕实例
 */
DEFINE_FUNCTION(patch_handle_t, terraria_main_get_graphics_device)

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_MAIN_H
