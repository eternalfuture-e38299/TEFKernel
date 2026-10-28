/*******************************************************************************
 * tefkernel - asset
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
 * Created: 2026/7/26
 *******************************************************************************/

#include "terraria/asset.h"
#include "internal/terraria/asset.h"

#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/il2cpp_api.h"
#include "patchlib/method.h"
#include "patchlib/struct/string.h"

static patch_handle_t asset_class = PATCH_NULL;

void terraria_asset_init() {
    asset_class = patchlib_type_get_type("ReLogic.Content", "Asset`1");
}

patch_handle_t terraria_asset_get_generic_class(patch_handle_t element_type) {
    if (!patchlib_is_valid(asset_class)) {
        TEKLOG_ERROR("Asset`1 class not initialized");
        return PATCH_NULL;
    }

    if (!patchlib_is_valid(element_type)) {
        TEKLOG_ERROR("Invalid element type");
        return PATCH_NULL;
    }

    // 准备泛型参数
    tefstd_vector_t generic_types;
    tefstd_vector_init(&generic_types, sizeof(patch_handle_t));

#if defined(__ANDROID__)
    // Android 上需要获取 mono 类型
    patch_handle_t obj_type = patchlib_type_get_mono_type(patchlib_get_basic_type(PATCH_OBJECT));
    if (!patchlib_is_valid(obj_type)) {
        TEKLOG_ERROR("Failed to get mono type for element type");
        tefstd_vector_destroy(&generic_types);
        return PATCH_NULL;
    }
    tefstd_vector_push_back(&generic_types, &obj_type);
#else
    tefstd_vector_push_back(&generic_types, &element_type);
#endif

    // 创建泛型类型 Asset`1[element_type]
    patch_handle_t generic_class = patchlib_type_make_generic_type(asset_class, &generic_types);

    tefstd_vector_destroy(&generic_types);

    if (!patchlib_is_valid(generic_class)) {
        TEKLOG_ERROR("Failed to create generic class Asset`1[%p]", element_type);
        return PATCH_NULL;
    }

    TEKLOG_DEBUG("Created generic class Asset`1[%p]: %p", element_type, generic_class);
    return generic_class;
}


patch_handle_t terraria_asset_create(patch_handle_t type, patch_handle_t value) {
    patch_handle_t asset_generic_class = terraria_asset_get_generic_class(type);

    patch_handle_t asset_ctor = patchlib_type_get_method_by_param_count(asset_generic_class, ".ctor", 1);
    patch_handle_t asset_value = patchlib_type_get_field(asset_generic_class, "<Value>k__BackingField");
    if (!asset_value) {
        asset_value = patchlib_type_get_field(asset_generic_class, "Value");
    }

    patch_handle_t null_str = patchlib_string_create("");
    patch_handle_t asset = PATCH_NULL;
    // void set_State(AssetState value)
    patch_handle_t set_state = patchlib_type_get_method_by_param_count(asset_generic_class, "set_State", 1);

#if !defined(__ANDROID__)
    void *args[1] = { &null_str };
    patchlib_constructor_invoke(asset_ctor, &asset, args);
#else
    asset = patchlib_type_new_instance(asset_generic_class);
#endif

    if (patchlib_is_valid(value)) patchlib_field_set_value(asset_value, asset, &value);

    int asset_state = 2;
    void* iargs[1] = { &asset_state };
    patchlib_method_invoke_args(set_state, asset, NULL, iargs);

    patchlib_free(asset_generic_class);
    patchlib_free(asset_ctor);
    patchlib_free(asset_value);
    patchlib_free(null_str);

    return asset;
}

patch_handle_t terraria_asset_get_value(patch_handle_t asset) {
    patch_handle_t asset_generic_class = il2cpp_object_get_class(asset);
    patch_handle_t asset_value = patchlib_type_get_field(asset_generic_class, "<Value>k__BackingField");

    patch_handle_t result = PATCH_NULL;

    patchlib_field_get_value(asset_value, asset, &result);

    patchlib_free(asset_generic_class);
    patchlib_free(asset_value);

    return result;
}