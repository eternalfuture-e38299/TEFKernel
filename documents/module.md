# 📦 TEFKernel 模块 (Module) 开发指南

> **⚠️ 核心定位**：模块 (Module) 是 TEFKernel **直接管理的功能单元**，优先级高于 ModLoader。模块通常用于实现游戏层面的扩展功能，如材质包加载、UI 增强、音效替换等。模块拥有完整的生命周期管理，支持热重载，并且可以依赖插件 (Plugin) 提供的底层符号。

---

## 📑 目录

<details>
<summary><b>📖 点击展开完整目录</b></summary>

- [📖 概述](#-概述)
  - [核心职责](#核心职责)
  - [与插件/ModLoader 的关系](#与插件modloader-的关系)
  - [生命周期](#生命周期)
- [🚀 快速开始](#-快速开始)
  - [最小模块示例](#最小模块示例)
  - [编译命令](#编译命令)
- [📚 API 参考](#-api-参考)
  - [模块信息结构](#模块信息结构)
  - [操作函数表](#操作函数表)
  - [模块条目结构](#模块条目结构)
  - [强制导出函数](#强制导出函数)
  - [使用插件符号](#使用插件符号)
- [💡 完整示例：材质包加载模块](#-完整示例材质包加载模块)
- [📦 打包与部署](#-打包与部署)
  - [目录结构](#目录结构)
  - [使用 TEFPkg-Tool 打包](#使用-tefpkg-tool-打包)
  - [部署配置](#部署配置)
  - [模块优先级说明](#模块优先级说明)
- [🔄 热重载](#-热重载)
  - [工作原理](#工作原理)
  - [实现支持](#实现支持)
- [⚠️ 注意事项](#-注意事项)
  - [依赖管理](#依赖管理)
  - [资源管理](#资源管理)
  - [线程安全](#线程安全)
- [🔗 相关链接](#-相关链接)

</details>

---

## 📖 概述

### 核心职责

| 职责                | 说明                                              |
|:--------------------|:--------------------------------------------------|
| **🎨 游戏扩展**     | 实现材质包、UI 增强、音效替换等直接面向游戏的功能 |
| **🔌 使用插件符号** | 通过插件注册的符号调用底层 API                    |
| **📦 管理资源**     | 管理自己的私有目录和日志目录                      |
| **🔄 支持热重载**   | 实现 `hot_reload` 函数响应运行时更新              |
| **📋 依赖管理**     | 声明依赖的插件 (plugin_dependencies)              |

### 与插件/ModLoader 的关系

| 类型              | 定位           | 依赖关系       |
|:------------------|:---------------|:---------------|
| **插件 (Plugin)** | 底层符号提供者 | 不依赖其他组件 |
| **模块 (Module)** | 游戏功能扩展   | 可依赖插件符号 |
| **ModLoader**     | Mod 加载容器   | 可依赖模块功能 |

> **优先级顺序**：插件 → 模块 → ModLoader (加载顺序)

### 生命周期

```
内核加载 .tefpkg
↓
memdl_open() 加载动态库
↓
调用 module_create() 获取操作表
↓
验证插件依赖 (plugin_dependencies)
↓
调用 init_module() → 初始化模块
↓
模块正常运行 (游戏运行期间)
↓
(可选) 热重载触发 → 调用 hot_reload()
↓
卸载时调用 cleanup_module()
↓
卸载动态库
```

---

## 🚀 快速开始

### 最小模块示例

```c
// minimal_module.c
#include "module/module_core.h"
#include <stdio.h>

// ============================================================
// 1. 模块信息 (静态常量)
// ============================================================
static const module_info_t g_info = {
    .pkg_id = "com.example.minimal",
    .name = "Minimal Module",
    .author = "Your Name",
    .version = "1.0.0",
    .version_code = 1,
    .api_version = 1,
    .plugin_dependencies_sizes = 0,
    .plugin_dependencies = NULL
};

// ============================================================
// 2. 生命周期函数
// ============================================================
static bool init(module_entry_t* entry) {
    printf("[Module] Initialized!\n");
    printf("[Module] Private dir: %s\n", entry->private_dir);
    printf("[Module] Logs dir: %s\n", entry->logs_dir);
    return true;
}

static bool cleanup(module_entry_t* entry) {
    printf("[Module] Cleanup complete.\n");
    return true;
}

static void hot_reload(module_entry_t* entry) {
    printf("[Module] Hot reload triggered!\n");
}

static const module_info_t* get_info(void) {
    return &g_info;
}

// ============================================================
// 3. 操作表 (静态常量)
// ============================================================
static const module_ops_t g_ops = {
    .init_module = init,
    .cleanup_module = cleanup,
    .hot_reload = hot_reload,
    .get_info = get_info
};

// ============================================================
// 4. 强制导出
// ============================================================
API_EXPORT const module_ops_t* API_CALL module_create(void) {
    return &g_ops;
}
```

### 编译命令

```bash
# Linux / Android
gcc -shared -fPIC -I/path/to/tefkernel minimal_module.c -o libminimal_module.so

# Windows (MinGW)
gcc -shared -I/path/to/tefkernel minimal_module.c -o minimal_module.dll

# macOS
gcc -shared -fPIC -I/path/to/tefkernel minimal_module.c -o libminimal_module.dylib
```

---

## 📚 API 参考

### 模块信息结构

```c
typedef struct {
    const char *pkg_id;                    ///< 唯一标识符 (反向域名)
    const char *name;                      ///< 显示名称
    const char *author;                    ///< 作者
    const char *version;                   ///< 版本字符串
    int version_code;                      ///< 版本代码 (数值比较)
    int api_version;                       ///< API 版本号
    int plugin_dependencies_sizes;         ///< 依赖插件数量
    const char **plugin_dependencies;      ///< 依赖插件 pkg_id 列表
} module_info_t;
```

### 操作函数表

```c
typedef struct {
    /**
     * @brief 初始化模块
     * @param entry 模块条目 (包含目录、包句柄等信息)
     * @return true=成功, false=失败
     */
    bool (*init_module)(module_entry_t *entry);

    /**
     * @brief 清理并关闭模块 (卸载前调用)
     * @param entry 模块条目
     * @return true=成功, false=失败
     */
    bool (*cleanup_module)(module_entry_t *entry);

    /**
     * @brief 热重载操作 (运行时更新)
     * @param entry 模块条目
     */
    void (*hot_reload)(module_entry_t *entry);

    /**
     * @brief 获取模块信息
     * @return 指向静态信息的指针
     */
    const module_info_t *(*get_info)(void);
} module_ops_t;
```

### 模块条目结构

```c
typedef struct module_entry_t {
    module_info_t *info;        ///< 模块信息 (由内核填充)
    module_ops_t *ops;          ///< 操作表 (由模块提供)
    tefpkg_t *pkg_handle;       ///< 包句柄 (由内核填充)
    const char *private_dir;    ///< 私有目录路径 (用于存储配置等)
    const char *logs_dir;       ///< 日志目录路径 (独立日志)
} module_entry_t;
```

### 强制导出函数

```c
/**
 * @brief 模块必须导出的唯一函数
 * @return 指向模块操作函数表的指针 (必须是静态内存)
 */
API_EXPORT const module_ops_t * API_CALL module_create(void);
```

### 使用插件符号

```c
#include "patchlib/type.h"
#include "patchlib/method.h"

// 假设插件注册了符号: get_game_version()
// 获取方式 (在模块中使用):
typedef int (*get_game_version_t)(void);

// 通过符号名获取函数指针
// 注意：实际获取方式由 TEFKernel 提供，这里展示概念
get_game_version_t get_version = (get_game_version_t)tpf_get_symbol("get_game_version");
if (get_version) {
    int version = get_version();
    printf("Game version: %d\n", version);
}
```

---

## 💡 完整示例：材质包加载模块

```c
// texture_pack_module.c
#include "module/module_core.h"
#include "patchlib/type.h"
#include "patchlib/method.h"
#include "patchlib/field.h"
#include "tefstd/vector.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// ============================================================
// 1. 模块信息 (依赖一个底层插件)
// ============================================================
static const char* deps[] = {
    "com.example.texture_api"  // 依赖纹理处理插件
};

static const module_info_t g_info = {
    .pkg_id = "com.example.texture_pack",
    .name = "Texture Pack Loader",
    .author = "TEFKernel Team",
    .version = "1.0.0",
    .version_code = 1,
    .api_version = 1,
    .plugin_dependencies_sizes = 1,
    .plugin_dependencies = deps
};

// ============================================================
// 2. 模块私有状态
// ============================================================
typedef struct {
    char pack_dir[512];
    tefstd_vector_t loaded_textures;
    int total_packs;
} module_state_t;

static module_state_t g_state = {0};

// ============================================================
// 3. 功能函数：扫描并加载材质包
// ============================================================
static int scan_texture_packs(const char* pack_dir) {
    DIR* dir = opendir(pack_dir);
    if (!dir) return 0;
    
    int count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            // 检查是否为有效的材质包目录
            char pack_path[512];
            snprintf(pack_path, sizeof(pack_path), "%s/%s/pack.json", 
                     pack_dir, entry->d_name);
            if (access(pack_path, F_OK) == 0) {
                printf("[Module] Found texture pack: %s\n", entry->d_name);
                count++;
            }
        }
    }
    closedir(dir);
    return count;
}

// ============================================================
// 4. Hook: 游戏加载纹理时注入自定义纹理
// ============================================================
static void texture_load_postfix(patch_handle_t instance, void** args, void* result,
                                  const patch_method_signature_t* sig_info) {
    // 假设 args[0] 是纹理路径, result 是 Texture2D 对象
    if (args && args[0]) {
        const char* path = *(const char**)args[0];
        if (path && strstr(path, ".png")) {
            // 这里可以使用插件提供的纹理加载函数替换
            // 例如: load_custom_texture(path, result);
        }
    }
}

// ============================================================
// 5. 模块生命周期
// ============================================================

// 初始化
static bool init_module(module_entry_t* entry) {
    printf("[Module] Initializing Texture Pack Loader...\n");
    
    // 1. 保存私有目录
    snprintf(g_state.pack_dir, sizeof(g_state.pack_dir), "%s/packs", 
             entry->private_dir);
    
    // 2. 创建私有目录
    mkdir(g_state.pack_dir, 0755);
    mkdir(entry->logs_dir, 0755);
    
    // 3. 扫描材质包
    g_state.total_packs = scan_texture_packs(g_state.pack_dir);
    printf("[Module] Found %d texture packs\n", g_state.total_packs);
    
    // 4. 获取游戏纹理加载方法并 Hook
    patch_handle_t texture_type = patchlib_type_get_type(
        "Terraria.Graphics", "TextureManager"
    );
    if (patchlib_is_valid(texture_type)) {
        patch_handle_t load_method = patchlib_type_get_method_by_param_count(
            texture_type, "Load", 1
        );
        if (patchlib_is_valid(load_method)) {
            patchlib_install_prepost_hook(load_method, NULL, texture_load_postfix);
            printf("[Module] Texture load hook installed!\n");
        }
    }
    
    // 5. 初始化 vector
    tefstd_vector_init(&g_state.loaded_textures, sizeof(char*));
    
    return true;
}

// 清理
static bool cleanup_module(module_entry_t* entry) {
    printf("[Module] Cleaning up Texture Pack Loader...\n");
    
    // 释放加载的纹理列表
    for (size_t i = 0; i < tefstd_vector_size(&g_state.loaded_textures); i++) {
        char** ptr = tefstd_vector_at(&g_state.loaded_textures, i);
        if (ptr && *ptr) free(*ptr);
    }
    tefstd_vector_destroy(&g_state.loaded_textures);
    
    return true;
}

// 热重载：重新扫描材质包
static void hot_reload(module_entry_t* entry) {
    printf("[Module] Hot reload: Re-scanning texture packs...\n");
    
    // 清空旧列表
    for (size_t i = 0; i < tefstd_vector_size(&g_state.loaded_textures); i++) {
        char** ptr = tefstd_vector_at(&g_state.loaded_textures, i);
        if (ptr && *ptr) free(*ptr);
    }
    tefstd_vector_clear(&g_state.loaded_textures);
    
    // 重新扫描
    g_state.total_packs = scan_texture_packs(g_state.pack_dir);
    printf("[Module] Found %d texture packs (after reload)\n", g_state.total_packs);
}

static const module_info_t* get_info(void) {
    return &g_info;
}

// ============================================================
// 6. 操作表与导出
// ============================================================
static const module_ops_t g_ops = {
    .init_module = init_module,
    .cleanup_module = cleanup_module,
    .hot_reload = hot_reload,
    .get_info = get_info
};

API_EXPORT const module_ops_t* API_CALL module_create(void) {
    return &g_ops;
}
```

---

## 📦 打包与部署

### 目录结构

```
工作目录/
├── module/
│   ├── enables.txt          # 启用的模块列表 (每行一个 pkg_id)
│   └── pkg/
│       └── com.example.texture_pack.tefpkg
│
└── mods/                    # Module 运行时目录 (由内核自动创建)
    └── com.example.texture_pack/
        ├── private/          # 私有数据目录 (module_entry_t->private_dir)
        ├── logs/             # 日志目录 (module_entry_t->logs_dir)
        └── packs/            # 材质包存放目录 (示例中自定义)
```

### 使用 TEFPkg-Tool 打包

> **官方工具**：https://github.com/eternalfuture-e38299/TEFPkg-Tool

#### 动态库命名约束

```text
libmodule.android.arm64.so
libmodule.android.arm.so
libmodule.linux.x64.so
libmodule.linux.x86.so
module.windows.x64.dll
module.windows.x86.dll
libmodule.mac.arm64.dylib
libmodule.mac.x64.dylib
libmodule.ios.arm64.dylib
libmodule.ios.x64.dylib
libmodule.ios.arm64-simulator.dylib
```

#### 执行打包

```bash
# 下载并编译 TEFPkg-Tool
git clone https://github.com/eternalfuture-e38299/TEFPkg-Tool.git
cd TEFPkg-Tool
mkdir build && cd build
cmake .. && make

# 打包模块
./tefpkg_tool build <dir> <outfile> <fingerprint>
```

### 部署配置

#### enables.txt

* 每行一个模块 ID (不含 .tefpkg 扩展名)

```text
com.example.texture_pack
com.example.ui_enhance
com.example.audio_engine
```

#### 部署命令

```bash
# 创建目录
mkdir -p workspace/module/pkg

# 复制包文件
cp com.example.texture_pack.tefpkg workspace/module/pkg/

# 添加到启用列表
echo "com.example.texture_pack" >> workspace/module/enables.txt

# 启动游戏，内核会自动加载
```

### 模块优先级说明

加载顺序（从高到低）：
1. **Plugin** (先加载符号)
2. **Module** (使用 Plugin 符号)
3. **ModLoader** (使用 Module 和 Plugin)

---

## 🔄 热重载

### 工作原理

```
热重载触发 (检测到文件修改 / 手动调用)
       ↓
内核调用所有模块的 hot_reload()
       ↓
模块重新加载配置/资源
       ↓
无需重启游戏即可生效
```

### 实现支持

```c
static void hot_reload(module_entry_t* entry) {
    // 1. 重新加载配置文件
    reload_config(entry->private_dir);
    
    // 2. 重新扫描资源目录
    scan_resources();
    
    // 3. 刷新游戏状态 (如有需要)
    refresh_game_state();
}
```

---

## ⚠️ 注意事项

### 依赖管理

```c
// ✅ 正确：声明依赖
static const char* deps[] = {
    "com.example.texture_api",
    "com.example.file_system"
};
static const module_info_t g_info = {
    .plugin_dependencies_sizes = 2,
    .plugin_dependencies = deps
};

// ❌ 错误：声明不存在的插件依赖
static const char* deps[] = {"non.existent.plugin"};  // 加载会失败

// ✅ 正确：无依赖
static const module_info_t g_info = {
    .plugin_dependencies_sizes = 0,
    .plugin_dependencies = NULL
};
```

### 资源管理

```c
// ✅ 正确：在 cleanup 中释放资源
static bool cleanup_module(module_entry_t* entry) {
    // 释放动态分配的内存
    free(g_state.buffer);
    
    // 关闭文件句柄
    if (g_state.file) fclose(g_state.file);
    
    return true;
}

// ✅ 正确：在 hot_reload 中清理旧状态
static void hot_reload(module_entry_t* entry) {
    // 清理旧数据
    cleanup_old_state();
    // 加载新数据
    load_new_state();
}
```

### 线程安全

| 规则           | 说明                              |
|:---------------|:----------------------------------|
| **主线程操作** | 所有模块操作应在主线程完成        |
| **避免阻塞**   | init/hot_reload 不应耗时过长      |
| **日志使用**   | 使用 entry->logs_dir 进行独立日志 |

---

## 🔗 相关链接

- [TEFPkg-Tool 官方仓库](https://github.com/eternalfuture-e38299/TEFPkg-Tool)
- [PatchLib API 参考](./patchlib.md)
- [TEFPKG 格式文档](./tefpkg.md)
- [插件 (Plugin) 开发指南](./plugin.md)
- [ModLoader 开发指南](./modloader.md)

---

*Happy Module Development! 🚀📦✨*