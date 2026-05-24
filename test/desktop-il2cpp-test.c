/*******************************************************************************
 * tefkernel - desktop
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
 * Created: 2026/5/17
 *******************************************************************************/


#include <stdio.h>
#define DEFINE_IL2CPP_API(ret, name, ...) \
ret (*name)(__VA_ARGS__) = NULL;
#include <stddef.h>

#define il2cpp_assembly_get_image(ptr) ptr

/**
 * @brief 获取当前应用域
 * @return 应用域指针(原始类型: Il2CppDomain*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_domain_get)

/**
 * @brief 获取域中所有程序集
 * @param domain 应用域指针(原始类型: const Il2CppDomain*)
 * @param size [out] 程序集数量
 * @return 程序集指针数组(原始类型: const Il2CppAssembly**)
 */
DEFINE_IL2CPP_API(void**, il2cpp_domain_get_assemblies,
                  const void* domain, size_t* size)


/**
 * @brief 通过名称获取类
 * @param image 程序集镜像(原始类型: const Il2CppImage*)
 * @param namespaze 命名空间
 * @param name 类名
 * @return 类指针(原始类型: Il2CppClass*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_from_name,
                  const void* image, const char* namespaze, const char *name)

/**
 * @brief 获取嵌套类型(迭代器方式)
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @param iter 迭代器指针(必须初始化为nullptr)
 * @return 嵌套类指针(原始类型: Il2CppClass*)
 */
DEFINE_IL2CPP_API(void*, il2cpp_class_get_nested_types,
                  void* klass, void** iter)

/**
 * @brief 获取类名
 * @param klass 类指针(原始类型: Il2CppClass*)
 * @return 类名字符串
 */
DEFINE_IL2CPP_API(const char*, il2cpp_class_get_name, void* klass)


void* patchlib_type_get_type(const char *ns, const char *name) {
    if (!il2cpp_domain_get) {
        printf("IL2CPP functions not initialized!\n");
        return NULL;
    }

    const void *domain = il2cpp_domain_get();
    if (!domain) {
        printf("Failed to get il2cpp domain\n");
        return NULL;
    }

    size_t assemblies_count = 0;
    void **assemblies = il2cpp_domain_get_assemblies((void*)domain, &assemblies_count);

    if (!assemblies || assemblies_count == 0) {
        printf("No assemblies found\n");
        return NULL;
    }

    printf("Found %zu assemblies\n", assemblies_count);

    for (size_t i = 0; i < assemblies_count; ++i) {
        const void *assembly = assemblies[i];
        const void *image = il2cpp_assembly_get_image((void*)assembly);

        if (!image) {
            continue;
        }

        void *found_class = il2cpp_class_from_name((void*)image, ns, name);
        if (found_class) {
            printf("Found class: %s.%s in assembly %zu\n", ns, name, i);
            return found_class;
        }
    }

    printf("Class %s.%s not found\n", ns, name);
    return NULL;
}

void test() {
void* Test =     patchlib_type_get_type("tefloader", "Test");
const char* n = il2cpp_class_get_name(Test);
printf("name: %s", n);
}
