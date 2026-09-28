/*******************************************************************************
 * File: item_manager
 * Project: tefkernel
 * Created: 2026/8/26
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

#ifndef TEFKERNEL_ITEM_MANAGER_H
#define TEFKERNEL_ITEM_MANAGER_H

#if IS_TEFKERNEL_BUILD

#include <stdbool.h>

#include "../tef_api.h"
#include "../patchlib/type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct terraria_item_handle_t terraria_item_handle_t;

typedef struct terraria_item_ops_t {
    /**
     * @brief 最早初始化，用于注册回调等前置操作
     * @param current 当前物品句柄
     * @note 此回调在 init_static 之前调用，仅执行一次
     */
    void(*early_init)(terraria_item_handle_t* current);
    
    /**
     * @brief 静态初始化
     * @param current 当前物品句柄
     */
    void(*init_static)(terraria_item_handle_t* current);

    /**
     * @brief 属性设置
     * @param current 当前物品句柄
     * @param instance 当前物品实例
     * @warning 这里的实例会由hook管理器释放
     */
    void(*set_defaults)(terraria_item_handle_t* current, patch_handle_t instance);
    
    /*
     * @brief 使用条件
     * @param current 当前物品句柄
     * @param player_instance 当前人物实例
     * @param item_instance 当前物品实例
     * @warning 这里的实例会由hook管理器释放
     */
    bool(*can_use)(terraria_item_handle_t* current, patch_handle_t player_instance, patch_handle_t item_instance, bool ignore_cursed);

    /**
     * @brief 获取纹理实例(Asset<Texture2d>)
     * @param current 当前物品句柄
     * @return Asset<Texture2d>
     * @warning 不要管理，内核会在加载后自动卸载
     */
    patch_handle_t(*get_texture)(terraria_item_handle_t* current);
} terraria_item_ops_t;

typedef struct terraria_item_handle_t {
    const char* parent_modloader_id; //<< 所属modloader id
    const char* parent_id; //<< 所属mod id
    const char* internal_name; //<< 内部名称
    bool has_tooltip; //<< 是否存在物品介绍
    int runtime_id;  //<< 由内核分配

    terraria_item_ops_t item_ops; //<< 内部逻辑
} terraria_item_handle_t;

/**
 * @brief 注册物品
 * @param item_handle 物品句柄
 * @return 注册结果
 * @warning 内核只会分配runtime_id以及初始化材质资源但不会管理，你需要手动实现注销逻辑
 */
DEFINE_FUNCTION(bool, terraria_item_manager_register_item, terraria_item_handle_t* item_handle)

/**
 * @brief 通过运行时 ID 注销物品
 * @param runtime_id 要注销的物品运行时 ID
 * @return 是否成功
 */
DEFINE_FUNCTION(bool, terraria_item_manager_unregister_item_by_id, int runtime_id)

/**
 * @brief 通过运行时 ID 获取物品句柄
 * @param runtime_id 物品运行时 ID
 * @return 物品句柄指针，不存在返回 NULL
 */
DEFINE_FUNCTION(terraria_item_handle_t*, terraria_item_manager_get_item, int runtime_id)

/**
 * @brief 获取所有自定义物品
 * @return 自定义物品数组
 * @warning 不要写或释放该数组
 */
DEFINE_FUNCTION(tefstd_vector_t*, terraria_item_manager_get_items)

#ifdef __cplusplus
}
#endif
#endif
#endif //TEFKERNEL_ITEM_MANAGER_H