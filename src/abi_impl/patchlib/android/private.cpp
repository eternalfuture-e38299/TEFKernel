/*******************************************************************************
 * tefkernel - private
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
 * Created: 2025/12/7
 *******************************************************************************/

#include "private.hpp"
#include "patchlib/field.h"
#include "patchlib/type.h"

#include <cstdlib>

#include "tef/core/log.hpp"

extern "C" {
#include "../il2cpp_api.h"
}

using tef::log::debug;
using tef::log::error;
using tef::log::warn;

namespace {
    /* Il2CppArray 头布局：m_class / m_monitor / m_bounds / m_length
     * 元素数据从 sizeof(Il2CppArray) 开始。 */
    struct il2cpp_array {
        // ReSharper disable once CppDeclaratorNeverUsed
        void *m_class;
        // ReSharper disable once CppDeclaratorNeverUsed
        void *m_monitor;
        // ReSharper disable once CppDeclaratorNeverUsed
        void *m_bounds;
        // ReSharper disable once CppDeclaratorNeverUsed
        int m_length;
    };
} // namespace

/* ============================================================================
 * Il2CppArray<System.Type> 构造（泛型反射用）
 * ============================================================================ */

void *create_type_array_from_vector(const tefstd_vector_t *type_vector,
                                    void *element_type_class) {
    if (!element_type_class) {
        error("element_type_class is nullptr");
        return nullptr;
    }
    if (!type_vector || tefstd_vector_size(type_vector) == 0) {
        warn("Invalid or empty type vector");
        return nullptr;
    }

    const size_t count = tefstd_vector_size(type_vector);
    void *array = il2cpp_array_new(element_type_class,
                                   static_cast<uintptr_t>(count));
    if (!array) {
        error("il2cpp_array_new failed");
        return nullptr;
    }

    int element_size = il2cpp_array_element_size(element_type_class);
    if (element_size <= 0)
        element_size = static_cast<int>(sizeof(void *));

    char *data_start = static_cast<char *>(array) + sizeof(il2cpp_array);

    for (size_t i = 0; i < count; ++i) {
        const auto obj = static_cast<void **>(tefstd_vector_at(type_vector, i));
        if (!obj) {
            warn("Type[{}] is NULL, skipping", i);
            continue;
        }
        *reinterpret_cast<void **>(data_start + i * element_size) = *obj;
    }

    return array;
}

/* ============================================================================
 * patch_type_t → ffi_type*
 *
 * PATCH_STRUCT 的关键：只取**实例字段**。
 *   il2cpp_class_get_fields 会同时返回 static 字段——Vector2 有 X/Y +
 *   Zero/One/UnitX/UnitY(static) 共 6 个，全塞进 ffi_type 会把 8 字节结构体
 *   当成 24 字节，libffi 走 sret 指针 ABI，而方法真实是从 s0/s1 返回 →
 *   返回值恒 (0,0)。
 * ============================================================================ */

ffi_type *patch_type_to_ffi_type(patch_type_t type, patch_handle_t struct_type) {
    switch (type) {
        case PATCH_VOID: return &ffi_type_void;
        case PATCH_BOOL: return &ffi_type_uint8;
        case PATCH_INT8: return &ffi_type_sint8;
        case PATCH_INT16: return &ffi_type_sint16;
        case PATCH_INT32: return &ffi_type_sint32;
        case PATCH_INT64: return &ffi_type_sint64;
        case PATCH_UINT8: return &ffi_type_uint8;
        case PATCH_UINT16: return &ffi_type_uint16;
        case PATCH_UINT32: return &ffi_type_uint32;
        case PATCH_UINT64: return &ffi_type_uint64;
        case PATCH_FLOAT: return &ffi_type_float;
        case PATCH_DOUBLE: return &ffi_type_double;
        case PATCH_CHAR: return &ffi_type_uint32;

        case PATCH_STRUCT: {
            tefstd_vector_t fields{};
            patchlib_type_get_fields(struct_type, false, &fields);
            const size_t n = tefstd_vector_size(&fields);

            size_t inst_count = 0;
            for (size_t i = 0; i < n; ++i) {
                const auto fh = *static_cast<patch_handle_t *>(tefstd_vector_at(&fields, i));
                if (fh && !patchlib_field_is_static(fh)) ++inst_count;
            }

            auto **elements = static_cast<ffi_type **>(
                std::malloc((inst_count + 1) * sizeof(ffi_type *)));
            if (!elements) {
                tefstd_vector_destroy(&fields);
                return &ffi_type_pointer;
            }

            size_t ei = 0;
            for (size_t i = 0; i < n; ++i) {
                const auto fh = *static_cast<patch_handle_t *>(tefstd_vector_at(&fields, i));
                if (!fh || patchlib_field_is_static(fh)) continue;

                patch_handle_t ft = il2cpp_field_get_type(fh);
                elements[ei++] = patch_type_to_ffi_type(
                    patchlib_field_get_type(fh),
                    il2cpp_class_from_type(ft));
            }
            tefstd_vector_destroy(&fields);
            elements[ei] = nullptr;

            auto *s = static_cast<ffi_type *>(std::malloc(sizeof(ffi_type)));
            if (!s) {
                std::free(elements);
                return &ffi_type_pointer;
            }

            s->size = 0;
            s->alignment = 0;
            s->type = FFI_TYPE_STRUCT;
            s->elements = elements;
            return s;
        }

        default: return &ffi_type_pointer;
    }
}

/* ============================================================================
 * 释放 patch_type_to_ffi_type 分配的 ffi_type
 * 基元 / 指针是静态常量，不能 free；只有 STRUCT 是动态分配的。
 * ============================================================================ */

void patch_ffi_type_free(ffi_type *ffi, patch_handle_t struct_type) {
    if (!ffi || ffi->type != FFI_TYPE_STRUCT) return;

    if (ffi->elements) {
        tefstd_vector_t fields{};
        patchlib_type_get_fields(struct_type, false, &fields);
        const size_t n = tefstd_vector_size(&fields);

        for (size_t i = 0; i < n; ++i) {
            ffi_type *elem = ffi->elements[i];
            if (!elem) break;

            if (elem->type == FFI_TYPE_STRUCT) {
                const auto fh = *static_cast<patch_handle_t *>(tefstd_vector_at(&fields, i));
                if (patchlib_field_get_type(fh) == PATCH_STRUCT) {
                    patch_handle_t fs = il2cpp_class_from_type(
                        il2cpp_field_get_type(fh));
                    patch_ffi_type_free(elem, fs);
                }
            }
        }
        tefstd_vector_destroy(&fields);

        std::free(ffi->elements);
        ffi->elements = nullptr;
    }

    std::free(ffi);
}

/* ============================================================================
 * 泛型反射符号
 * ============================================================================ */

patch_handle_t patchlib_make_generic_type = nullptr;
patch_handle_t patchlib_make_generic_method_impl = nullptr;
