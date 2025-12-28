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

#include "internal/log.h"
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/type.h"
#include "patchlib/struct/array.h"

void Initialize_AlmostEverything_pre(patch_handle_t orig_func, patch_handle_t instance, void** args, const patch_method_signature_t* sig_info) {
    TEKLOG_INFO("Hook triggered: Initialize_AlmostEverything_pre called with parameter");
}

void Initialize_AlmostEverything_post(patch_handle_t orig_func, patch_handle_t instance, void** args, void* result, const patch_method_signature_t* sig_info) {
    TEKLOG_INFO("Hook triggered: Initialize_AlmostEverything_post called with parameter");


    patch_handle_t sets = patchlib_type_get_inner_type(patchlib_type_get_type("Terraria.ID", "ItemID"), "Sets");
    patch_handle_t deprecated = patchlib_type_get_field(sets, "Deprecated");

    patch_handle_t deprecated_value;
    bool value = false;

    patchlib_field_get_value(deprecated, nullptr, &deprecated_value);
    patchlib_array_fill(deprecated_value, &value, PATCH_BOOL);

    TEKLOG_INFO("已解除禁装限制");

    auto* mem = static_cast<unsigned char *>(deprecated_value);
    TEKLOG_INFO("前128字节内存内容:");

    for (int i = 0; i < 128; i += 16) {
        char hex_line[256] = "";
        char ascii_line[64] = "";

        // 16进制部分
        for (int j = 0; j < 16; j++) {
            if (i + j < 128) {
                char hex[4];
                snprintf(hex, sizeof(hex), "%02X ", mem[i + j]);
                strcat(hex_line, hex);

                // ASCII部分
                char c = mem[i + j];
                strcat(ascii_line, (c >= 32 && c <= 126) ? (char[]){c, '\0'} : ".");
            } else {
                strcat(hex_line, "   ");
                strcat(ascii_line, " ");
            }
        }

        TEKLOG_INFO("%p: %s |%s|", (void*)(mem + i), hex_line, ascii_line);
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