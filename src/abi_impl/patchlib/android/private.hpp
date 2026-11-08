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

#ifndef TEFKERNEL_PRIVATE_H
#define TEFKERNEL_PRIVATE_H

#include <ffi.h>

#include "patchlib/type.h"
#include "tefstd/vector.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 从 vector_t（元素为 void*）创建 Il2CppArray<System.Type>
 *
 * @param type_vector 指向已初始化的 vector_t，其中每个元素是一个 Il2CppObject*（即 Type 对象）
 * @param element_type_class Il2CppClass 类
 * @return 成功返回 Il2CppArray*；失败或空输入返回 NULL
 *
 * @note 要求 vector_t 的 elem_size == sizeof(void*)
 */
void *create_type_array_from_vector(const tefstd_vector_t *type_vector, void *element_type_class);

/**
 * @brief 转换为ffi type
 * @param type patch类型
 * @return ffi_type
 */
ffi_type *patch_type_to_ffi_type(patch_type_t type, patch_handle_t struct_type);

/**
 * @brief 释放由 patch_type_to_ffi_type 为结构体动态分配的 ffi_type
 * @param ffi  要释放的 ffi_type 指针（可为 NULL）
 * @param struct_type 对应的 patch 结构体句柄，用于确定字段数量
 */
void patch_ffi_type_free(ffi_type *ffi, patch_handle_t struct_type);

extern patch_handle_t patchlib_make_generic_type;
extern patch_handle_t patchlib_make_generic_method_impl;

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_PRIVATE_H
