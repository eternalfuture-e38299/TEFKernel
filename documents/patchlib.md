# 📚 PatchLib 使用指南

> **快速导航**：点击下方链接直接跳转到对应章节

## 📑 目录

<details>
<summary><b>📖 点击展开完整目录</b></summary>

- [🚀 快速入门](#-快速入门) - 包含头文件 + 基本使用流程

#### 类型系统 (type.h)
- [📦 类型系统](#-类型系统-typeh)
  - [获取类型](#获取类型)
  - [类型信息查询](#类型信息查询)
  - [批量获取成员](#批量获取成员)

#### 方法操作 (method.h)
- [⚡ 方法操作](#-方法操作-methodh)
  - [获取方法](#获取方法)
  - [方法信息查询](#方法信息查询)
  - [调用方法](#调用方法)
  - [泛型方法](#泛型方法)

#### Hook 系统
- [🪝 Hook 系统](#-hook-系统)
  - [Prefix/Postfix Hook](#prefixpostfix-hook)
  - [跳过原方法的 Hook](#跳过原方法的-hook)

#### 字段操作 (field.h)
- [📊 字段操作](#-字段操作-fieldh)

#### 属性操作 (property.h)
- [🏷️ 属性操作](#-属性操作-propertyh)

#### 数据结构操作
- [🏗️ 数据结构操作](#-数据结构操作)
  - [Array 数组](#array-数组)
  - [List 列表](#list-列表)
  - [Dictionary 字典](#dictionary-字典)
  - [String 字符串](#string-字符串)

#### 线程操作 (thread.h)
- [🧵 线程操作](#-线程操作-threadh)

#### 实践与参考
- [💡 完整示例](#-完整示例mod-开发中的典型用法)
- [⚠ 注意事项](#-注意事项)

</details>


## 🚀 快速入门

### 1. 包含头文件

```c
// 只需要包含需要的头文件即可
#include "patchlib/type.h"      // 类型系统
#include "patchlib/method.h"    // 方法操作
#include "patchlib/field.h"     // 字段操作
#include "patchlib/property.h"  // 属性操作

// 数据结构操作
#include "patchlib/struct/array.h"
#include "patchlib/struct/list.h"
#include "patchlib/struct/dictionary.h"
#include "patchlib/struct/string.h"

// 线程操作 (Android)
#include "patchlib/thread.h"
```

### 2. 基本使用流程

```c
// 1. 获取类型
patch_handle_t type = patchlib_type_get_type("Terraria", "Main");

// 2. 获取方法
patch_handle_t method = patchlib_type_get_method_by_param_count(type, "Initialize", 0);

// 3. 调用方法
patchlib_method_invoke_args(method, PATCH_NULL, NULL, NULL);

// 4. 清理资源 (桌面端)
#ifndef __ANDROID__
patchlib_free(type);
patchlib_free(method);
#endif
```

**[⬆ 回到顶部](#-patchlib-使用指南)**

---

## 📦 类型系统 (type.h)

### 获取类型

```c
// 基础类型
patch_handle_t int_type = patchlib_get_basic_type(PATCH_INT32);
patch_handle_t float_type = patchlib_get_basic_type(PATCH_FLOAT);
patch_handle_t string_type = patchlib_get_basic_type(PATCH_OBJECT);

// 游戏类型
patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
patch_handle_t player_type = patchlib_type_get_type("Terraria", "Player");
patch_handle_t npc_type = patchlib_type_get_type("Terraria", "NPC");

// 嵌套类型
patch_handle_t inner_type = patchlib_type_get_inner_type(main_type, "MyInnerClass");

// 泛型类型 (例如 List<string>)
patch_handle_t list_type_def = patchlib_type_get_type("System.Collections.Generic", "List`1");
tefstd_vector_t type_args;
tefstd_vector_init(&type_args, sizeof(patch_handle_t));
patch_handle_t string_type = patchlib_get_basic_type(PATCH_OBJECT);
tefstd_vector_push_back(&type_args, &string_type);
patch_handle_t list_string_type = patchlib_type_make_generic_type(list_type_def, &type_args);
tefstd_vector_destroy(&type_args);
```

### 类型信息查询

```c
// 获取类型名称
const char* name = patchlib_type_get_name(main_type);
printf("Type name: %s\n", name);

// 获取命名空间
const char* ns = patchlib_type_get_namespace(main_type);
printf("Namespace: %s\n", ns);

// 获取完整名称 (需要 free)
char* full_name = patchlib_type_get_full_name(main_type);
printf("Full name: %s\n", full_name);
free(full_name);

// 获取父类型
patch_handle_t parent = patchlib_type_get_parent(player_type);
if (patchlib_is_valid(parent)) {
    const char* parent_name = patchlib_type_get_name(parent);
    printf("Parent: %s\n", parent_name);
}

// 创建实例
patch_handle_t instance = patchlib_type_new_instance(player_type);
```

### 批量获取成员

```c
tefstd_vector_t methods;
tefstd_vector_t fields;
tefstd_vector_t properties;

// 获取所有方法 (包括父类)
patchlib_type_get_methods(main_type, true, &methods);
patchlib_type_get_fields(main_type, true, &fields);
patchlib_type_get_properties(main_type, true, &properties);

// 遍历方法
size_t count = tefstd_vector_size(&methods);
for (size_t i = 0; i < count; i++) {
    patch_handle_t* method_ptr = tefstd_vector_at(&methods, i);
    const char* method_name = patchlib_method_get_name(*method_ptr);
    printf("Method: %s\n", method_name);
}

// 清理资源
tefstd_vector_destroy(&methods);
tefstd_vector_destroy(&fields);
tefstd_vector_destroy(&properties);
```

**[⬆ 回到顶部](#-patchlib-使用指南)**

---

## ⚡ 方法操作 (method.h)

### 获取方法

```c
// 1. 按名称获取 (可能有重载)
patch_handle_t method = patchlib_type_get_method(main_type, "Initialize");

// 2. 按名称 + 参数数量获取 (推荐)
patch_handle_t init_method = patchlib_type_get_method_by_param_count(
    main_type, "Initialize", 0
);

// 3. 按名称 + 参数类型获取 (最精确)
patch_handle_t string_type = patchlib_get_basic_type(PATCH_OBJECT);
patch_handle_t method = patchlib_type_get_method_by_param_types(
    main_type, 
    "SomeMethod", 
    1, 
    &string_type
);

// 4. 按签名获取
const char* arg_names[] = {"message"};
patch_handle_t method = patchlib_type_get_method_by_signature(
    main_type,
    "PrintMessage",
    1,
    &string_type,
    arg_names
);
```

### 方法信息查询

```c
// 获取方法名称
const char* name = patchlib_method_get_name(method);

// 获取参数数量
int param_count = patchlib_method_get_param_count(method);

// 检查方法类型
bool is_static = patchlib_method_is_static(method);
bool is_instance = patchlib_method_is_instance(method);

// 获取返回值类型
patch_type_t return_type = patchlib_method_get_return_type(method);

// 获取方法 token
int token = patchlib_method_get_token(method);
```

### 调用方法

```c
// 调用静态方法 (无参数)
patch_handle_t method = patchlib_type_get_method_by_param_count(
    main_type, "Initialize", 0
);
patchlib_method_invoke_args(method, PATCH_NULL, NULL, NULL);

// 调用静态方法 (有参数)
patch_handle_t method = patchlib_type_get_method_by_param_types(
    main_type, "PrintMessage", 1, &string_type
);
patch_handle_t instance = ...;
const char* msg = "Hello World";
void* args[] = { (void*)&msg };
patchlib_method_invoke_args(method, PATCH_NULL, NULL, args);

// 调用实例方法
patch_handle_t player = patchlib_type_new_instance(player_type);
patch_handle_t method = patchlib_type_get_method_by_param_count(
    player_type, "Update", 0
);
patchlib_method_invoke_args(method, player, NULL, NULL);

// 调用有返回值的方法
patch_handle_t method = patchlib_type_get_method_by_param_count(
    player_type, "GetLife", 0
);
int life = 0;
patchlib_method_invoke_args(method, player, &life, NULL);
printf("Life: %d\n", life);

// 调用构造函数
patch_handle_t constructor = patchlib_type_get_method_by_param_count(
    string_type, ".ctor", 1
);
patch_handle_t new_string = PATCH_NULL;
const char* text = "Hello";
void* args[] = { (void*)&text };
patchlib_constructor_invoke(constructor, &new_string, args);
```

### 泛型方法

```c
// 假设有一个泛型方法: T GetValue<T>()
patch_handle_t generic_method = patchlib_type_get_method(
    some_type, "GetValue"
);

// 准备泛型参数
tefstd_vector_t template_types;
tefstd_vector_init(&template_types, sizeof(patch_handle_t));
patch_handle_t int_type = patchlib_get_basic_type(PATCH_INT32);
tefstd_vector_push_back(&template_types, &int_type);

// 实例化泛型方法
patch_handle_t concrete_method = patchlib_method_make_generic_instance(
    generic_method, &template_types
);

tefstd_vector_destroy(&template_types);

// 调用实例化后的方法
int result = 0;
patchlib_method_invoke_args(concrete_method, PATCH_NULL, &result, NULL);
```

**[⬆ 回到顶部](#-patchlib-使用指南)**

---

## 🪝 Hook 系统

### Prefix/Postfix Hook

```c
#include "patchlib/method.h"

// Prefix 回调: 在目标方法执行前调用
// 返回 true 表示跳过原方法，false 表示继续执行原方法
bool my_prefix(patch_handle_t instance, void** args, 
               const patch_method_signature_t* sig_info, void* result) {
    printf("Prefix: Method about to be called\n");
    
    // 修改参数 (例如第一个参数)
    if (args && args[0]) {
        int* param = (int*)args[0];
        *param = 100;  // 修改参数值
    }
    
    // 返回 false 继续执行原方法
    return false;
}

// Postfix 回调: 在目标方法执行后调用
void my_postfix(patch_handle_t instance, void** args, void* result,
                const patch_method_signature_t* sig_info) {
    printf("Postfix: Method executed\n");
    
    // 修改返回值
    if (result) {
        int* ret = (int*)result;
        *ret = *ret * 2;  // 返回值翻倍
    }
}

// 安装 Hook
patch_handle_t target_method = ...;
patch_hook_id_t hook_id = patchlib_install_prepost_hook(
    target_method,
    my_prefix,   // Prefix 回调
    my_postfix   // Postfix 回调
);

if (hook_id != PATCH_HOOK_INVALID_ID) {
    printf("Hook installed successfully! ID: %d\n", hook_id);
}

// 卸载 Hook
if (patchlib_uninstall_hook(hook_id)) {
    printf("Hook uninstalled successfully\n");
}
```

### 跳过原方法的 Hook

```c
// Prefix 返回 true 会跳过原方法
bool skip_prefix(patch_handle_t instance, void** args,
                 const patch_method_signature_t* sig_info, void* result) {
    printf("Skipping original method!\n");
    
    // 直接设置返回值
    if (result) {
        int* ret = (int*)result;
        *ret = 999;  // 自定义返回值
    }
    
    return true;  // 跳过原方法
}

patch_hook_id_t hook_id = patchlib_install_prepost_hook(
    target_method,
    skip_prefix,
    NULL  // 不需要 Postfix
);
```

**[⬆ 回到顶部](#-patchlib-使用指南)**

---

## 📊 字段操作 (field.h)

### 获取和操作字段

```c
// 获取字段
patch_handle_t life_field = patchlib_type_get_field(player_type, "statLife");
patch_handle_t name_field = patchlib_type_get_field(player_type, "name");

// 获取字段信息
const char* field_name = patchlib_field_get_name(life_field);
bool is_static = patchlib_field_is_static(life_field);
bool is_const = patchlib_field_is_const(life_field);
patch_type_t field_type = patchlib_field_get_type(life_field);
size_t field_size = patchlib_field_get_size(life_field);

// 读取字段值
patch_handle_t player_instance = ...;
int life = 0;
patchlib_field_get_value(life_field, player_instance, &life);
printf("Life: %d\n", life);

// 设置字段值
int new_life = 100;
patchlib_field_set_value(life_field, player_instance, &new_life);

// 静态字段操作
patch_handle_t static_field = patchlib_type_get_field(main_type, "SomeStaticField");
int static_value = 0;
patchlib_field_get_value(static_field, PATCH_NULL, &static_value);

// Android 平台：直接获取指针 (高性能场景)
#if __ANDROID__
void* field_ptr = patchlib_field_get_pointer(life_field, player_instance);
if (field_ptr) {
    int* life_ptr = (int*)field_ptr;
    *life_ptr = 200;  // 直接修改内存
}
#endif
```

**[⬆ 回到顶部](#-patchlib-使用指南)**

---

## 🏷️ 属性操作 (property.h)

### 获取和操作属性

```c
// 获取属性
patch_handle_t property = patchlib_type_get_property(player_type, "Life");

// 获取属性名称
const char* prop_name = patchlib_property_get_name(property);

// 获取 getter/setter 方法
patch_handle_t get_method = patchlib_property_get_get_method(property);
patch_handle_t set_method = patchlib_property_get_set_method(property);

// 使用 getter 读取属性值
int life = 0;
patchlib_method_invoke_args(get_method, player_instance, &life, NULL);

// 使用 setter 设置属性值
int new_life = 150;
void* args[] = { &new_life };
patchlib_method_invoke_args(set_method, player_instance, NULL, args);
```

**[⬆ 回到顶部](#-patchlib-使用指南)**

---

## 🏗️ 数据结构操作

### Array 数组

```c
#include "patchlib/struct/array.h"

// 创建数组
patch_handle_t int_type = patchlib_get_basic_type(PATCH_INT32);
patch_handle_t array = patchlib_array_create(10, int_type);

// 获取数组信息
size_t length = patchlib_array_length(array);
bool empty = patchlib_array_empty(array);

// 读取/设置元素
int value = 0;
patchlib_array_at(array, 0, &value);
printf("Array[0]: %d\n", value);

int new_value = 42;
patchlib_array_set(array, 0, &new_value);

// 填充数组
int fill_value = 99;
patchlib_array_fill(array, &fill_value);

// C 数组与游戏数组互转
int c_array[5] = {1, 2, 3, 4, 5};
patchlib_array_copy_from_c(array, c_array, 5);

int c_array_out[5];
patchlib_array_copy_to_c(c_array_out, array, 5);
```

### List 列表

```c
#include "patchlib/struct/list.h"

// 创建 List<int>
patch_handle_t int_type = patchlib_get_basic_type(PATCH_INT32);
patch_handle_t list = patchlib_list_create(10, int_type);

// 添加元素
int value = 42;
patchlib_list_add(list, &value);

// 删除元素
patchlib_list_remove_at(list, 0);

// 清空列表
patchlib_list_clear(list);

// 从数组复制
patch_handle_t array = patchlib_array_create(5, int_type);
patchlib_list_copy_from(list, array);

// 获取内部数组
patch_handle_t internal_array = patchlib_list_get_array(list);
size_t length = patchlib_array_length(internal_array);
```

### Dictionary 字典

```c
#include "patchlib/struct/dictionary.h"

// 创建 Dictionary<string, int>
patch_handle_t string_type = patchlib_get_basic_type(PATCH_OBJECT);
patch_handle_t int_type = patchlib_get_basic_type(PATCH_INT32);
patch_handle_t dict = patchlib_dictionary_create(string_type, int_type, 10);

// 添加键值对
const char* key = "score";
int value = 100;
patchlib_dictionary_add(dict, (void*)&key, &value);

// 获取值
int out_value = 0;
patchlib_dictionary_get_value(dict, (void*)&key, &out_value);
printf("score = %d\n", out_value);

// 修改值
int new_value = 200;
patchlib_dictionary_set_value(dict, (void*)&key, &new_value);

// 删除键值对
patchlib_dictionary_remove(dict, (void*)&key);

// 获取长度
size_t length = patchlib_dictionary_length(dict);
```

### String 字符串

```c
#include "patchlib/struct/string.h"

// 创建字符串
patch_handle_t str = patchlib_string_create("Hello World");

// 转换回 C 字符串 (需要 free)
char* c_str = patchlib_string_cstr(str);
printf("String: %s\n", c_str);
free(c_str);

// 判断是否为空
bool empty = patchlib_string_empty(str);

// 获取长度
size_t len = patchlib_string_length(str);
```

**[⬆ 回到顶部](#-patchlib-使用指南)**

---

## 🧵 线程操作 (thread.h)

```c
#include "patchlib/thread.h"

// 仅在 Android 平台有效
#if __ANDROID__
// 获取当前线程句柄
patch_handle_t thread = patchlib_thread_current();

// 附加当前线程到运行时 (在子线程调用时使用)
patch_handle_t attached = patchlib_thread_attach();

// 从运行时分离
patchlib_thread_detach(attached);
#endif
```

**[⬆ 回到顶部](#-patchlib-使用指南)**

---

## 💡 完整示例：Mod 开发中的典型用法

```c
#include "patchlib/type.h"
#include "patchlib/method.h"
#include "patchlib/field.h"
#include "patchlib/struct/list.h"
#include "patchlib/struct/string.h"

// 全局变量
static patch_hook_id_t g_hook_id = PATCH_HOOK_INVALID_ID;
static patch_handle_t g_player_type = PATCH_NULL;
static patch_handle_t g_life_field = PATCH_NULL;

// ============ Hook 回调函数 ============

// 伤害处理 Postfix
void damage_postfix(patch_handle_t instance, void** args, void* result,
                    const patch_method_signature_t* sig_info) {
    // 获取伤害值 (第二个参数)
    if (args && args[1]) {
        int* damage = (int*)args[1];
        printf("[Mod] Player took %d damage\n", *damage);
        
        // 伤害减半 (演示)
        *damage /= 2;
        printf("[Mod] Damage reduced to %d\n", *damage);
    }
}

// ============ 游戏初始化回调 ============

void on_game_initialized() {
    printf("[Mod] Game initialized!\n");
    
    // 1. 获取类型
    g_player_type = patchlib_type_get_type("Terraria", "Player");
    if (!patchlib_is_valid(g_player_type)) {
        printf("[Mod] Failed to get Player type!\n");
        return;
    }
    
    // 2. 获取 Main 类型并获取当前玩家
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    if (!patchlib_is_valid(main_type)) {
        printf("[Mod] Failed to get Main type!\n");
        return;
    }
    
    patch_handle_t player_field = patchlib_type_get_field(main_type, "player");
    if (!patchlib_is_valid(player_field)) {
        printf("[Mod] Failed to get player field!\n");
        return;
    }
    
    // 3. 读取玩家实例
    patch_handle_t player = PATCH_NULL;
    patchlib_field_get_value(player_field, PATCH_NULL, &player);
    
    if (!patchlib_is_valid(player)) {
        printf("[Mod] No player instance yet!\n");
        return;
    }
    
    // 4. 获取玩家生命值字段
    g_life_field = patchlib_type_get_field(g_player_type, "statLife");
    if (!patchlib_is_valid(g_life_field)) {
        printf("[Mod] Failed to get statLife field!\n");
        return;
    }
    
    int life = 0;
    patchlib_field_get_value(g_life_field, player, &life);
    printf("[Mod] Current life: %d\n", life);
    
    // 5. 创建 Hook 监控玩家伤害
    patch_handle_t hurt_method = patchlib_type_get_method_by_param_count(
        g_player_type, "Hurt", 2
    );
    
    if (patchlib_is_valid(hurt_method)) {
        g_hook_id = patchlib_install_prepost_hook(
            hurt_method,
            NULL,           // 不需要 Prefix
            damage_postfix  // Postfix 处理
        );
        
        if (g_hook_id != PATCH_HOOK_INVALID_ID) {
            printf("[Mod] Hook installed! ID: %d\n", g_hook_id);
        }
    }
}

// ============ Mod 初始化入口 ============

bool my_mod_init(module_entry_t* entry) {
    printf("[Mod] Initializing MyMod...\n");
    
    // 寻找游戏初始化方法
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    patch_handle_t init_method = patchlib_type_get_method_by_param_count(
        main_type, "Initialize", 0
    );
    
    if (patchlib_is_valid(init_method)) {
        // Hook 游戏初始化，在游戏初始化完成后执行我们的逻辑
        patchlib_install_prepost_hook(init_method, NULL, 
            (postfix_callback_t)on_game_initialized
        );
        printf("[Mod] Game init hook installed!\n");
    }
    
    return true;
}

// ============ Mod 清理 ============

void my_mod_cleanup(module_entry_t* entry) {
    printf("[Mod] Cleaning up MyMod...\n");
    
    // 卸载 Hook
    if (g_hook_id != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(g_hook_id);
        g_hook_id = PATCH_HOOK_INVALID_ID;
    }
    
    // 释放资源 (桌面端)
#ifndef __ANDROID__
    if (patchlib_is_valid(g_player_type)) {
        patchlib_free(g_player_type);
        g_player_type = PATCH_NULL;
    }
#endif
}
```

**[⬆ 回到顶部](#-patchlib-使用指南)**

---

## ⚠️ 注意事项

### 平台差异

| 功能                          | Android (IL2CPP) | 桌面 (Mono)       |
|:------------------------------|:-----------------|:------------------|
| `patchlib_free`               | 空操作 (自动 GC) | 需要手动调用      |
| `patchlib_handle_copy`        | 空操作           | 需要手动复制      |
| `patchlib_field_get_pointer`  | ✅ 支持          | ❌ 不支持         |
| `patchlib_method_get_pointer` | ✅ 支持          | ❌ 不支持         |
| `patchlib_thread_*`           | ✅ 支持          | ✅ 空操作但是支持 |

### 最佳实践

#### ✅ 总是检查句柄有效性

```c
patch_handle_t type = patchlib_type_get_type("Terraria", "Main");
if (!patchlib_is_valid(type)) {
    // 处理错误
    return;
}
```

#### ✅ 桌面平台记得释放资源

```c
#ifndef __ANDROID__
patchlib_free(type);
patchlib_free(method);
patchlib_free(instance);
#endif
```

#### ✅ 使用 `tefstd_vector_t` 批量操作

```c
tefstd_vector_t methods;
patchlib_type_get_methods(type, false, &methods);
// ... 处理
tefstd_vector_destroy(&methods);
```

#### ✅ Hook 回调不要做耗时操作

```c
// ❌ 不要这样做
bool my_prefix(...) {
    sleep(1);  // 会卡住游戏
    return false;
}

// ✅ 应该快速返回
bool my_prefix(...) {
    // 快速操作
    return false;
}
```

#### ✅ 在子线程中使用前先 attach

```c
#if __ANDROID__
void my_thread_func() {
    patch_handle_t thread = patchlib_thread_attach();
    // ... 操作
    patchlib_thread_detach(thread);
}
#endif
```

### 常见错误码

| 错误                    | 原因                 | 解决方案                   |
|:------------------------|:---------------------|:---------------------------|
| `PATCH_NULL`            | 类型/方法/字段不存在 | 检查命名空间和名称是否正确 |
| `PATCH_HOOK_INVALID_ID` | Hook 安装失败        | 检查 method 是否有效       |
| 崩溃                    | 参数类型不匹配       | 使用正确的参数类型调用方法 |

**[⬆ 回到顶部](#-patchlib-使用指南)**

*Happy Modding! 🎮✨*