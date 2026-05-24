/*******************************************************************************
 * tefkernel - method
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
 * Created: 2025/11/23
 *******************************************************************************/

#include "patchlib/method.h"

#include <stdlib.h>

#include "internal/log.h"
#include "patchlib/il2cpp_api.h"

bool patchlib_method_invoke_args(patch_handle_t method, patch_handle_t instance,
                                 void *return_value, void** args) {
    TEKLOG_DEBUG("patchlib_method_invoke_args (Desktop): method=%p, instance=%p, return_value=%p",
                 method, instance, return_value);

    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle");
        return false;
    }

    // 验证实例方法调用
    if (!patchlib_method_is_static(method) && !patchlib_is_valid(instance)) {
        TEKLOG_ERROR("Instance method requires valid instance");
        return false;
    }

    // 直接调用 il2cpp_runtime_invoke
    // 注意：il2cpp_runtime_invoke 返回 bool 表示是否成功
    // return_value 用于存储返回值
    const bool result = il2cpp_method_invoke(method, instance, args, return_value);

    if (!result) {
        TEKLOG_ERROR("il2cpp_runtime_invoke failed");
        return false;
    }

    TEKLOG_DEBUG("Method invocation completed successfully");
    return true;
}

patch_handle_t patchlib_method_make_generic_instance(patch_handle_t method, const tefstd_vector_t *template_types) {
    TEKLOG_DEBUG("patchlib_method_make_generic_instance (Desktop): method=%p", method);

    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle");
        return PATCH_NULL;
    }

    if (!template_types || tefstd_vector_size(template_types) == 0) {
        TEKLOG_ERROR("Invalid template types");
        return PATCH_NULL;
    }

    // 获取泛型参数类型数组
    const size_t type_count = tefstd_vector_size(template_types);
    void** type_array = malloc(sizeof(void*) * type_count);
    if (!type_array) {
        TEKLOG_ERROR("Failed to allocate type array");
        return PATCH_NULL;
    }

    for (size_t i = 0; i < type_count; i++) {
        patch_handle_t* type_handle = tefstd_vector_at(template_types, i);
        if (type_handle && patchlib_is_valid(*type_handle)) {
            type_array[i] = *type_handle;
        } else {
            TEKLOG_ERROR("Invalid type at index %zu", i);
            free(type_array);
            return PATCH_NULL;
        }
    }

    // 直接调用 il2cpp_method_make_generic API
    patch_handle_t result = il2cpp_method_make_generic(method, type_array, (int)type_count);

    free(type_array);

    if (!patchlib_is_valid(result)) {
        TEKLOG_ERROR("il2cpp_method_make_generic failed");
        return PATCH_NULL;
    }

    TEKLOG_DEBUG("Generic method created successfully: %p", result);
    return result;
}



patch_hook_id_t patchlib_install_prepost_hook(
                patch_handle_t method, prefix_callback_t prefix, postfix_callback_t postfix) {

    return 0;
}

bool patchlib_uninstall_hook(patch_hook_id_t hook_id) {
    return 0;
}