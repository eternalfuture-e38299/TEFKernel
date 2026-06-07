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
 * along with this program. NoCMakeLists.txtyet, see <https://www.gnu.org/licenses/>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2025/11/23
 *******************************************************************************/

#include "patchlib/method.h"

#include <stdlib.h>
#include <string.h>

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

    patch_handle_t result = il2cpp_method_make_generic(method, type_array, (int)type_count);

    free(type_array);

    if (!patchlib_is_valid(result)) {
        TEKLOG_ERROR("il2cpp_method_make_generic failed");
        return PATCH_NULL;
    }

    TEKLOG_DEBUG("Generic method created successfully: %p", result);
    return result;
}

patch_hook_id_t patchlib_install_prepost_hook(patch_handle_t method, const prefix_callback_t prefix, const postfix_callback_t postfix) {
    TEKLOG_DEBUG("=== [START] patchlib_install_prepost_hook ===");
    TEKLOG_DEBUG("Parameters:");
    TEKLOG_DEBUG("  method: %p", method);
    TEKLOG_DEBUG("  prefix: %p", prefix);
    TEKLOG_DEBUG("  postfix: %p", postfix);

    // 记录函数指针的详细信息
    if (prefix) {
        TEKLOG_DEBUG("  prefix is valid function pointer at %p", prefix);
    } else {
        TEKLOG_DEBUG("  prefix is NULL");
    }

    if (postfix) {
        TEKLOG_DEBUG("  postfix is valid function pointer at %p", postfix);
     } else {
        TEKLOG_DEBUG("  postfix is NULL");
    }

    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle: %p", method);
        TEKLOG_DEBUG("=== [END] patchlib_install_prepost_hook (invalid method) ===");
        return PATCH_HOOK_INVALID_ID;
    }

    if (prefix == NULL && postfix == NULL) {
        TEKLOG_ERROR("Both prefix and postfix callbacks are NULL");
        TEKLOG_DEBUG("=== [END] patchlib_install_prepost_hook (both callbacks NULL) ===");
        return PATCH_HOOK_INVALID_ID;
    }

    const bool is_hooked = il2cpp_is_method_hooked(method);
    TEKLOG_DEBUG("Method %p is already hooked: %s", method, is_hooked ? "true" : "false");

    patch_method_signature_t* sig = NULL;
    bool newly_allocated_sig = false;

    if (!is_hooked) {
        TEKLOG_DEBUG("Getting method signature for method %p (first time hook)", method);
        sig = malloc(sizeof(patch_method_signature_t));
        if (!sig) {
            TEKLOG_ERROR("Failed to allocate memory for signature");
            TEKLOG_DEBUG("=== [END] patchlib_install_prepost_hook (malloc failed) ===");
            return PATCH_HOOK_INVALID_ID;
        }
        newly_allocated_sig = true;

        TEKLOG_DEBUG("Allocated signature memory at %p, size: %zu bytes",
                     sig, sizeof(patch_method_signature_t));

        // 初始化签名结构体
        memset(sig, 0, sizeof(patch_method_signature_t));
        TEKLOG_DEBUG("Initialized signature struct to zeros");

        TEKLOG_DEBUG("Calling patchlib_method_get_signature for method %p", method);
        if (!patchlib_method_get_signature(method, sig)) {
            TEKLOG_ERROR("Failed to get method signature for method %p", method);
            patchlib_method_signature_free(sig);
            free(sig);
            TEKLOG_DEBUG("=== [END] patchlib_install_prepost_hook (get signature failed) ===");
            return PATCH_HOOK_INVALID_ID;
        }

        TEKLOG_DEBUG("Method signature obtained successfully:");
        TEKLOG_DEBUG("  - Signature pointer: %p", sig);
        TEKLOG_DEBUG("  - Return type: %d", sig->return_type);
        TEKLOG_DEBUG("  - Argument count: %zu", tefstd_vector_size(&sig->arg_types));

        // 打印参数类型
        for (size_t i = 0; i < tefstd_vector_size(&sig->arg_types); i++) {
            patch_handle_t* type_ptr = tefstd_vector_at(&sig->arg_types, i);
            if (type_ptr) {
                TEKLOG_DEBUG("  - Arg[%zu]: type handle %p", i, *type_ptr);
            } else {
                TEKLOG_DEBUG("  - Arg[%zu]: NULL", i);
            }
        }

        TEKLOG_DEBUG("Signature will be persisted for C# layer");
    } else {
        TEKLOG_DEBUG("Method %p is already hooked, retrieving existing signature", method);
        sig = il2cpp_get_hooked_method_sig(method);
        if (sig == NULL) {
            TEKLOG_ERROR("Failed to get hooked method signature for method %p", method);
            TEKLOG_DEBUG("=== [END] patchlib_install_prepost_hook (get cached sig failed) ===");
            return PATCH_HOOK_INVALID_ID;
        }
        TEKLOG_DEBUG("Retrieved existing signature for method %p: sig=%p", method, sig);
    }

    TEKLOG_DEBUG("Preparing to call il2cpp_hook_method:");
    TEKLOG_DEBUG("  - Method handle: %p", method);
    TEKLOG_DEBUG("  - Signature: %p", sig);
    TEKLOG_DEBUG("  - Prefix: %p", prefix);
    TEKLOG_DEBUG("  - Postfix: %p", postfix);

    const patch_hook_id_t hook_id = il2cpp_hook_method(method, sig, prefix, postfix);
    TEKLOG_DEBUG("il2cpp_hook_method returned: hook_id=%d", hook_id);

    if (hook_id == PATCH_HOOK_INVALID_ID) {
        TEKLOG_ERROR("il2cpp_hook_method failed for method %p", method);
        // 如果是第一次 Hook 且失败了，需要释放新分配的签名
        if (newly_allocated_sig && sig != NULL) {
            TEKLOG_DEBUG("Freeing newly allocated signature at %p (hook failed)", sig);
            patchlib_method_signature_free(sig);
            free(sig);
        }
        TEKLOG_DEBUG("=== [END] patchlib_install_prepost_hook (hook failed) ===");
        return PATCH_HOOK_INVALID_ID;
    }

    TEKLOG_DEBUG("Hook installed successfully:");
    TEKLOG_DEBUG("  - Method: %p", method);
    TEKLOG_DEBUG("  - Hook ID: %d", hook_id);
    TEKLOG_DEBUG("  - Signature stored at: %p", sig);
    TEKLOG_DEBUG("  - Newly allocated signature: %s", newly_allocated_sig ? "yes" : "no");

    TEKLOG_DEBUG("=== [END] patchlib_install_prepost_hook (success) ===");
    return hook_id;
}

bool patchlib_uninstall_hook(const patch_hook_id_t hook_id) {
    TEKLOG_DEBUG("patchlib_uninstall_hook called: hook_id=%d", hook_id);

    if (hook_id == PATCH_HOOK_INVALID_ID) {
        TEKLOG_ERROR("Invalid hook_id");
        return false;
    }

    // 获取方法句柄
    patch_handle_t method = il2cpp_get_method_by_hook_node(hook_id);
    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Failed to get method for hook_id %d", hook_id);
        return false;
    }

    // 检查是否只有一个 Hook 节点
    const bool single_node = il2cpp_has_single_hook_node(method);
    TEKLOG_DEBUG("Method %p has single hook node: %s", method, single_node ? "true" : "false");

    // 如果是最后一个 Hook，需要释放签名
    if (single_node) {
        patch_method_signature_t* sig = il2cpp_get_hooked_method_sig(method);
        if (sig != NULL) {
            TEKLOG_DEBUG("Freeing method signature at %p for method %p", sig, method);
            patchlib_method_signature_free(sig);
            free(sig);
        }
    }

    // 卸载 Hook
    const bool result = il2cpp_unhook_method(hook_id);

    if (result) {
        TEKLOG_DEBUG("Hook uninstalled successfully: hook_id=%d", hook_id);
    } else {
        TEKLOG_ERROR("Failed to uninstall hook: hook_id=%d", hook_id);
    }

    return result;
}