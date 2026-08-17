# 🚀 TEFKernel 项目介绍 ([🌍English](README-en.md))

## 📖 概述

**TEFKernel** 是一个跨平台、多架构的游戏模组运行时框架，专为基于 Unity/Mono 和 Unity/IL2CPP 的游戏（尤其是 Terraria ）设计。它提供了从底层内存操作到上层模组管理的完整解决方案，使开发者能够在不修改原始游戏二进制文件的情况下，实现功能扩展、逻辑修改和资源替换。

项目采用 **C/C++** 编写核心引擎，通过 **Java (Android)** 和 **C# (Desktop)** 两层加载器实现平台的完全适配，最终在游戏进程中建立一个强大的"内核态"运行时环境。

## 🌍 平台与架构支持

TEFKernel 在设计之初就考虑了广泛的兼容性，目前已完整支持以下平台和指令集架构：

| 平台           | 支持架构                               | 加载器实现                 | 运行时后端    |
|:---------------|:---------------------------------------|:---------------------------|:--------------|
| **🤖 Android** | ARM64 (arm64-v8a), ARM32 (armeabi-v7a) | Java + ContentProvider     | IL2CPP        |
| **🪟 Windows** | x86_64, x86                            | C# (.NET Framework / .NET) | Mono / DotNet |
| **🐧 Linux**   | x86_64, x86                            | C# (.NET Core / Mono)      | Mono / DotNet |
| **🍎 macOS**   | x86_64, ARM64 (Apple Silicon)          | C# (.NET Core / Mono)      | Mono / DotNet |

> **💡 注意**：在 Windows/Linux/macOS 平台上，TEFKernel 通过一个 C# 加载器（`tefloader`）启动游戏进程，并在游戏入口点之前注入内核库，实现对不同 .NET 运行时的统一支持。🎯


## ⚙️ 核心技术原理

### 🏗️ 双模式加载架构

TEFKernel 采用"平台适配层 + 核心引擎"的分层设计，在不同平台上使用不同的加载策略：

#### 🤖 Android 平台：Java 引导 + JNI 注入

```
[游戏启动] 🚀 → [TefLoaderAppComponentFactory] ⚡ → [Tefloader.initTefKernel()] 🔧
    ↓
通过 ContentProvider 获取内核库文件描述符 📁
    ↓
复制内核库到应用私有目录 → System.load() 📥
    ↓
内核库的 __attribute__((constructor)) 自动执行 ⚡
    ↓
获取 JavaVM → 启动 IL2CPP 监听线程 → Hook il2cpp_init 🎣
```

**关键组件** 🔑：
- `TefLoaderAppComponentFactory`：利用 Android 的 `AppComponentFactory` 机制，在应用组件（Activity/Service/Provider）实例化时触发内核加载，无需修改 AndroidManifest.xml 中的 Application 类。✨
- `Tefloader.java`：通过 ContentProvider 封装的文件操作 API，从外部存储读取内核库文件，并复制到应用私有目录后加载。📂
- `cpcall` + `iohook`：使用 Dobby Hook 拦截 `libc` 的文件操作函数（`open`, `stat`, `mkdir` 等），将所有针对工作目录的 IO 请求重定向到 Java 层的 ContentProvider，实现文件系统虚拟化。🔄

#### 🖥️ 桌面平台：C# 加载器 + 程序集注入

```
[TefLoader.exe] 🚀 → 加载内核库 (libtefkernel.so/dylib/dll) 📦
    ↓
解析并加载原始游戏程序集 (Terraria.exe) 🎮
    ↓
使用 Harmony 在 Terraria.Program.SetupLogging 处注入 Prefix Hook 🪝
    ↓
Hook 触发时调用 init_tefkernel(workdir, isServer) ⚡
    ↓
内核库初始化 PatchLib、ModLoader、Plugin 等子系统 🧩
```

**关键组件** 🔑：
- `Program.cs`：命令行参数解析、内核库加载、游戏程序集加载与入口点调用。⚙️
- `LibLoader.cs`：跨平台动态库加载抽象层，统一了 Windows (`LoadLibraryExW`)、Linux (`dlopen`) 和 macOS (`dlopen`) 的 API 差异，并特别处理了 Wine 环境的兼容性。🔄
- `HookManager.cs`：基于 Harmony 的 Hook 管理器，将 C# 层的 Prefix/Postfix 回调转换为 C 层可调用的函数指针，实现了 C# 与 C 之间的双向 Hook 传递。🌉

### 🔍 多平台运行时抽象 (PatchLib)

`patchlib/` 目录下的 API 是所有游戏交互的基础，它抽象了不同 .NET 运行时的差异：

| 运行时               | 底层实现                           | 关键差异处理                                                                       |
|:---------------------|:-----------------------------------|:-----------------------------------------------------------------------------------|
| **Mono (Desktop)**   | 直接通过 Mono API 操作             | 使用 `mono_class_get_method_from_name` 等标准 API                                  |
| **IL2CPP (Android)** | 通过 IL2CPP 导出的 `il2cpp_*` 函数 | 使用 `il2cpp_class_from_name` 等 API，通过 Dobby 挂钩 `il2cpp_init` 获取初始化时机 |
| **IL2CPP (Desktop)** | 通过 IL2CPP 导出的 `il2cpp_*` 函数 | 与 Android 一致，但通过 C# 加载器在游戏启动时主动初始化                            |

**核心机制** 🧠：
- **类型系统** (`type.h`)：通过 `patchlib_type_get_type(ns, name)` 统一获取类型句柄，底层根据运行时调用 `mono_class_from_name` 或 `il2cpp_class_from_name`。📋
- **方法 Hook** (`method.h`)：
    - Mono 平台：使用 Mono 的 `mono_add_internal_call` 或直接修改虚表。
    - IL2CPP 平台：利用 `il2cpp_method_get_pointer` 获取函数地址，再通过 Dobby 进行 Hook。🎯
    - 桌面 C# 桥接：通过 `HookManager.il2cpp_hook_method` 将 C 层的 Hook 请求转发给 Harmony 处理。🌉

- 热重载线程通过 `inotify` (Linux) 或轮询目录修改时间，监听工作目录的变化。👀
- 当检测到 `.tefpkg` 文件更新时，自动调用对应模块的 `hot_reload` 函数，实现代码和资源的动态更新。✨

### 📦 内存动态库加载 (memdl)

`memdl` 模块实现了一个**从内存加载动态库**的机制，无需将库文件写入磁盘：💾

```c
// 从 TEF 包中提取动态库数据，直接从内存加载 🚀
void* dylib_handle = memdl_open(dylib, dylib_size, MEMDL_LAZY);
```

**原理** 🔧：
- 在 Linux/Android 上，通过解析 ELF 文件格式，手动将代码段、数据段映射到内存，并处理重定位和符号解析。📄
- 在 Windows 上，模拟 `LoadLibrary` 的行为，从内存中的 PE 文件构建模块。🪟
- 这使得插件和模块可以完全打包在 TEF 文件中，分发时只需一个 `.tefpkg` 文件，无需额外安装动态库。🎁

### 📁 TEF Package 格式

`.tefpkg` 是一种自描述的打包格式，集成了以下特性：✨

| 特性              | 实现方式                                     |
|:------------------|:---------------------------------------------|
| **🗜️ 压缩**       | 支持 LZ4 / LZ4HC，每个条目独立压缩           |
| **✅ 完整性校验** | 头部校验和 + 内容哈希 (CRC64)                |
| **🔐 签名验证**   | 支持包签名，防止篡改                         |
| **📊 预留条目**   | 创建时可预留条目空间，减少后续追加时的重分配 |
| **🔍 条目索引**   | 支持通过索引快速访问文件                     |

```c
typedef struct __attribute__((packed)) {
    uint32_t magic;          // "TEFP" ✨
    uint16_t version;        // 版本号 📌
    uint16_t file_count;     // 当前文件总数 📊
    uint16_t reserved_entries; // 预留条目数 📋
    uint32_t data_offset;    // 数据区偏移 📍
    uint64_t checksum;       // 头部校验和 ✅
    uint64_t content_hash;   // 内容完整性哈希 🔒
    uint64_t signature;      // 包签名 🛡️
} tefpkg_header_t;
```

### 🛡️ 崩溃处理与日志系统

- **崩溃处理器** (`crash_handler.c`) 🚨：
    - Windows：使用 `SetUnhandledExceptionFilter` 捕获结构化异常。🪟
    - Linux/macOS/Android：使用 `sigaction` 捕获 `SIGSEGV`, `SIGABRT`, `SIGFPE` 等信号。🐧🍎🤖
    - 崩溃时自动打印调用栈（通过 `backtrace` 或 `_Unwind_Backtrace`），并将日志刷新到磁盘。📝

- **异步日志系统** (`log.c`) 📋：
    - 采用生产者-消费者模型，日志消息先入队列，由独立线程异步写入文件和控制台。🔄
    - 支持日志级别过滤（TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL）。🔍
    - Android 平台同时输出到 `logcat` 和文件。📱

---

## 💎 总结

TEFKernel 不仅仅是一个模组加载器，它构建了一个完整的**游戏运行时扩展框架**：🏗️

- **🔧 底层**通过 PatchLib 和 memdl 实现了对游戏运行时的深度控制；
- **🏗️ 中层**通过 Plugin/Module/ModLoader 三层架构实现了灵活的功能组织；
- **🚀 上层**通过跨平台的 Java/C# 加载器实现了无缝的部署体验。

无论是想在 Android 手机上修改游戏逻辑，还是在 Windows 上开发复杂的 Mod，TEFKernel 都提供了强大且统一的工具链支持。🌟 加入我们，一起探索游戏模组的无限可能！🎉

---

## 📜 许可证

TEFKernel 项目包含两种许可证：
- **MIT 许可证**：api头文件接口 (`tef_api.h`, `patchlib/`, `tefstd/`, `memdl/` 等)
- **AGPL-3.0 许可证**：可执行和框架组件 (`tefpkg.h`, `tpf_core.h`, `module_core.h`, `modloader_core.h`, 以及除`tef_api.c`外的所有 `.c` 实现文件)

请根据您的使用场景遵守相应的许可证规定。📋

## 📚 第三方库依赖

TEFKernel 项目使用了以下优秀的开源库来实现其核心功能：

### 核心依赖

| 库名称                                         | 用途                                                                                                               | 许可证       |
|:-----------------------------------------------|:-------------------------------------------------------------------------------------------------------------------|:-------------|
| **[LZ4](https://github.com/lz4/lz4)**          | 极速无损压缩算法，用于 TEF 包中条目的实时压缩/解压                                                                 | BSD-2-Clause |
| **[Dobby](https://github.com/jmpews/Dobby)**   | 跨平台 Hook 框架，用于拦截和修改函数调用（IL2CPP/Android 平台核心）                                                | MIT          |
| **[libffi](https://github.com/libffi/libffi)** | 外部函数接口库，用于实现动态调用和类型转换（通过 [zuri-lang/ffi](https://github.com/zuri-lang/ffi) 的 CMake 绑定） | MIT          |
| **[xDL](https://github.com/hexhacking/xDL)**   | Android 平台的增强版 `dlopen`/`dlsym`，用于可靠地定位和加载 `libil2cpp.so`                                         | MIT          |

### 桌面端额外依赖

| 库名称                                              | 用途                                                        | 许可证 |
|:----------------------------------------------------|:------------------------------------------------------------|:-------|
| **[HarmonyX](https://github.com/BepInEx/HarmonyX)** | .NET 运行时的方法 Hook 库，用于桌面端 C# 加载器的运行时补丁 | MIT    |

> **💡 说明**：
> - 所有第三方库均在其原始许可证下使用，TEFKernel 不会修改它们的许可证条款。
> - 部分库（如 libffi）通过 CMake 构建系统自动集成，无需手动下载。
> - HarmonyX 仅在桌面端（Windows/Linux/macOS）的 C# 加载器中使用，Android 平台不依赖该库。
> - Dobby 仅在移动端集成，桌面端不依赖该库

### 开发文档
#### - **[patchlib](documents/patchlib.md)**
#### - **[memdl](https://github.com/eternalfuture-e38299/memdl)**
#### - **[tefstd](documents/tefstd.md)**
#### - **[tefpkg](documents/tefpkg.md)**
#### - **[plugin](documents/plugin.md)**
#### - **[module](documents/module.md)**
#### - **[modloader](documents/modloader.md)**
* 别忘了将`tef_api_imp.c`添加到编译文件中
* 如果你想学习Mod开发，你应该去看对应ModLoader的开发文档，TEFKernel并没有对Mod进行标准统一，我们认为Mod定义应该留给ModLoader

欢迎通过 GitHub Issues 和 Pull Requests 为本项目做出贡献！🤝✨