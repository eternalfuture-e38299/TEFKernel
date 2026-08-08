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

#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/struct/string.h"

static patch_handle_t asset_class = PATCH_NULL;

void terraria_asset_init() {
    asset_class = patchlib_type_get_type("ReLogic.Content", "Asset`1");

}

patch_handle_t terraria_asset_create(patch_handle_t type, patch_handle_t value) {
    tefstd_vector_t generic_types;
    tefstd_vector_init(&generic_types, sizeof(patch_handle_t));
#if defined(__ANDROID__)
    patch_handle_t obj_type = patchlib_type_get_mono_type(patchlib_get_basic_type(PATCH_OBJECT));
    tefstd_vector_push_back(&generic_types, &obj_type);
#else
    tefstd_vector_push_back(&generic_types, &type);
#endif

    patch_handle_t asset_generic_class = patchlib_type_make_generic_type(asset_class, &generic_types);
    patch_handle_t asset_ctor = patchlib_type_get_method_by_param_count(asset_generic_class, ".ctor", 1);
    patch_handle_t asset_value = patchlib_type_get_field(asset_generic_class, "<Value>k__BackingField");
    patch_handle_t null_str = patchlib_string_create("");
    patch_handle_t asset = PATCH_NULL;

    void *args[1] = { &null_str };
    patchlib_constructor_invoke(asset_ctor, &asset, args);

    if (patchlib_is_valid(value)) patchlib_field_set_value(asset_value, asset, &value);

    tefstd_vector_destroy(&generic_types);
    patchlib_free(asset_generic_class);
    patchlib_free(asset_ctor);
    patchlib_free(asset_value);
    patchlib_free(null_str);

    return asset;
}