/*******************************************************************************
 * tefkernel - type
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
 * Created: 2026/1/3
 *******************************************************************************/

#include "patchlib/type.h"

#include <functional>
#include <stdlib.h>
#include <string.h>
#include <vector>


#include "tef/core/log.hpp"
#include "../il2cpp_api.h"

/*
 * 桌面端 il2cpp_* API 由 C# 加载器实现，返回的数组（assemblies / members）
 * 来自 Marshal.AllocHGlobal。Windows 上 .NET Framework 的 AllocHGlobal 使用
 * LocalAlloc，而 MinGW 内核里的 free() 使用另一套 CRT 堆，直接 free 会破坏堆
 * （0xC0000374 / invalid address specified to RtlFreeHeap）。
 * 因此 Windows 必须用 LocalFree 释放这类内存；Unix 上 AllocHGlobal 就是 malloc。
 */
#if defined(_WIN32)
#include <windows.h>
#define TEF_FREE_NATIVE(ptr) LocalFree((HLOCAL)(ptr))
#else
#define TEF_FREE_NATIVE(ptr) free(ptr)
#endif

/* ============================================================================
 * 内部辅助（C++ 实现，不导出）
 * ============================================================================ */

#if defined(_WIN32)
#include <windows.h>
#endif

using tef::log::debug;
using tef::log::error;
using tef::log::info;
using tef::log::trace;
using tef::log::warn;

/* ============================================================================
 * 平台：native 数组释放
 *
 * Android: il2cpp 用迭代器返回，无数组可释放 → 宏 no-op
 * Windows: C# 加载器用 Marshal.AllocHGlobal → LocalAlloc 堆 → LocalFree
 * Unix:    AllocHGlobal == malloc → free
 * ============================================================================ */

#if defined(__ANDROID__)
#   define FreeNative(p) (void(0))
#elif defined(_WIN32)
constexpr void FreeNative(void *p) {
    if (p) LocalFree(static_cast<HLOCAL>(p));
}
#else
constexpr void FreeNative(void *p) {
    std::free(p);
}
#endif

/* ============================================================================
 * 匿名 namespace：内部辅助
 * ============================================================================ */

namespace {
#if defined(__ANDROID__)
    using MemberIterator = std::function<void*(patch_handle_t, void **)>;
#else
    using MemberIterator = std::function<void**(patch_handle_t, int *)>;
#endif

    std::vector<patch_handle_t> CollectFromHierarchy(
        patch_handle_t start, const bool including_parent,
        const MemberIterator &iter_fn) {
        std::vector<patch_handle_t> out;
        patch_handle_t cur = start;

        do {
#if defined(__ANDROID__)
            void *it = nullptr;
            while (void *item = iter_fn(cur, &it))
                out.push_back(item);
#else
            int n = 0;
            if (void **items = iter_fn(cur, &n)) {
                for (int i = 0; i < n; ++i)
                    if (items[i]) out.push_back(items[i]);
                FreeNative(items); /* Android 上宏展开为 (void)0 */
            }
#endif
            if (!including_parent) break;
            cur = il2cpp_class_get_parent(cur);
        } while (cur);

        return out;
    }

#if defined(__ANDROID__)
    const MemberIterator &NestedTypesIter() {
        static const MemberIterator fn = [](patch_handle_t t, void **it) {
            return il2cpp_class_get_nested_types(t, it);
        };
        return fn;
    }
    const MemberIterator &MethodsIter() {
        static const MemberIterator fn = [](patch_handle_t t, void **it) {
            return il2cpp_class_get_methods(t, it);
        };
        return fn;
    }
    const MemberIterator &FieldsIter() {
        static const MemberIterator fn = [](patch_handle_t t, void **it) {
            return il2cpp_class_get_fields(t, it);
        };
        return fn;
    }
    const MemberIterator &PropertiesIter() {
        static const MemberIterator fn = [](patch_handle_t t, void **it) {
            return il2cpp_class_get_properties(t, it);
        };
        return fn;
    }
#else
    const MemberIterator &NestedTypesIter() {
        static const MemberIterator fn = [](patch_handle_t t, int *n) {
            return il2cpp_class_get_nested_types(t, n);
        };
        return fn;
    }

    const MemberIterator &MethodsIter() {
        static const MemberIterator fn = [](patch_handle_t t, int *n) {
            return il2cpp_class_get_methods(t, n);
        };
        return fn;
    }

    const MemberIterator &FieldsIter() {
        static const MemberIterator fn = [](patch_handle_t t, int *n) {
            return il2cpp_class_get_fields(t, n);
        };
        return fn;
    }

    const MemberIterator &PropertiesIter() {
        static const MemberIterator fn = [](patch_handle_t t, int *n) {
            return il2cpp_class_get_properties(t, n);
        };
        return fn;
    }
#endif

    bool MatchArgs(patch_handle_t method, int argc,
                   const patch_handle_t *types, const char **names) {
        // argc < 0 表示调用方不关心参数个数
        if (argc >= 0) {
            const int actual = static_cast<int>(il2cpp_method_get_param_count(method));
            if (actual != argc) {
                debug("Parameter count mismatch: expected {} got {}", argc, actual);
                return false;
            }
        }
        // types/names 仍需按各自非空判断
        if (types || names) {
            const int actual = static_cast<int>(il2cpp_method_get_param_count(method));
            for (int i = 0; i < actual; ++i) {
                if (types) {
                    void *pt = il2cpp_method_get_param(method, static_cast<uint32_t>(i));
                    if (!pt) return false;
                    patch_handle_t klass = il2cpp_class_from_type(pt);
                    if (types[i] != klass) { debug("Parameter {} type mismatch", i); return false; }
                }
                if (names && names[i]) {
                    const char *n = il2cpp_method_get_param_name(method, static_cast<uint32_t>(i));
                    if (!n || std::strcmp(n, names[i]) != 0) {
                        debug("Parameter {} name mismatch", i);
                        return false;
                    }
                }
            }
        }
        return true;
    }

    std::vector<patch_handle_t> CollectMethods(patch_handle_t type) {
        std::vector<patch_handle_t> out;
        tefstd_vector_t tmp{};
        patchlib_type_get_methods(type, false, &tmp);
        out.reserve(tefstd_vector_size(&tmp));
        for (size_t i = 0; i < tefstd_vector_size(&tmp); ++i)
            out.push_back(*static_cast<patch_handle_t *>(tefstd_vector_at(&tmp, i)));
        tefstd_vector_destroy(&tmp);
        return out;
    }

    patch_handle_t FindMethod(patch_handle_t type, const char *name, int argc,
                              const patch_handle_t *types, const char **names) {
        if (!type || !name) return nullptr;
        auto methods = CollectMethods(type);
        debug("Searching in {} methods", methods.size());
        for (auto m: methods) {
            const char *mn = il2cpp_method_get_name(m);
            if (!mn || std::strcmp(mn, name) != 0) continue;
            if (MatchArgs(m, argc, types, names)) {
                info("Found method '{}' at {}", name, fmt::ptr(m));
                return m;
            }
        }
        warn("No matching method: {} with {} params", name, argc);
        return nullptr;
    }

    void OutputHandles(std::vector<patch_handle_t> &&handles, tefstd_vector_t *out) {
        tefstd_vector_init(out, sizeof(patch_handle_t));
        for (auto h: handles)
            tefstd_vector_push_back(out, &h);
    }
} // namespace

/* ============================================================================
 * 导出函数
 * ============================================================================ */

extern "C" bool patchlib_is_valid(patch_handle_t h) {
    if (!h) {
        trace("patch_handle is null");
        return false;
    }
    return true;
}

extern "C" patch_handle_t patchlib_type_get_type(const char *ns, const char *name) {
    debug("patchlib_type_get_type: ns='{}' name='{}'",
          ns ? ns : "NULL", name ? name : "NULL");
    if (!ns || !name) return nullptr;

    void *domain = il2cpp_domain_get();
    if (!domain) {
        error("Failed to get il2cpp domain");
        return nullptr;
    }

    size_t count = 0;
    void **asms = il2cpp_domain_get_assemblies(domain, &count);
    il2cpp_free(domain);

    if (!asms || count == 0) {
        error("No assemblies found");
        return nullptr;
    }

    debug("Searching in {} assemblies", count);
    patch_handle_t found = nullptr;

    for (size_t i = 0; i < count && !found; ++i) {
        const void *img = il2cpp_assembly_get_image(asms[i]);
        if (!img) continue;
        found = il2cpp_class_from_name(img, ns, name);
        il2cpp_free(const_cast<void *>(img));
    }

    FreeNative(asms); /* Android 上 (void)0 */

    if (found) debug("Found type {}.{} at {}", ns, name, fmt::ptr(found));
    else warn("Type not found: {}.{}", ns, name);
    return found;
}

extern "C" patch_handle_t patchlib_get_basic_type(patch_type_t basic_type) {
    const char *name;
    switch (basic_type) {
        case PATCH_VOID: name = "Void";
            break;
        case PATCH_INT8: name = "SByte";
            break;
        case PATCH_INT16: name = "Int16";
            break;
        case PATCH_INT32: name = "Int32";
            break;
        case PATCH_INT64: name = "Int64";
            break;
        case PATCH_UINT8: name = "Byte";
            break;
        case PATCH_UINT16: name = "UInt16";
            break;
        case PATCH_UINT32: name = "UInt32";
            break;
        case PATCH_UINT64: name = "UInt64";
            break;
        case PATCH_BOOL: name = "Boolean";
            break;
        case PATCH_FLOAT: name = "Single";
            break;
        case PATCH_DOUBLE: name = "Double";
            break;
        case PATCH_POINTER: name = "IntPtr";
            break;
        case PATCH_OBJECT: name = "Object";
            break;
        case PATCH_CHAR: name = "Char";
            break;
        default: return nullptr;
    }
    return patchlib_type_get_type("System", name);
}

extern "C" patch_handle_t patchlib_type_new_instance(patch_handle_t type) {
    if (!patchlib_is_valid(type)) {
        error("Invalid type handle");
        return nullptr;
    }
    patch_handle_t result = il2cpp_object_new(type);
    if (!result) error("Failed to create instance for {}", fmt::ptr(type));
    return result;
}

extern "C" size_t get_size_from_patch_type(patch_type_t type) {
#if defined(__ANDROID__)
    constexpr size_t kObj = sizeof(void *);
#else
    constexpr size_t kObj = sizeof(int32_t);
#endif
    switch (type) {
        case PATCH_UINT8: return sizeof(uint8_t);
        case PATCH_UINT16: return sizeof(uint16_t);
        case PATCH_UINT32: return sizeof(uint32_t);
        case PATCH_UINT64: return sizeof(uint64_t);
        case PATCH_INT8: return sizeof(int8_t);
        case PATCH_INT16: return sizeof(int16_t);
        case PATCH_INT32: return sizeof(int32_t);
        case PATCH_INT64: return sizeof(int64_t);
        case PATCH_FLOAT: return sizeof(float);
        case PATCH_DOUBLE: return sizeof(double);
        case PATCH_BOOL: return sizeof(bool);
        case PATCH_POINTER:
        case PATCH_OBJECT: return kObj;
        case PATCH_CHAR: return sizeof(char);
        default: return 0;
    }
}

extern "C" const char *patchlib_type_get_name(patch_handle_t type) {
    if (!patchlib_is_valid(type)) return nullptr;
    return il2cpp_class_get_name(type);
}

extern "C" const char *patchlib_type_get_namespace(patch_handle_t type) {
    if (!patchlib_is_valid(type)) return nullptr;
    return il2cpp_class_get_namespace(type);
}

extern "C" char *patchlib_type_get_full_name(patch_handle_t type) {
    if (!patchlib_is_valid(type)) return nullptr;

    const char *ns = il2cpp_class_get_namespace(type);
    const char *name = il2cpp_class_get_name(type);
    if (!name) name = "";
    if (!ns) ns = "";

    std::string full;
    if (*ns) {
        full = ns;
        full += '.';
    }
    full += name;

    char *out = static_cast<char *>(std::malloc(full.size() + 1));
    if (!out) {
        error("malloc failed for full name");
        return nullptr;
    }
    std::memcpy(out, full.c_str(), full.size() + 1);
    return out;
}

extern "C" patch_handle_t patchlib_type_get_parent(patch_handle_t type) {
    if (!patchlib_is_valid(type)) return nullptr;
    return il2cpp_class_get_parent(type);
}

extern "C" patch_handle_t patchlib_type_get_inner_type(patch_handle_t parent,
                                                       const char *name) {
    if (!patchlib_is_valid(parent) || !name) return nullptr;
    auto types = CollectFromHierarchy(parent, false, NestedTypesIter());
    for (auto t: types) {
        const char *n = il2cpp_class_get_name(t);
        if (n && std::strcmp(n, name) == 0) return t;
    }
    warn("Inner type not found: {}", name);
    return nullptr;
}

extern "C" patch_handle_t patchlib_type_get_field(patch_handle_t type,
                                                  const char *name) {
    if (!patchlib_is_valid(type) || !name) return nullptr;
    return il2cpp_class_get_field_from_name(type, name);
}

extern "C" patch_handle_t patchlib_type_get_property(patch_handle_t type,
                                                     const char *name) {
    if (!patchlib_is_valid(type) || !name) return nullptr;
    return il2cpp_class_get_property_from_name(type, name);
}

extern "C" patch_handle_t patchlib_type_get_method(patch_handle_t type,
                                                   const char *name) {
    return FindMethod(type, name, -1, nullptr, nullptr);
}

extern "C" patch_handle_t patchlib_type_get_method_by_param_count(
    patch_handle_t type, const char *name, int argc) {
    if (!patchlib_is_valid(type) || !name) return nullptr;
    return il2cpp_class_get_method_from_name(type, name, argc);
}

extern "C" patch_handle_t patchlib_type_get_method_by_param_names(
    patch_handle_t type, const char *name, int argc, const char **names) {
    return FindMethod(type, name, argc, nullptr, names);
}

extern "C" patch_handle_t patchlib_type_get_method_by_param_types(
    patch_handle_t type, const char *name, int argc,
    const patch_handle_t *types) {
    return FindMethod(type, name, argc, types, nullptr);
}

extern "C" patch_handle_t patchlib_type_get_method_by_signature(
    patch_handle_t type, const char *name, int argc,
    const patch_handle_t *types, const char **names) {
    return FindMethod(type, name, argc, types, names);
}

extern "C" bool patchlib_type_get_inner_types(patch_handle_t type,
                                              bool including_parent,
                                              tefstd_vector_t *out) {
    if (!patchlib_is_valid(type) || !out) return false;
    OutputHandles(CollectFromHierarchy(type, including_parent, NestedTypesIter()), out);
    return true;
}

extern "C" bool patchlib_type_get_methods(patch_handle_t type,
                                          bool including_parent,
                                          tefstd_vector_t *out) {
    if (!patchlib_is_valid(type) || !out) return false;
    OutputHandles(CollectFromHierarchy(type, including_parent, MethodsIter()), out);
    return true;
}

extern "C" bool patchlib_type_get_fields(patch_handle_t type,
                                         bool including_parent,
                                         tefstd_vector_t *out) {
    if (!patchlib_is_valid(type) || !out) return false;
    OutputHandles(CollectFromHierarchy(type, including_parent, FieldsIter()), out);
    return true;
}

extern "C" bool patchlib_type_get_properties(patch_handle_t type,
                                             bool including_parent,
                                             tefstd_vector_t *out) {
    if (!patchlib_is_valid(type) || !out) return false;
    OutputHandles(CollectFromHierarchy(type, including_parent, PropertiesIter()), out);
    return true;
}
