/*******************************************************************************
 * tefkernel - il2cpp_api
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

#define IL2CPP_API_IMPL 1

#include "../il2cpp_api.h"

#include "xdl.h"

#define LOAD_SYMBOL(name) \
name = xdl_sym(handle, #name, NULL)

void il2cpp_api_init(void *handle) {
    // 加载基础API
    LOAD_SYMBOL(il2cpp_init);
    LOAD_SYMBOL(il2cpp_domain_get);
    LOAD_SYMBOL(il2cpp_domain_get_assemblies);
    LOAD_SYMBOL(il2cpp_assembly_get_image);
    LOAD_SYMBOL(il2cpp_get_corlib);

    // 加载类相关API
    LOAD_SYMBOL(il2cpp_class_from_name);
    LOAD_SYMBOL(il2cpp_class_get_nested_types);
    LOAD_SYMBOL(il2cpp_class_get_name);
    LOAD_SYMBOL(il2cpp_class_get_parent);
    LOAD_SYMBOL(il2cpp_class_get_methods);
    LOAD_SYMBOL(il2cpp_class_get_fields);
    LOAD_SYMBOL(il2cpp_class_get_properties);
    LOAD_SYMBOL(il2cpp_class_get_static_field_data);
    LOAD_SYMBOL(il2cpp_class_get_namespace);
    LOAD_SYMBOL(il2cpp_class_get_type);
    LOAD_SYMBOL(il2cpp_class_is_abstract);
    LOAD_SYMBOL(il2cpp_class_is_interface);
    LOAD_SYMBOL(il2cpp_class_is_enum);
    LOAD_SYMBOL(il2cpp_class_is_generic);
    LOAD_SYMBOL(il2cpp_class_from_system_type);
    LOAD_SYMBOL(il2cpp_class_get_field_from_name);
    LOAD_SYMBOL(il2cpp_class_get_property_from_name);
    LOAD_SYMBOL(il2cpp_class_get_method_from_name);
    LOAD_SYMBOL(il2cpp_class_from_il2cpp_type);

    // 加载对象相关API
    LOAD_SYMBOL(il2cpp_object_new);

    // 加载字段相关API
    LOAD_SYMBOL(il2cpp_field_get_name);
    LOAD_SYMBOL(il2cpp_field_get_parent);
    LOAD_SYMBOL(il2cpp_field_get_offset);
    LOAD_SYMBOL(il2cpp_field_get_type);
    LOAD_SYMBOL(il2cpp_field_static_get_value);

    // 加载属性相关API
    LOAD_SYMBOL(il2cpp_property_get_name);
    LOAD_SYMBOL(il2cpp_property_get_get_method);
    LOAD_SYMBOL(il2cpp_property_get_set_method);

    // 加载方法相关API
    LOAD_SYMBOL(il2cpp_method_get_name);
    LOAD_SYMBOL(il2cpp_method_get_param_count);
    LOAD_SYMBOL(il2cpp_method_get_param_name);
    LOAD_SYMBOL(il2cpp_method_get_param);
    LOAD_SYMBOL(il2cpp_method_is_instance);
    LOAD_SYMBOL(il2cpp_method_is_generic);
    LOAD_SYMBOL(il2cpp_method_get_return_type);
    LOAD_SYMBOL(il2cpp_method_get_declaring_type);
    LOAD_SYMBOL(il2cpp_method_get_object);
    LOAD_SYMBOL(il2cpp_method_get_from_reflection);
    LOAD_SYMBOL(il2cpp_method_get_class);
    LOAD_SYMBOL(il2cpp_method_get_token);

    // 加载类型相关API
    LOAD_SYMBOL(il2cpp_type_get_type);
    LOAD_SYMBOL(il2cpp_class_from_type);
    LOAD_SYMBOL(il2cpp_type_get_object);

    // 加载数组相关API
    LOAD_SYMBOL(il2cpp_array_new);
    LOAD_SYMBOL(il2cpp_array_element_size);
    LOAD_SYMBOL(il2cpp_array_get_byte_length);
    LOAD_SYMBOL(il2cpp_array_length);

    // 加载字符串相关API
    LOAD_SYMBOL(il2cpp_string_length);
    LOAD_SYMBOL(il2cpp_string_chars);
    LOAD_SYMBOL(il2cpp_string_new);

    //加载runtime相关API
    LOAD_SYMBOL(il2cpp_runtime_invoke);
    LOAD_SYMBOL(il2cpp_object_unbox);
}

void * il2cpp_object_get_class(void *object) {
    return *(void**)object;
}
