/*******************************************************************************
 * tefkernel - desktop_test
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

#include "internal/log.h"
#include "patchlib/method.h"
#include "patchlib/field.h"
#include "patchlib/struct/array.h"

// 简单版本的 Chest.Assign Hook
void SilentChestAssignHook(patch_handle_t instance, void **args, void *result,
                          const patch_method_signature_t *sig_info) {

    if (!args || !args[0]) return;

    patch_handle_t chest = *(patch_handle_t*)args[0];
    if (!patchlib_is_valid(chest)) return;

    // 获取字段句柄
    static patch_handle_t items_field = -1;
    static patch_handle_t stack_field = -1;

    if (items_field == -1) {
        patch_handle_t chest_type = patchlib_type_get_type("Terraria", "Chest");
        if (patchlib_is_valid(chest_type)) {
            items_field = patchlib_type_get_field(chest_type, "item");
            patchlib_field_free(chest_type);
        }

        patch_handle_t item_type = patchlib_type_get_type("Terraria", "Item");
        if (patchlib_is_valid(item_type)) {
            stack_field = patchlib_type_get_field(item_type, "stack");
            patchlib_field_free(item_type);
        }

        if (items_field == -1 || stack_field == -1) return;
    }

    // 处理物品
    patch_handle_t items_array = -1;
    patchlib_field_get_value(items_field, chest, &items_array);
    if (!patchlib_is_valid(items_array)) return;

    static int maxStack = 9999;
    int modified = 0;

    for (int i = 0; i < 40; i++) {
        patch_handle_t item = -1;
        if (!patchlib_array_at(items_array, i, &item, PATCH_OBJECT)) continue;
        if (!patchlib_is_valid(item)) continue;

        int stack = 0;
        patchlib_field_get_value(stack_field, item, &stack);

        if (stack > 0 && stack < maxStack) {
            patchlib_field_set_value(stack_field, item, &maxStack);
            patchlib_field_set_value(stack_field, item, &maxStack);
            modified++;
        }

        patchlib_field_free(item);
    }

    patchlib_field_free(items_array);

    // 只在有修改时打印
    if (modified > 0) {
        TEKLOG_INFO("修改了 %d 个物品", modified);
    }
}

void Test() {
    TEKLOG_INFO("=== 安装 Chest.Assign Hook ===");

    // 查找 Chest.Assign 方法
    patch_handle_t chest_type = patchlib_type_get_type("Terraria", "Chest");
    if (!patchlib_is_valid(chest_type)) {
        TEKLOG_ERROR("无法获取 Chest 类型");
        return;
    }

    // 使用正确的方法查找
    patch_handle_t assign_method = patchlib_type_get_method(
        chest_type, "Assign");

    if (patchlib_is_valid(assign_method)) {
        TEKLOG_INFO("找到 Chest.Assign 方法，句柄: %d", assign_method);

        if (patchlib_install_prepost_hook(assign_method, NULL, SilentChestAssignHook)) {
            TEKLOG_INFO("成功安装 Chest.Assign Hook");
        } else {
            TEKLOG_ERROR("安装 Chest.Assign Hook 失败");
        }

        patchlib_field_free(assign_method);
    } else {
        TEKLOG_ERROR("找不到 Chest.Assign 方法");

        // 尝试另一种查找方法
        TEKLOG_INFO("尝试通过参数数量查找...");
        assign_method = patchlib_type_get_method_by_param_count(
            chest_type, "Assign", 1);

        if (patchlib_is_valid(assign_method)) {
            TEKLOG_INFO("通过参数数量找到 Chest.Assign 方法，句柄: %d", assign_method);

            if (patchlib_install_prepost_hook(assign_method, NULL, SilentChestAssignHook)) {
                TEKLOG_INFO("成功安装 Chest.Assign Hook");
            } else {
                TEKLOG_ERROR("安装 Chest.Assign Hook 失败");
            }

            patchlib_field_free(assign_method);
        } else {
            TEKLOG_ERROR("通过参数数量也找不到 Chest.Assign 方法");
        }
    }

    patchlib_field_free(chest_type);
    TEKLOG_INFO("=== Hook 安装完成 ===");
}