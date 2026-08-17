# 📋 TEFKernel ModLoader 开发指南

> **⚠️ 核心定位**：ModLoader 是 TEFKernel 中**专门负责加载和管理游戏 Mod** 的容器组件。它运行在 Module 之后，可以利用 Module 提供的游戏扩展功能（如材质包、UI 系统等）来为 Mod 提供更丰富的运行环境。ModLoader 本身不直接实现游戏功能，而是作为 Mod 的"宿主"。

---

## 📑 目录

<details>
<summary><b>📖 点击展开完整目录</b></summary>

- [📖 概述](#-概述)
  - [核心职责](#核心职责)
  - [与插件/模块的关系](#与插件模块的关系)
  - [生命周期](#生命周期)
- [🚀 快速开始](#-快速开始)
  - [最小 ModLoader 示例](#最小-modloader-示例)
  - [编译命令](#编译命令)
- [📚 API 参考](#-api-参考)
  - [ModLoader 信息结构](#modloader-信息结构)
  - [操作函数表](#操作函数表)
  - [Mod 清单结构](#mod-清单结构)
  - [ModLoader 条目结构](#modloader-条目结构)
  - [联机信息结构](#联机信息结构)
  - [结果码](#结果码)
  - [强制导出函数](#强制导出函数)
  - [使用模块功能](#使用模块功能)
- [💡 完整示例：多格式 ModLoader](#-完整示例多格式-modloader)
- [📦 打包与部署](#-打包与部署)
  - [目录结构](#目录结构)
  - [使用 TEFPkg-Tool 打包](#使用-tefpkg-tool-打包)
  - [部署配置](#部署配置)
  - [Mod 目录结构](#mod-目录结构)
- [🔄 Mod 生命周期管理](#-mod-生命周期管理)
  - [加载流程](#加载流程)
  - [卸载流程](#卸载流程)
  - [热重载流程](#热重载流程)
- [🔒 联机安全机制](#-联机安全机制)
- [⚠️ 注意事项](#-注意事项)
  - [内存管理](#内存管理)
  - [Mod 隔离](#mod-隔离)
  - [错误处理](#错误处理)
  - [性能考虑](#性能考虑)
- [🔗 相关链接](#-相关链接)

</details>

---

## 📖 概述

### 核心职责

| 职责              | 说明                                        |
|:------------------|:--------------------------------------------|
| **📂 Mod 加载**   | 从指定目录加载 Mod 文件（.tefpkg、.zip 等） |
| **🔧 Mod 初始化** | 调用 Mod 的初始化逻辑，准备运行环境         |
| **🔄 Mod 热重载** | 支持 Mod 在运行时重新加载                   |
| **🗑️ Mod 卸载**   | 清理 Mod 占用的资源                         |
| **🔒 联机安全**   | 提供联机安全检查，标记 Mod 是否可联机       |
| **📋 依赖管理**   | 声明 ModLoader 依赖的插件                   |

### 与插件/模块的关系

| 类型              | 定位           | 依赖关系             |
|:------------------|:---------------|:---------------------|
| **插件 (Plugin)** | 底层符号提供者 | 不依赖其他组件       |
| **模块 (Module)** | 游戏功能扩展   | 可依赖插件符号       |
| **ModLoader**     | Mod 加载容器   | 可依赖插件和模块功能 |
| **Mod**           | 最终用户模组   | 由 ModLoader 管理    |

> **加载顺序**：插件 → 模块 → ModLoader → Mod

### 生命周期

```
内核加载 .tefpkg
↓
memdl_open() 加载动态库
↓
调用 ml_create() 获取操作表
↓
验证插件依赖 (plugin_dependencies)
↓
调用 init_ml() → ModLoader 自身初始化
↓
内核读取 modloader/{pkg_id}/enables.txt
↓
对每个启用的 Mod:
├── 构建 mod_manifest_t
├── 调用 load_mod() 加载 Mod
└── 调用 init_mod() 初始化 Mod
↓
ModLoader 正常运行 (游戏运行期间)
↓
(可选) 热重载 → 调用 reload_mod()
↓
卸载时:
├── 调用 unload_mod() 卸载所有 Mod
└── 调用 cleanup_ml() 清理 ModLoader
↓
卸载动态库
```

---

## 🚀 快速开始

### 最小 ModLoader 示例

```c
// minimal_modloader.c
#include "modloader/modloader_core.h"
#include <stdio.h>

// ============================================================
// 1. ModLoader 信息 (静态常量)
// ============================================================
static const ml_info_t g_info = {
    .pkg_id = "com.example.minimal_ml",
    .version = "1.0.0",
    .version_code = 1,
    .api_version = 1,
    .plugin_dependencies_sizes = 0,
    .plugin_dependencies = NULL
};

// ============================================================
// 2. Mod 操作函数
// ============================================================
static ml_result_t load_mod(mod_manifest_t* manifest) {
    printf("[ModLoader] Loading mod: %s\n", manifest->mod_id);
    printf("  Path: %s\n", manifest->path);
    printf("  Private dir: %s\n", manifest->private_dir);
    printf("  Logs dir: %s\n", manifest->logs_dir);
    return ML_SUCCESS;
}

static ml_result_t unload_mod(mod_manifest_t* manifest) {
    printf("[ModLoader] Unloading mod: %s\n", manifest->mod_id);
    return ML_SUCCESS;
}

static ml_result_t reload_mod(mod_manifest_t* manifest) {
    printf("[ModLoader] Reloading mod: %s\n", manifest->mod_id);
    return ML_SUCCESS;
}

static ml_result_t init_mod(mod_manifest_t* manifest) {
    printf("[ModLoader] Initializing mod: %s\n", manifest->mod_id);
    return ML_SUCCESS;
}

static const multiplayer_mod_info_t* get_multiplayer_info(mod_manifest_t* manifest) {
    // 返回静态联机信息
    static multiplayer_mod_info_t info = {
        .mod_id = "com.example.mod",
        .is_multiplayer_safe = 1,
        .version_code = 1,
        .version = "1.0.0"
    };
    return &info;
}

// ============================================================
// 3. ModLoader 生命周期
// ============================================================
static ml_result_t init_ml(ml_entry_t* entry) {
    printf("[ModLoader] Initializing ModLoader...\n");
    printf("  Private dir: %s\n", entry->private_dir);
    printf("  Logs dir: %s\n", entry->logs_dir);
    return ML_SUCCESS;
}

static ml_result_t cleanup_ml(ml_entry_t* entry) {
    printf("[ModLoader] Cleanup complete.\n");
    return ML_SUCCESS;
}

static const ml_info_t* get_info(void) {
    return &g_info;
}

// ============================================================
// 4. 操作表 (静态常量)
// ============================================================
static const ml_ops_t g_ops = {
    .load_mod = load_mod,
    .unload_mod = unload_mod,
    .reload_mod = reload_mod,
    .init_mod = init_mod,
    .get_multiplayer_info = get_multiplayer_info,
    .init_ml = init_ml,
    .cleanup_ml = cleanup_ml,
    .get_info = get_info
};

// ============================================================
// 5. 强制导出
// ============================================================
API_EXPORT const ml_ops_t* API_CALL ml_create(void) {
    return &g_ops;
}
```

### 编译命令

```bash
# Linux / Android
gcc -shared -fPIC -I/path/to/tefkernel minimal_modloader.c -o libminimal_ml.so

# Windows (MinGW)
gcc -shared -I/path/to/tefkernel minimal_modloader.c -o minimal_ml.dll

# macOS
gcc -shared -fPIC -I/path/to/tefkernel minimal_modloader.c -o libminimal_ml.dylib
```

---

## 📚 API 参考

### ModLoader 信息结构

```c
typedef struct {
    const char *pkg_id;                    ///< 唯一标识符 (反向域名)
    int version_code;                      ///< 版本代码 (数值比较)
    const char *version;                   ///< 版本字符串
    int api_version;                       ///< API 版本号
    int plugin_dependencies_sizes;         ///< 依赖插件数量
    const char **plugin_dependencies;      ///< 依赖插件 pkg_id 列表
} ml_info_t;
```

### 操作函数表

```c
typedef struct {
    /**
     * @brief 加载单个 Mod
     * @param mod_manifest Mod 描述信息
     * @return ML_SUCCESS 或错误码
     */
    ml_result_t (*load_mod)(mod_manifest_t *mod_manifest);

    /**
     * @brief 卸载单个 Mod
     * @param mod_manifest Mod 描述信息
     * @return ML_SUCCESS 或错误码
     */
    ml_result_t (*unload_mod)(mod_manifest_t *mod_manifest);

    /**
     * @brief 重新加载 Mod (热重载)
     * @param mod_manifest Mod 描述信息
     * @return ML_SUCCESS 或错误码
     */
    ml_result_t (*reload_mod)(mod_manifest_t *mod_manifest);

    /**
     * @brief 初始化单个 Mod
     * @param mod_manifest Mod 描述信息
     * @return ML_SUCCESS 或错误码
     */
    ml_result_t (*init_mod)(mod_manifest_t *mod_manifest);

    /**
     * @brief 获取 Mod 的联机检测信息
     * @param mod_manifest Mod 描述信息
     * @return 指向 multiplayer_mod_info_t 的指针 (静态内存)
     * @warning 内核不会释放返回的信息，需使用静态内存
     */
    const multiplayer_mod_info_t * (*get_multiplayer_info)(mod_manifest_t *mod_manifest);

    /**
     * @brief 初始化 ModLoader
     * @param ml_entry ModLoader 条目
     * @return ML_SUCCESS 或错误码
     */
    ml_result_t (*init_ml)(ml_entry_t* ml_entry);

    /**
     * @brief 清理并关闭 ModLoader
     * @param ml_entry ModLoader 条目
     * @return ML_SUCCESS 或错误码
     */
    ml_result_t (*cleanup_ml)(ml_entry_t* ml_entry);

    /**
     * @brief 获取 ModLoader 信息
     * @return 指向静态 ml_info_t 的指针
     */
    const ml_info_t *(*get_info)(void);
} ml_ops_t;
```

### Mod 清单结构

```c
typedef struct {
    const char *path;          ///< Mod 文件路径
    const char *mod_id;        ///< Mod 唯一标识符
    const char *private_dir;   ///< Mod 私有目录
    const char *logs_dir;      ///< Mod 日志目录
} mod_manifest_t;
```

### ModLoader 条目结构

```c
typedef struct ml_entry_t {
    ml_info_t *info;           ///< ModLoader 信息 (由内核填充)
    ml_ops_t *ops;             ///< 操作表 (由 ModLoader 提供)
    tefpkg_t *pkg_handle;      ///< 包句柄 (由内核填充)
    const char *private_dir;   ///< ModLoader 私有目录
    const char *logs_dir;      ///< ModLoader 日志目录
} ml_entry_t;
```

### 联机信息结构

```c
typedef struct {
    const char *mod_id;                ///< Mod 唯一标识符
    int is_multiplayer_safe;           ///< 是否可联机 (1=安全, 0=不安全)
    int version_code;                  ///< 版本代码
    const char *version;               ///< 版本字符串
} multiplayer_mod_info_t;
```

### 结果码

```c
typedef enum {
    ML_SUCCESS = 0,              ///< 操作成功
    ML_ERROR = -1,               ///< 一般性错误
    ML_ERROR_INVALID_PARAM = -2, ///< 参数无效
    ML_ERROR_NOT_FOUND = -3      ///< Mod 未找到
} ml_result_t;
```

### 强制导出函数

```c
/**
 * @brief ModLoader 必须导出的唯一函数
 * @return 指向 ModLoader 操作函数表的指针 (必须是静态内存)
 */
API_EXPORT const ml_ops_t * API_CALL ml_create(void);
```

### 使用模块功能

```c
#include "patchlib/type.h"
#include "patchlib/method.h"

// 示例：在 ModLoader 中使用 Module 提供的纹理加载功能
static ml_result_t load_mod(mod_manifest_t* manifest) {
    // 假设 Module 注册了纹理加载符号
    typedef bool (*load_texture_t)(const char* path);
    load_texture_t load_tex = (load_texture_t)tpf_get_symbol("load_texture");
    
    if (load_tex) {
        // 加载 Mod 的纹理
        load_tex(manifest->path);
    }
    
    return ML_SUCCESS;
}
```

---

## 💡 完整示例：多格式 ModLoader

这个示例展示了一个支持 `.tefpkg` 和 `.zip` 两种格式的 ModLoader：

```c
// advanced_modloader.c
#include "modloader/modloader_core.h"
#include "tefpackage/tefpkg.h"
#include "patchlib/type.h"
#include "patchlib/method.h"
#include "tefstd/vector.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// ============================================================
// 1. ModLoader 信息 (依赖一个模块)
// ============================================================
static const char* deps[] = {
    "com.example.texture_pack"  // 依赖纹理模块
};

static const ml_info_t g_info = {
    .pkg_id = "com.example.advanced_ml",
    .version = "2.0.0",
    .version_code = 200,
    .api_version = 2,
    .plugin_dependencies_sizes = 1,
    .plugin_dependencies = deps
};

// ============================================================
// 2. ModLoader 私有状态
// ============================================================
typedef struct {
    tefstd_vector_t loaded_mods;   // 已加载的 Mod ID 列表
    char mods_dir[512];             // Mod 存放目录
    char logs_dir[512];             // 日志目录
} ml_state_t;

static ml_state_t g_state = {0};

// ============================================================
// 3. Mod 操作实现
// ============================================================

// 检查 Mod 格式并加载
static ml_result_t load_mod(mod_manifest_t* manifest) {
    printf("[ML] Loading mod: %s\n", manifest->mod_id);
    printf("[ML]   Path: %s\n", manifest->path);
    printf("[ML]   Private: %s\n", manifest->private_dir);
    
    // 检查文件是否存在
    if (access(manifest->path, F_OK) != 0) {
        printf("[ML]   ERROR: File not found!\n");
        return ML_ERROR_NOT_FOUND;
    }
    
    // 根据扩展名选择加载方式
    const char* ext = strrchr(manifest->path, '.');
    if (ext && strcmp(ext, ".tefpkg") == 0) {
        // 加载 .tefpkg 格式
        tefpkg_t* pkg = NULL;
        tefpkg_result_t result = tefpkg_open_readonly(manifest->path, &pkg);
        if (result == TEF_OK) {
            printf("[ML]   Loaded .tefpkg with %d files\n", 
                   tefpkg_get_entries_count(pkg));
            tefpkg_close(pkg);
            // 保存加载记录
            char* id_copy = strdup(manifest->mod_id);
            if (id_copy) {
                tefstd_vector_push_back(&g_state.loaded_mods, &id_copy);
            }
            return ML_SUCCESS;
        } else {
            printf("[ML]   Failed to open .tefpkg: %d\n", result);
            return ML_ERROR;
        }
    } else if (ext && strcmp(ext, ".zip") == 0) {
        // 加载 .zip 格式 (示例: 调用 zip 处理)
        printf("[ML]   Loading .zip format (simulated)\n");
        return ML_SUCCESS;
    }
    
    printf("[ML]   Unknown format\n");
    return ML_ERROR;
}

// 卸载 Mod
static ml_result_t unload_mod(mod_manifest_t* manifest) {
    printf("[ML] Unloading mod: %s\n", manifest->mod_id);
    
    // 从列表中移除
    for (size_t i = 0; i < tefstd_vector_size(&g_state.loaded_mods); i++) {
        char** ptr = tefstd_vector_at(&g_state.loaded_mods, i);
        if (ptr && *ptr && strcmp(*ptr, manifest->mod_id) == 0) {
            free(*ptr);
            tefstd_vector_erase(&g_state.loaded_mods, i, NULL);
            break;
        }
    }
    
    return ML_SUCCESS;
}

// 热重载 Mod
static ml_result_t reload_mod(mod_manifest_t* manifest) {
    printf("[ML] Reloading mod: %s\n", manifest->mod_id);
    // 先卸载再加载
    unload_mod(manifest);
    return load_mod(manifest);
}

// 初始化 Mod
static ml_result_t init_mod(mod_manifest_t* manifest) {
    printf("[ML] Initializing mod: %s\n", manifest->mod_id);
    // 这里可以调用 Mod 的入口函数
    return ML_SUCCESS;
}

// ============================================================
// 4. 联机信息
// ============================================================
static const multiplayer_mod_info_t* get_multiplayer_info(mod_manifest_t* manifest) {
    // 静态存储，内核会使用但不会释放
    static multiplayer_mod_info_t info;
    
    // 根据 Mod ID 返回不同的联机信息
    if (strcmp(manifest->mod_id, "com.example.safe_mod") == 0) {
        info.is_multiplayer_safe = 1;
    } else if (strcmp(manifest->mod_id, "com.example.unsafe_mod") == 0) {
        info.is_multiplayer_safe = 0;
    } else {
        info.is_multiplayer_safe = 1; // 默认安全
    }
    
    info.mod_id = manifest->mod_id;
    info.version = "1.0.0";
    info.version_code = 1;
    
    return &info;
}

// ============================================================
// 5. ModLoader 生命周期
// ============================================================

static ml_result_t init_ml(ml_entry_t* entry) {
    printf("[ML] Initializing Advanced ModLoader...\n");
    
    // 1. 保存目录
    snprintf(g_state.mods_dir, sizeof(g_state.mods_dir), "%s/mods", 
             entry->private_dir);
    snprintf(g_state.logs_dir, sizeof(g_state.logs_dir), "%s", 
             entry->logs_dir);
    
    // 2. 创建目录
    mkdir(g_state.mods_dir, 0755);
    mkdir(g_state.logs_dir, 0755);
    
    // 3. 初始化状态
    tefstd_vector_init(&g_state.loaded_mods, sizeof(char*));
    
    printf("[ML] Mods directory: %s\n", g_state.mods_dir);
    printf("[ML] Logs directory: %s\n", g_state.logs_dir);
    
    return ML_SUCCESS;
}

static ml_result_t cleanup_ml(ml_entry_t* entry) {
    printf("[ML] Cleaning up ModLoader...\n");
    
    // 释放所有 Mod 记录
    for (size_t i = 0; i < tefstd_vector_size(&g_state.loaded_mods); i++) {
        char** ptr = tefstd_vector_at(&g_state.loaded_mods, i);
        if (ptr && *ptr) free(*ptr);
    }
    tefstd_vector_destroy(&g_state.loaded_mods);
    
    return ML_SUCCESS;
}

static const ml_info_t* get_info(void) {
    return &g_info;
}

// ============================================================
// 6. 操作表与导出
// ============================================================
static const ml_ops_t g_ops = {
    .load_mod = load_mod,
    .unload_mod = unload_mod,
    .reload_mod = reload_mod,
    .init_mod = init_mod,
    .get_multiplayer_info = get_multiplayer_info,
    .init_ml = init_ml,
    .cleanup_ml = cleanup_ml,
    .get_info = get_info
};

API_EXPORT const ml_ops_t* API_CALL ml_create(void) {
    return &g_ops;
}
```

---

## 📦 打包与部署

### 目录结构

```
工作目录/
├── modloader/
│   ├── enables.txt          # 启用的 ModLoader 列表
│   └── pkg/
│       └── com.example.advanced_ml.tefpkg
│
└── mods/                    # ModLoader 运行时目录
    └── com.example.advanced_ml/
        ├── private/          # 私有目录 (ml_entry_t->private_dir)
        ├── logs/             # 日志目录 (ml_entry_t->logs_dir)
        ├── enables.txt       # 该 ModLoader 启用的 Mod 列表
        └── mod/              # Mod 存放目录
            ├── com.example.mod1.tefpkg
            ├── com.example.mod2.tefpkg
            └── com.example.mod3.zip
```

### 使用 TEFPkg-Tool 打包

> **官方工具**：https://github.com/eternalfuture-e38299/TEFPkg-Tool

#### 动态库命名约束

```text
libloader.android.arm64.so
libloader.android.arm.so
libloader.linux.x64.so
libloader.linux.x86.so
loader.windows.x64.dll
loader.windows.x86.dll
libloader.mac.arm64.dylib
libloader.mac.x64.dylib
libloader.ios.arm64.dylib
libloader.ios.x64.dylib
libloader.ios.arm64-simulator.dylib
```

#### 执行打包

```bash
# 下载并编译 TEFPkg-Tool
git clone https://github.com/eternalfuture-e38299/TEFPkg-Tool.git
cd TEFPkg-Tool
mkdir build && cd build
cmake .. && make

# 打包 ModLoader
./tefpkg_tool build <dir> <outfile> <fingerprint>
```

### 部署配置

#### modloader/enables.txt

```text
# 启用的 ModLoader 列表
com.example.advanced_ml
com.example.another_ml
```

#### ModLoader 的 enables.txt

* 该 ModLoader 启用的 Mod 列表
* 路径: mods/{pkg_id}/enables.txt

```text
com.example.mod1
com.example.mod2
com.example.mod3
```

### Mod 目录结构

每个 ModLoader 有独立的 Mod 目录：

```
mods/com.example.advanced_ml/
├── enables.txt           # 启用的 Mod 列表
├── private/              # Mod 私有数据 (传递给 mod_manifest_t)
│   ├── com.example.mod1/
│   └── com.example.mod2/
├── logs/                 # Mod 日志 (传递给 mod_manifest_t)
│   ├── com.example.mod1/
│   └── com.example.mod2/
└── mod/                  # Mod 文件存放
    ├── com.example.mod1.tefpkg
    └── com.example.mod2.tefpkg
```

---

## 🔄 Mod 生命周期管理

### 加载流程

```
1. 内核读取 mods/{pkg_id}/enables.txt
2. 对每个 Mod ID:
   a. 构建 mod_manifest_t:
      - path: mods/{pkg_id}/mod/{mod_id}.tefpkg
      - mod_id: 从 enables.txt 读取
      - private_dir: mods/{pkg_id}/private/{mod_id}
      - logs_dir: mods/{pkg_id}/logs/{mod_id}
   b. 调用 ml_ops->load_mod(manifest)
   c. 如果成功，调用 ml_ops->init_mod(manifest)
```

### 卸载流程

```
1. 对每个已加载的 Mod:
   a. 调用 ml_ops->unload_mod(manifest)
   b. 清理 Mod 资源
```

### 热重载流程

```
1. 内核检测到文件变化 (或手动触发)
2. 对每个启用的 Mod:
   a. 调用 ml_ops->reload_mod(manifest)
   b. 重新加载 Mod 资源
```

---

## 🔒 联机安全机制

```c
// Mod 联机信息示例
static const multiplayer_mod_info_t* get_multiplayer_info(mod_manifest_t* manifest) {
    static multiplayer_mod_info_t info;
    
    // 根据 Mod 类型判断是否安全
    if (is_mod_trusted(manifest->mod_id)) {
        info.is_multiplayer_safe = 1;  // 可联机
    } else {
        info.is_multiplayer_safe = 0;  // 不可联机
    }
    
    return &info;
}
```

---

## ⚠️ 注意事项

### 内存管理

```c
// ✅ 正确：使用静态内存返回联机信息
static multiplayer_mod_info_t g_multiplayer_info;
static const multiplayer_mod_info_t* get_multiplayer_info(mod_manifest_t* manifest) {
    // 填充静态结构
    g_multiplayer_info.mod_id = manifest->mod_id;
    g_multiplayer_info.is_multiplayer_safe = 1;
    return &g_multiplayer_info;  // 安全
}

// ❌ 错误：动态分配返回 (内核不会释放)
static const multiplayer_mod_info_t* get_multiplayer_info(mod_manifest_t* manifest) {
    multiplayer_mod_info_t* info = malloc(sizeof(multiplayer_mod_info_t));  // 内存泄漏！
    return info;
}
```

### Mod 隔离

```c
// ✅ 每个 Mod 使用独立目录
static ml_result_t load_mod(mod_manifest_t* manifest) {
    // 创建 Mod 专属目录
    mkdir(manifest->private_dir, 0755);
    mkdir(manifest->logs_dir, 0755);
    
    // Mod 的数据只写入自己的 private_dir
    return ML_SUCCESS;
}
```

### 错误处理

```c
// ✅ 正确处理错误
static ml_result_t load_mod(mod_manifest_t* manifest) {
    if (!manifest || !manifest->path) {
        return ML_ERROR_INVALID_PARAM;
    }
    
    if (access(manifest->path, F_OK) != 0) {
        return ML_ERROR_NOT_FOUND;
    }
    
    // 加载逻辑...
    
    return ML_SUCCESS;
}
```

### 性能考虑

| 建议           | 说明                                            |
|:---------------|:------------------------------------------------|
| **延迟初始化** | Mod 在 init_mod 中完成初始化，load_mod 只做加载 |
| **缓存结果**   | 缓存 Mod 的元数据，避免重复解析                 |
| **异步加载**   | 大 Mod 可以考虑异步加载                         |
| **日志级别**   | 发布版本减少调试日志                            |

---

## 🔗 相关链接

- [TEFPkg-Tool 官方仓库](https://github.com/eternalfuture-e38299/TEFPkg-Tool)
- [PatchLib API 参考](./patchlib.md)
- [TEFPKG 格式文档](./tefpkg.md)
- [插件 (Plugin) 开发指南](./plugin.md)
- [模块 (Module) 开发指南](./module.md)

---

*Happy ModLoader Development! 🚀📋✨*