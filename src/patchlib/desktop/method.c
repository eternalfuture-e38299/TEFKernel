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
#include <string.h>

#include "net_api.h"
#include "internal/log.h"

const char *patchlib_method_get_name(patch_handle_t method) {
    if (!patchlib_is_valid(method)) {
        TEKLOG_WARN("Invalid method handle");
        return NULL;
    }

    const char* result = net_method_get_name(method);
    TEKLOG_DEBUG("Method name: %s", result ? result : "NULL");
    return result;
}

int patchlib_method_get_param_count(patch_handle_t method) {
    if (!patchlib_is_valid(method)) {
        TEKLOG_WARN("Invalid method handle");
        return -1;
    }

    const int result = (int) net_method_get_param_count(method);
    TEKLOG_DEBUG("Method parameter count: %d", result);
    return result;
}

bool patchlib_method_is_instance(patch_handle_t method) {
    if (!patchlib_is_valid(method)) {
        TEKLOG_WARN("Invalid method handle");
        return false;
    }

    const bool result = net_method_is_instance(method);
    TEKLOG_DEBUG("Method is instance: %s", result ? "true" : "false");
    return result;
}

patch_handle_t patchlib_method_make_generic_instance(patch_handle_t method, const tef_vector_t *template_types) {
    TEKLOG_DEBUG("patchlib_method_make_generic_instance called: method=%d, template_types_count=%zu",
                 method, template_types ? tefstd_vector_size(template_types) : 0);

    if (!patchlib_is_valid(method) || !template_types) {
        TEKLOG_ERROR("Invalid method handle");
        return PATCH_NULL;
    }

    const patch_handle_t result = net_method_make_generic_method(method, template_types->data, (int)template_types->size);
    TEKLOG_DEBUG("Generic method created: %d", result);
    return result;
}

bool patchlib_method_get_signature(const patch_handle_t method, patch_method_signature_t* signature) {
    TEKLOG_DEBUG("patchlib_method_get_signature called: method=%d", method);

    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle");
        return false;
    }

    signature->method = method;
    signature->return_type = net_type_to_patchlib_type(net_method_get_return_type(method));
    signature->is_instance = patchlib_method_is_instance(method);
    tefstd_vector_init(&signature->arg_types, sizeof(patch_type_t));
    tefstd_vector_init(&signature->arg_names, sizeof(char*)); // 存储char*指针

    const int param_count = patchlib_method_get_param_count(method);
    TEKLOG_DEBUG("Method signature: return_type=%d, is_instance=%s, param_count=%d",
                 signature->return_type, signature->is_instance ? "true" : "false", param_count);

    for (int i = 0; i < param_count; ++i) {
        patch_type_t t = net_type_to_patchlib_type(net_method_get_param(method, i));
        const char* original_name = net_method_get_param_name(method, i);

        // 复制字符串
        char* name_copy = NULL;
        if (original_name != NULL) {
            const size_t name_len = strlen(original_name) + 1; // 包含结束符
            name_copy = (char*)malloc(name_len);
            if (name_copy != NULL) {
                strcpy(name_copy, original_name);
            } else {
                TEKLOG_WARN("Failed to allocate memory for parameter name");
                name_copy = (char*)malloc(1);
                if (name_copy) name_copy[0] = '\0';
            }
        } else {
            // 如果名称为空，分配一个空字符串
            name_copy = (char*)malloc(1);
            if (name_copy) name_copy[0] = '\0';
        }

        tefstd_vector_push_back(&signature->arg_types, &t);
        tefstd_vector_push_back(&signature->arg_names, &name_copy);
        TEKLOG_DEBUG("Parameter %d type: %d, name: %s", i, t, name_copy);
    }

    TEKLOG_DEBUG("Signature extraction completed: total_params=%zu",
                 tefstd_vector_size(&signature->arg_types));
    return true;
}

bool patchlib_method_signature_free(patch_method_signature_t* signature) {
    if (signature == NULL) return false;

    for (size_t i = 0; i < tefstd_vector_size(&signature->arg_names); i++) {
        char** name_ptr = tefstd_vector_at(&signature->arg_names, i);
        if (name_ptr != NULL && *name_ptr != NULL) {
            free(*name_ptr);
        }
    }

    tefstd_vector_destroy(&signature->arg_types);
    tefstd_vector_destroy(&signature->arg_names);
    patchlib_method_free(signature->method);
    signature->method = PATCH_NULL;
    signature->is_instance = 0;
    signature->return_type = PATCH_VOID;

    return true;
}

bool patchlib_method_invoke_args(const patch_handle_t method, const patch_handle_t instance,
                                void *return_value, void** args) {
    TEKLOG_DEBUG("patchlib_method_invoke_args called: method=%d, instance=%d, return_value=%p",
                 method, instance, return_value);

    if (!patchlib_is_valid(method)) {
        TEKLOG_ERROR("Invalid method handle");
        return false;
    }

    // 获取方法签名
    patch_method_signature_t signature;
    if (!patchlib_method_get_signature(method, &signature)) {
        TEKLOG_ERROR("Failed to get method signature");
        return false;
    }

    const bool r = net_method_invoke(method, instance, (int)tefstd_vector_size(&signature.arg_types), return_value, args, (patch_type_t*)signature.arg_types.data);

    patchlib_method_signature_free(&signature);
    TEKLOG_DEBUG("Method invocation completed successfully");

    return r;
}

patch_hook_id_t patchlib_install_prepost_hook(const patch_handle_t method, const prefix_callback_t prefix, const postfix_callback_t postfix) {
    const bool is_hooked = net_is_method_hooked(method);
    patch_method_signature_t* sig = NULL;
    if (is_hooked) {
        sig = malloc(sizeof(patch_method_signature_t));
        patchlib_method_get_signature(method, sig);
    }

    return net_hook_method(method, sig, prefix, postfix);
}

bool patchlib_uninstall_hook(const patch_hook_id_t hook_id) {
    const patch_handle_t method = net_get_method_by_hook_node(hook_id);
    const bool single_node = net_has_single_hook_node(method);

    if (single_node) {
        patch_method_signature_t* sig = net_get_hooked_method_sig(method);
        patchlib_method_signature_free(sig);
        free(sig);
    }

    return net_unhook_method(hook_id);
}

bool patchlib_method_free(const patch_handle_t method) {
    TEKLOG_DEBUG("Method freed: %d", method);
    net_method_free(method);
    return true;
}