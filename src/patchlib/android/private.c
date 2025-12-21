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

#include "private.h"

#include "il2cpp_api.h"

void *create_type_array_from_vector(const tef_vector_t *type_vector) {
    if (!type_vector || tefstd_vector_size(type_vector) == 0) {
        return NULL;
    }

    const void *corlib = il2cpp_get_corlib();
    void *typeClass = il2cpp_class_from_name(corlib, "System", "Type");
    if (!typeClass) {
        return NULL;
    }

    const size_t count = tefstd_vector_size(type_vector);
    void *array = il2cpp_array_new(typeClass, count);
    if (!array) {
        return NULL;
    }

    // 获取数组类型的 TypeInfo 并计算元素大小
    const int elementSize = il2cpp_array_element_size(typeClass); // 应为 sizeof(void*)

    // 计算元素数据起始地址
    // Il2CppArray 布局：[Il2CppObject][Il2CppArrayBounds][uint32_t max_length][elements...]
    char *dataStart = (char *) array + sizeof(void *) * 2 + sizeof(void *) + sizeof(uint32_t);

    // 逐个复制指针
    for (size_t i = 0; i < count; ++i) {
        void *obj = *(void **) tefstd_vector_at(type_vector, i);
        if (obj) {
            void **elemPtr = (void **) (dataStart + i * elementSize);
            *elemPtr = obj;
        }
    }

    return array;
}

patch_handle_t patchlib_MakeGenericType = NULL;
patch_handle_t patchlib_MakeGenericMethod_impl = NULL;