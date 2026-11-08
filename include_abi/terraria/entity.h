/*******************************************************************************
 * File: entity
 * Project: tefkernel
 * Created: 2026/11/5
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

#ifndef TEFKERNEL_ENTITY_H
#define TEFKERNEL_ENTITY_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Terraria 领域句柄公共头
 *
 * 字段语义：
 *   parent_modloader_id  所属 modloader 的 pkg_id
 *   parent_id            所属 mod 的 pkg_id
 *   internal_name        mod 内唯一名称
 *   runtime_id           由内核分配的运行时 ID（注册前为 0 / -1）
 */
typedef struct terraria_entity_handle_t {
    const char *parent_modloader_id;
    const char *parent_id;
    const char *internal_name;
    int runtime_id;
} terraria_entity_handle_t;

/**
 * @brief 把任意 manager 句柄转成公共头指针
 * @note 句柄必须把 terraria_entity_handle_t 放在偏移 0
 */
#define TEF_ENTITY(h) ((terraria_entity_handle_t *)(h))

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_ENTITY_H
