# 🚀 TEFKernel Project Introduction

## 📖 Overview

**TEFKernel** is a cross-platform, multi-architecture game modding runtime framework designed for Unity/Mono and Unity/IL2CPP based games (especially Terraria). It provides a complete solution from low-level memory operations to upper-level mod management, enabling developers to extend functionality, modify logic, and replace resources without modifying the original game binaries.

The project uses **C/C++** for the core engine, with **Java (Android)** and **C# (Desktop)** loaders to achieve full platform adaptation, ultimately establishing a powerful "kernel-mode" runtime environment within the game process.

## 🌍 Platform and Architecture Support

TEFKernel is designed with broad compatibility in mind and currently fully supports the following platforms and instruction set architectures:

| Platform       | Supported Architectures                | Loader Implementation      | Runtime Backend |
|:---------------|:---------------------------------------|:---------------------------|:----------------|
| **🤖 Android** | ARM64 (arm64-v8a), ARM32 (armeabi-v7a) | Java + ContentProvider     | IL2CPP          |
| **🪟 Windows** | x86_64, x86                            | C# (.NET Framework / .NET) | Mono / DotNet   |
| **🐧 Linux**   | x86_64, x86                            | C# (.NET Core / Mono)      | Mono / DotNet   |
| **🍎 macOS**   | x86_64, ARM64 (Apple Silicon)          | C# (.NET Core / Mono)      | Mono / DotNet   |

> **💡 Note**: On Windows/Linux/macOS platforms, TEFKernel uses a C# loader (`tefloader`) to launch the game process and injects the kernel library before the game entry point, achieving unified support for different .NET runtimes. 🎯

## ⚙️ Core Technical Principles

### 🏗️ Dual-Mode Loading Architecture

TEFKernel adopts a "platform adaptation layer + core engine" layered design, using different loading strategies on different platforms:

#### 🤖 Android Platform: Java Bootstrapping + JNI Injection

```
[Game Launch] 🚀 → [TefLoaderAppComponentFactory] ⚡ → [Tefloader.initTefKernel()] 🔧
↓
Obtain kernel library file descriptor via ContentProvider 📁
↓
Copy kernel library to app private directory → System.load() 📥
↓
Kernel library's __attribute__((constructor)) executes automatically ⚡
↓
Get JavaVM → Start IL2CPP monitoring thread → Hook il2cpp_init 🎣
```

**Key Components** 🔑:
- `TefLoaderAppComponentFactory`: Utilizes Android's `AppComponentFactory` mechanism to trigger kernel loading when application components (Activity/Service/Provider) are instantiated, eliminating the need to modify the Application class in AndroidManifest.xml. ✨
- `Tefloader.java`: Reads kernel library files from external storage via ContentProvider-wrapped file operations API, copies them to the app's private directory, and loads them. 📂
- `cpcall` + `iohook`: Uses Dobby Hook to intercept `libc` file operations functions (`open`, `stat`, `mkdir`, etc.), redirecting all IO requests targeting the working directory to Java-layer ContentProvider, achieving filesystem virtualization. 🔄

#### 🖥️ Desktop Platform: C# Loader + Assembly Injection

```
[TefLoader.exe] 🚀 → Load kernel library (libtefkernel.so/dylib/dll) 📦
↓
Parse and load original game assembly (Terraria.exe) 🎮
↓
Use Harmony to inject Prefix Hook at Terraria.Program.SetupLogging 🪝
↓
When Hook triggers, call init_tefkernel(workdir, isServer) ⚡
↓
Kernel library initializes PatchLib, ModLoader, Plugin and other subsystems 🧩
```

**Key Components** 🔑:
- `Program.cs`: Command-line argument parsing, kernel library loading, game assembly loading and entry point invocation. ⚙️
- `LibLoader.cs`: Cross-platform dynamic library loading abstraction layer, unifying Windows (`LoadLibraryExW`), Linux (`dlopen`), and macOS (`dlopen`) API differences, with special handling for Wine environment compatibility. 🔄
- `HookManager.cs`: Harmony-based Hook manager, converting C#-layer Prefix/Postfix callbacks to C-callable function pointers, enabling bidirectional Hook transfer between C# and C. 🌉

### 🔍 Multi-Platform Runtime Abstraction (PatchLib)

The APIs under `patchlib/` are the foundation for all game interactions, abstracting differences across different .NET runtimes:

| Runtime              | Underlying Implementation                | Key Difference Handling                                                           |
|:---------------------|:-----------------------------------------|:----------------------------------------------------------------------------------|
| **Mono (Desktop)**   | Direct manipulation via Mono API         | Uses standard Mono APIs like `mono_class_get_method_from_name`                    |
| **IL2CPP (Android)** | Via IL2CPP exported `il2cpp_*` functions | Uses APIs like `il2cpp_class_from_name`, hooks `il2cpp_init` via Dobby for timing |
| **IL2CPP (Desktop)** | Via IL2CPP exported `il2cpp_*` functions | Same as Android, but actively initialized via C# loader at game startup           |

**Core Mechanisms** 🧠:
- **Type System** (`type.h`): Unified type handle retrieval via `patchlib_type_get_type(ns, name)`, underlying call to `mono_class_from_name` or `il2cpp_class_from_name` based on runtime. 📋
- **Method Hook** (`method.h`):
  - Mono platform: Uses Mono's `mono_add_internal_call` or direct vtable modification.
  - IL2CPP platform: Gets function address via `il2cpp_method_get_pointer`, then Hooks via Dobby. 🎯
  - Desktop C# bridge: Forwards C-layer Hook requests to Harmony via `HookManager.il2cpp_hook_method`. 🌉

- The hot-reload thread uses `inotify` (Linux) or directory modification time polling to monitor changes in the working directory. 👀
- When a `.tefpkg` file update is detected, it automatically calls the corresponding module's `hot_reload` function, enabling dynamic code and resource updates. ✨

### 📦 In-Memory Dynamic Library Loading (memdl)

The `memdl` module implements a mechanism to **load dynamic libraries from memory**, eliminating the need to write library files to disk: 💾

```c
// Extract dynamic library data from TEF package, load directly from memory 🚀
void* dylib_handle = memdl_open(dylib, dylib_size, MEMDL_LAZY);
```

**Principle** 🔧:
- On Linux/Android, parses ELF file format, manually maps code and data segments to memory, and handles relocations and symbol resolution. 📄
- On Windows, simulates `LoadLibrary` behavior, constructing a module from an in-memory PE file. 🪟
- This allows plugins and modules to be fully packaged within TEF files, requiring only a single `.tefpkg` file for distribution, without additional dynamic library installation. 🎁

### 📁 TEF Package Format

`.tefpkg` is a self-describing packaging format that integrates the following features: ✨

| Feature                       | Implementation                                                            |
|:------------------------------|:--------------------------------------------------------------------------|
| **🗜️ Compression**            | Supports LZ4 / LZ4HC, each entry independently compressed                 |
| **✅ Integrity Check**        | Header checksum + content hash (CRC64)                                    |
| **🔐 Signature Verification** | Supports package signing to prevent tampering                             |
| **📊 Reserved Entries**       | Can reserve entry slots at creation to reduce reallocation when appending |
| **🔍 Entry Indexing**         | Supports quick file access via index                                      |

```c
typedef struct __attribute__((packed)) {
    uint32_t magic;          // "TEFP" ✨
    uint16_t version;        // Version number 📌
    uint16_t file_count;     // Current total file count 📊
    uint16_t reserved_entries; // Reserved entry count 📋
    uint32_t data_offset;    // Data section offset 📍
    uint64_t checksum;       // Header checksum ✅
    uint64_t content_hash;   // Content integrity hash 🔒
    uint64_t signature;      // Package signature 🛡️
} tefpkg_header_t;
```

### 🛡️ Crash Handling and Logging System

- **Crash Handler** (`crash_handler.c`) 🚨:
  - Windows: Uses `SetUnhandledExceptionFilter` to catch structured exceptions. 🪟
  - Linux/macOS/Android: Uses `sigaction` to catch signals like `SIGSEGV`, `SIGABRT`, `SIGFPE`. 🐧🍎🤖
  - Automatically prints call stack on crash (via `backtrace` or `_Unwind_Backtrace`) and flushes logs to disk. 📝

- **Asynchronous Logging System** (`log.c`) 📋:
  - Producer-consumer model: Log messages are queued and asynchronously written to file and console by a dedicated thread. 🔄
  - Supports log level filtering (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL). 🔍
  - On Android, outputs to both `logcat` and file. 📱

---

## 💎 Summary

TEFKernel is more than just a mod loader; it builds a complete **game runtime extension framework**: 🏗️

- **🔧 At the bottom layer**, it achieves deep control over the game runtime via PatchLib and memdl.
- **🏗️ In the middle layer**, it implements flexible functional organization through the Plugin/Module/ModLoader three-tier architecture.
- **🚀 At the upper layer**, it provides a seamless deployment experience through cross-platform Java/C# loaders.

Whether you want to modify game logic on Android phones or develop complex Mods on Windows, TEFKernel provides a powerful and unified toolchain. 🌟 Join us and explore the infinite possibilities of game modding! 🎉

---

## 📜 License

The TEFKernel project includes two types of licenses:
- **MIT License**: API header files interfaces (`tef_api.h`, `patchlib/`, `tefstd/`, `memdl/`, etc.)
- **AGPL-3.0 License**: Executable and framework components (`tefpkg.h`, `tpf_core.h`, `module_core.h`, `modloader_core.h`, and all `.c` implementation files except `tef_api.c`)

Please comply with the corresponding license terms according to your usage scenario. 📋

## 📚 Third-Party Library Dependencies

TEFKernel uses the following excellent open-source libraries to implement its core functionality:

### Core Dependencies

| Library Name                                   | Purpose                                                                                                                                              | License      |
|:-----------------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------|:-------------|
| **[LZ4](https://github.com/lz4/lz4)**          | Ultra-fast lossless compression algorithm, used for real-time compression/decompression of TEF package entries                                       | BSD-2-Clause |
| **[Dobby](https://github.com/jmpews/Dobby)**   | Cross-platform Hook framework for intercepting and modifying function calls (core for IL2CPP/Android platform)                                       | MIT          |
| **[libffi](https://github.com/libffi/libffi)** | Foreign function interface library for dynamic invocation and type conversion (via [zuri-lang/ffi](https://github.com/zuri-lang/ffi) CMake bindings) | MIT          |
| **[xDL](https://github.com/hexhacking/xDL)**   | Android platform enhanced `dlopen`/`dlsym` for reliably locating and loading `libil2cpp.so`                                                          | MIT          |

### Desktop-Specific Dependencies

| Library Name                                        | Purpose                                                                    | License |
|:----------------------------------------------------|:---------------------------------------------------------------------------|:--------|
| **[HarmonyX](https://github.com/BepInEx/HarmonyX)** | .NET runtime method hooking library for desktop C# loader runtime patching | MIT     |

> **💡 Notes**:
> - All third-party libraries are used under their original licenses; TEFKernel does not modify their license terms.
> - Some libraries (like libffi) are automatically integrated via the CMake build system, no manual download required.
> - HarmonyX is used only in the desktop (Windows/Linux/macOS) C# loader and is not required for the Android platform.
> - Dobby is integrated only on mobile platforms and is not used on desktop.

### Development Documentation
#### - **[patchlib](documents/patchlib-en.md)**
#### - **[memdl](https://github.com/eternalfuture-e38299/memdl)**
#### - **[tefstd](documents/tefstd-en.md)**
#### - **[tefpkg](documents/tefpkg-en.md)**
#### - **[plugin](documents/plugin-en.md)**
#### - **[module](documents/module-en.md)**
#### - **[modloader](documents/modloader-en.md)**
* Remember to add `tef_api_imp.c` to your compiled files.
* If you want to learn Mod development, you should refer to the corresponding ModLoader development documentation. TEFKernel does not standardize Mods; we believe Mod definitions should be left to ModLoader.

Welcome to contribute to this project via GitHub Issues and Pull Requests! 🤝✨