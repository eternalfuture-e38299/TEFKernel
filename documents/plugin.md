# 🔌 TEFKernel 插件 (Plugin) 开发指南

> **⚠️ 核心定位**：插件是 TEFKernel 架构中 **纯粹的符号提供者**，专为 Module 和 ModLoader 注册函数/变量符号。插件不实现具体业务逻辑，不参与游戏交互，其全部价值在于导出可供其他组件使用的底层 API。

---

## 📑 目录

<details>
<summary><b>📖 点击展开完整目录</b></summary>

- [📖 概述](#-概述)
  - [核心职责](#核心职责)
  - [生命周期](#生命周期)
- [🚀 快速开始](#-快速开始)
  - [最小插件示例](#最小插件示例)
  - [编译命令](#编译命令)
- [📚 API 参考](#-api-参考)
  - [插件信息结构](#插件信息结构)
  - [操作函数表](#操作函数表)
  - [强制导出函数](#强制导出函数)
  - [符号注册 API](#符号注册-api)
- [💡 完整示例：符号提供者](#-完整示例符号提供者)
- [📦 打包与部署](#-打包与部署)
  - [目录结构](#目录结构)
  - [使用 TEFPkg-Tool 打包](#使用-tefpkg-tool-打包)
  - [部署配置](#部署配置)
- [⚠️ 注意事项](#-注意事项)
  - [符号注册规则](#符号注册规则)
  - [内存管理](#内存管理)
  - [线程安全](#线程安全)
- [🔗 相关链接](#-相关链接)

</details>

---

## 📖 概述

### 核心职责

| 职责            | 说明                                                |
|:----------------|:----------------------------------------------------|
| **📦 符号注册** | 在 `initialize` 中通过 `TPF_SYMBOL()` 注册函数/变量 |
| **🔌 API 提供** | 提供底层函数实现，供 Module/ModLoader 调用          |
| **🔄 版本管理** | 通过 `version_code` 管理 API 兼容性                 |
| **🧹 资源清理** | 在 `cleanup` 中释放插件自身分配的资源               |

### 生命周期

```
内核加载 .tefpkg
↓
memdl_open() 加载动态库
↓
调用 tpf_create_plugin() 获取操作表
↓
调用 initialize()  → 注册所有符号
↓
符号可供 Module/ModLoader 使用
↓
（游戏运行期间）
↓
调用 cleanup() → 释放资源
↓
卸载动态库
```

---

## 🚀 快速开始

### 最小插件示例

```c
// minimal_plugin.c
#include "tefplugin/tpf_core.h"
#include <stdio.h>

// ============================================================
// 1. 插件信息 (静态常量)
// ============================================================
static const tpf_plugin_info_t g_info = {
    .pkg_id = "com.example.minimal",
    .name = "Minimal Plugin",
    .author = "Your Name",
    .version = "1.0.0",
    .version_code = 1
};

// ============================================================
// 2. 要注册的符号函数
// ============================================================
int get_answer(void) {
    return 42;
}

// ============================================================
// 3. 生命周期函数
// ============================================================
static bool init(plugin_handle_t* handle) {
    TPF_SYMBOL(get_answer);  // 注册符号
    return true;
}

static void cleanup(plugin_handle_t* handle) {
    // 无资源需要释放
}

static const tpf_plugin_info_t* get_info(void) {
    return &g_info;
}

// ============================================================
// 4. 操作表 (静态常量)
// ============================================================
static const tpf_plugin_ops_t g_ops = {
    .initialize = init,
    .cleanup = cleanup,
    .get_info = get_info
};

// ============================================================
// 5. 强制导出
// ============================================================
API_EXPORT const tpf_plugin_ops_t* API_CALL tpf_create_plugin(void) {
    return &g_ops;
}
```

### 编译命令

```bash
# Linux / Android
gcc -shared -fPIC -I/path/to/tefkernel minimal_plugin.c -o libminimal.so

# Windows (MinGW)
gcc -shared -I/path/to/tefkernel minimal_plugin.c -o minimal.dll

# macOS
gcc -shared -fPIC -I/path/to/tefkernel minimal_plugin.c -o libminimal.dylib
```

---

## 📚 API 参考

### 插件信息结构

```c
typedef struct {
    const char *pkg_id;     ///< 插件唯一标识符 (反向域名格式)
    const char *name;       ///< 插件显示名称
    const char *author;     ///< 作者
    const char *version;    ///< 版本字符串
    int version_code;       ///< 版本代码 (数值比较)
} tpf_plugin_info_t;
```

### 操作函数表

```c
typedef struct {
    /**
     * @brief 初始化插件 (唯一注册符号的机会)
     * @param this_handle 插件句柄 (由内核传入)
     * @return true=成功, false=失败 (导致插件卸载)
     */
    bool (*initialize)(plugin_handle_t *this_handle);

    /**
     * @brief 清理插件 (卸载前调用)
     * @param this_handle 插件句柄
     */
    void (*cleanup)(plugin_handle_t *this_handle);

    /**
     * @brief 获取插件信息
     * @return 指向静态信息的指针
     */
    const tpf_plugin_info_t *(*get_info)(void);
} tpf_plugin_ops_t;
```

### 强制导出函数

```c
/**
 * @brief 插件必须导出的唯一函数
 * @return 指向插件操作函数表的指针 (必须是静态内存)
 * @warning 不能动态分配内存返回
 */
API_EXPORT const tpf_plugin_ops_t * API_CALL tpf_create_plugin(void);
```

### 符号注册 API

```c
/**
 * @brief 注册插件符号 (底层函数)
 * @param this_handle 当前插件句柄
 * @param name 符号名称 (字符串)
 * @param addr 符号地址 (函数或变量指针)
 * @return true=成功, false=失败
 */
DEFINE_FUNCTION(bool, tpf_register_symbol,
                plugin_handle_t* this_handle,
                const char *name,
                const void *addr);

/**
 * @brief 注册符号的便捷宏
 * @param func 函数名称 (自动使用 #func 作为符号名)
 * @example TPF_SYMBOL(my_function);
 */
#define TPF_SYMBOL(func) \
    tpf_register_symbol(this_handle, #func, (const void *)(func))
```

> **⚠️ 重要**：所有符号注册必须且只能在 `initialize` 函数中完成。

---

## 💡 完整示例：符号提供者

```c
// symbol_provider.c
#include "tefplugin/tpf_core.h"
#include "patchlib/type.h"
#include "patchlib/field.h"
#include <stdio.h>
#include <string.h>

// ============================================================
// 1. 插件信息
// ============================================================
static const tpf_plugin_info_t g_plugin_info = {
    .pkg_id = "com.example.symbol_provider",
    .name = "Terraria API Provider",
    .author = "TEFKernel Team",
    .version = "2.0.0",
    .version_code = 200
};

// ============================================================
// 2. 要导出的符号函数
// ============================================================

// 符号 1: 获取游戏版本
int get_game_version(void) {
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    if (!patchlib_is_valid(main_type)) return -1;
    
    patch_handle_t field = patchlib_type_get_field(main_type, "curRelease");
    int version = 0;
    patchlib_field_get_value(field, PATCH_NULL, &version);
    return version;
}

// 符号 2: 获取玩家生命值
int get_player_life(void) {
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    patch_handle_t player_field = patchlib_type_get_field(main_type, "player");
    patch_handle_t player = PATCH_NULL;
    patchlib_field_get_value(player_field, PATCH_NULL, &player);
    
    if (!patchlib_is_valid(player)) return -1;
    
    patch_handle_t player_type = patchlib_type_get_type("Terraria", "Player");
    patch_handle_t life_field = patchlib_type_get_field(player_type, "statLife");
    int life = 0;
    patchlib_field_get_value(life_field, player, &life);
    return life;
}

// 符号 3: 设置玩家生命值 (返回是否成功)
bool set_player_life(int new_life) {
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    patch_handle_t player_field = patchlib_type_get_field(main_type, "player");
    patch_handle_t player = PATCH_NULL;
    patchlib_field_get_value(player_field, PATCH_NULL, &player);
    
    if (!patchlib_is_valid(player)) return false;
    
    patch_handle_t player_type = patchlib_type_get_type("Terraria", "Player");
    patch_handle_t life_field = patchlib_type_get_field(player_type, "statLife");
    patchlib_field_set_value(life_field, player, &new_life);
    return true;
}

// 符号 4: API 版本常量
const int MOD_API_VERSION = 2;

// 符号 5: 消息格式化函数
const char* format_message(const char* template, int value) {
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), template, value);
    return buffer;
}

// ============================================================
// 3. 插件生命周期
// ============================================================

static bool my_plugin_initialize(plugin_handle_t* this_handle) {
    printf("[Plugin] Registering symbols...\n");
    
    // 注册所有导出的符号
    TPF_SYMBOL(get_game_version);
    TPF_SYMBOL(get_player_life);
    TPF_SYMBOL(set_player_life);
    TPF_SYMBOL(MOD_API_VERSION);
    TPF_SYMBOL(format_message);
    
    printf("[Plugin] All symbols registered successfully!\n");
    return true;
}

static void my_plugin_cleanup(plugin_handle_t* this_handle) {
    // 插件无需清理其他资源 (符号由内核自动管理)
    printf("[Plugin] Cleanup complete.\n");
}

static const tpf_plugin_info_t* my_plugin_get_info(void) {
    return &g_plugin_info;
}

// ============================================================
// 4. 操作表
// ============================================================
static const tpf_plugin_ops_t g_plugin_ops = {
    .initialize = my_plugin_initialize,
    .cleanup = my_plugin_cleanup,
    .get_info = my_plugin_get_info
};

// ============================================================
// 5. 强制导出
// ============================================================
API_EXPORT const tpf_plugin_ops_t* API_CALL tpf_create_plugin(void) {
    return &g_plugin_ops;
}
```

---

## 📦 打包与部署

### 目录结构

```
工作目录/
├── plugin/
│   ├── enables.txt          # 启用的插件列表 (每行一个 pkg_id)
│   └── pkg/
│       └── com.example.symbol_provider.tefpkg
```

### 使用 TEFPkg-Tool 打包

> **官方工具**：https://github.com/eternalfuture-e38299/TEFPkg-Tool

#### 动态库命名约束

```text
libplugin.android.arm64.so
libplugin.android.arm.so
libplugin.linux.x64.so
libplugin.linux.x86.so
plugin.windows.x64.dll
plugin.windows.x86.dll
libplugin.mac.arm64.dylib
libplugin.mac.x64.dylib
libplugin.ios.arm64.dylib
libplugin.ios.x64.dylib
libplugin.ios.arm64-simulator.dylib
```

#### 执行打包

```bash
# 下载并编译 TEFPkg-Tool
git clone https://github.com/eternalfuture-e38299/TEFPkg-Tool.git
cd TEFPkg-Tool
mkdir build && cd build
cmake .. && make

# 打包插件
./tefpkg_tool build <dir> <outfile> <fingerprint>
```

### 部署配置

#### enables.txt

* 每行一个插件 ID (不含 .tefpkg 扩展名)
* 一般不需要手动启用，TEFKernel会自动加载对应依赖
```text
com.example.symbol_provider
com.example.another_plugin
```

#### 部署命令

```bash
# 创建目录
mkdir -p workspace/plugin/pkg

# 复制包文件
cp com.example.symbol_provider.tefpkg workspace/plugin/pkg/

# 添加到启用列表
echo "com.example.symbol_provider" >> workspace/plugin/enables.txt
```

---

## ⚠️ 注意事项

### 符号注册规则

```c
// ✅ 正确：在 initialize 中注册
static bool init(plugin_handle_t* handle) {
    TPF_SYMBOL(my_function);
    TPF_SYMBOL(my_data);
    return true;
}

// ❌ 错误：在函数外部注册 (不会生效)
TPF_SYMBOL(my_function);  // 编译错误或运行时无效

// ❌ 错误：注册空指针
TPF_SYMBOL(NULL);  // 无效

// ✅ 正确：可以注册变量地址
int global_counter = 0;
TPF_SYMBOL(global_counter);
```

### 内存管理

```c
// ✅ 正确：使用静态内存
static const tpf_plugin_info_t g_info = {...};
static const tpf_plugin_ops_t g_ops = {...};
static char g_buffer[256];

// ❌ 错误：动态分配内存返回 (会导致内存泄漏)
const tpf_plugin_ops_t* tpf_create_plugin(void) {
    tpf_plugin_ops_t* ops = malloc(sizeof(tpf_plugin_ops_t));  // 危险！
    return ops;
}

// ✅ 正确：返回静态内存地址
const tpf_plugin_ops_t* tpf_create_plugin(void) {
    return &g_ops;  // 安全
}
```

### 线程安全

| 规则                 | 说明                               |
|:---------------------|:-----------------------------------|
| **主线程操作**       | 所有插件相关操作必须在主线程完成   |
| **串行初始化**       | 插件加载、初始化、符号注册是串行的 |
| **禁止并发符号注册** | 符号遍历时不能进行符号注册         |
| **清理时机**         | cleanup 在插件不再使用时调用       |

---

## 🔗 相关链接

- [TEFPkg-Tool 官方仓库](https://github.com/eternalfuture-e38299/TEFPkg-Tool)
- [PatchLib API 参考](./patchlib.md)
- [TEFPKG 格式文档](./tefpkg.md)
- [模组加载器 (ModLoader) 开发指南](./modloader.md)
- [模块 (Module) 开发指南](./module.md)
---

*Happy Plugin Development! 🚀✨*