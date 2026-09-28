/*******************************************************************************
 * File: recipe_manager
 * Project: tefkernel
 * Created: 2026/8/30
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

#ifndef TEFKERNEL_RECIPE_MANAGER_H
#define TEFKERNEL_RECIPE_MANAGER_H

#include "../tef_api.h"
#include "../patchlib/type.h"

#ifdef __cplusplus
extern "C" {
#endif


// 完全自由的回调函数 - 让Mod开发者自己发挥
typedef void (*terraria_recipe_manager_addrecipes_t)();

// 便捷函数

/**
 * @brief 获取当前正在编辑的配方实例指针
 * @return patch_handle_t 当前配方的句柄，可用于直接操作Recipe字段
 * 
 * @note 返回的句柄指向全局currentRecipe对象
 *       可以直接通过该句柄访问createItem、requiredItem等底层字段
 */
DEFINE_FUNCTION(patch_handle_t, terraria_recipe_manager_get_current_recipe)

/**
 * @brief 设置配方产物
 * @param item_id 产物物品ID
 * @param stack   产物数量
 * 
 * @note 等同于 currentRecipe.createItem.SetDefaults(item_id);
 *       currentRecipe.createItem.stack = stack;
 */
DEFINE_FUNCTION(void, terraria_recipe_set_result, int item_id, int stack)

/**
 * @brief 设置配方材料
 * @param materials 材料数组，元素为成对的 (item_id, stack)
 * @param count     材料种类数量（即成对数据的组数）
 * 
 * @note 数组格式：每两个元素为一组，分别表示 物品ID 和 数量
 *       例如设置2种材料：
 *       int mats[] = {4230, 5, 225, 2};
 *       terraria_recipe_set_material(mats, 2);
 *       表示5个蛛丝(4230) + 2个藤蔓(225)
 * 
 * @warning 材料种类数量不能超过maxRequirements（通常为15）
 *          每种材料的stack不能超过物品最大堆叠数
 *          数组长度必须等于 count * 2
 */
DEFINE_FUNCTION(void, terraria_recipe_set_material, int* materials, int count)

/**
 * @brief 设置合成所需的合成站（工作台、熔炉等）
 * @param tile_id 合成站的物块ID，-1表示不需要合成站
 * 
 * @note 常用合成站ID：
 *       - 13: 工作台 (Work Bench)
 *       - 17: 熔炉 (Furnace)  
 *       - 37: 铁砧 (Iron Anvil)
 *       - 77: 恶魔祭坛 (Demon Altar)
 *       - 94: 秘银砧 (Mythril Anvil)
 *       - 412: 血肉克隆缸 (Flesh Cloning Vat)
 *       
 *       等同于 currentRecipe.requiredTile = tile_id;
 */
DEFINE_FUNCTION(void, terraria_recipe_set_station, int tile_id)

/**
 * @brief 将当前配置的配方提交注册到游戏中
 * 
 * @note 调用此函数后，当前配方会被正式添加到游戏配方列表中
 *       每次调用前需确保已通过terraria_recipe_set_result等函数完成配置
 *       等同于调用 AddRecipe()
 * 
 * @warning 提交后当前配方配置不会被自动清除
 *          如需添加新配方，请手动重置相关字段
 */
DEFINE_FUNCTION(void, terraria_recipe_add)

/**
 * @brief 注册一个配方添加回调函数
 * @param callback 回调函数指针，类型为 terraria_recipe_manager_addrecipes
 * 
 * @note 注册的回调会在适当的时机被批量执行
 *       多个Mod可以分别注册自己的回调，互不干扰
 *       回调函数内部应调用terraria_recipe_set_result等函数配置并提交配方
 * 
 * @code
 * // 注册示例
 * void my_recipe_callback(void) {
 *     terraria_recipe_set_result(4182, 1);
 *     terraria_recipe_set_material(2, 4230, 5, 225, 2);
 *     terraria_recipe_set_station(412);
 *     terraria_recipe_add();
 * }
 * terraria_recipe_manager_register_callback(my_recipe_callback);
 * @endcode
 */
DEFINE_FUNCTION(void, terraria_recipe_manager_register_callback, terraria_recipe_manager_addrecipes_t callback)

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_RECIPE_MANAGER_H
