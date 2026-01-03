/*******************************************************************************
 * tefkernel - android_test
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
 * Created: 2025/12/28
 *******************************************************************************/

#include <cstdio>
#include <cstring>
#include <ctime>

#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/type.h"
#include "patchlib/android/il2cpp_api.h"
#include "patchlib/struct/array.h"

void Initialize_AlmostEverything_pre(patch_handle_t orig_func, patch_handle_t instance, void** args, const patch_method_signature_t* sig_info) {
    TEKLOG_INFO("Hook triggered: Initialize_AlmostEverything_pre called with parameter");
}

void Initialize_AlmostEverything_post(patch_handle_t orig_func, patch_handle_t instance, void** args, void* result, const patch_method_signature_t* sig_info) {
    TEKLOG_INFO("Hook triggered: Initialize_AlmostEverything_post called with parameter");

    patch_handle_t unity_texture2d = patchlib_type_get_type("UnityEngine", "Texture2D");
    patch_handle_t GetRawTextureData_method = patchlib_type_get_method_by_param_count(unity_texture2d, "GetRawTextureData`1", 0);
    patch_handle_t ctor = patchlib_type_get_method_by_param_count(unity_texture2d, ".ctor", 2);

    (void*(*)(void*, int, int))patchlib_method_get_pointer(ctor);

    tef_vector_t types;
    patch_handle_t ushort_type = patchlib_get_basic_type(PATCH_UINT16);
    tefstd_vector_init(&types, sizeof(patch_handle_t));
    tefstd_vector_push_back(&types, &ushort_type);
    patch_handle_t GetRawTextureData = patchlib_method_make_generic_instance(GetRawTextureData_method, &types);

    patch_handle_t NativeArray = ((void*(*)(void*))GetRawTextureData)();
    patch_handle_t buffer = patchlib_type_get_field(il2cpp_object_get_class(NativeArray), "m_Buffer");
    patch_handle_t length = patchlib_type_get_field(il2cpp_object_get_class(NativeArray), "m_Length");

    int size = 0;
    void* c_buffer;
    patchlib_field_get_value(length, NativeArray, &size);
    patchlib_field_get_value(buffer, NativeArray, &c_buffer);

    // 简单导出到文件
    if (c_buffer && size > 0) {
        char filename[64];
        snprintf(filename, sizeof(filename), "texture_%ld.bin", time(NULL));

        FILE* file = fopen(filename, "wb");
        if (file) {
            fwrite(c_buffer, 1, size, file);
            fclose(file);
            TEKLOG_INFO("Exported %d bytes to %s", size, filename);
        }
    }
}


void start_test() {
    patch_handle_t method = patchlib_type_get_method_by_param_count(
        patchlib_type_get_type("Terraria", "Main"),
        "Initialize_AlmostEverything",
        0
    );

    patchlib_install_prepost_hook(method, (void*)Initialize_AlmostEverything_pre, (void*)Initialize_AlmostEverything_post);
}