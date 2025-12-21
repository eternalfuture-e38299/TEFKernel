# memdl 🌍 [English](README-en.md) | [Русский](README-ru.md) - 内存动态加载库 📚

## 基本介绍 🚀

* memdl 是一个跨平台 C 库，支持**直接从内存数据加载共享库**（.so/.dll），无需写入物理文件。提供类似 `dlopen` 的 API，但操作的是内存中的库数据而非文件路径。

## 核心特性 ✨

- **真正内存加载** 🔄 - 某些平台支持纯内存加载，不产生物理文件
- **跨平台支持** 🌍 - Windows、Linux、Android、macOS、iOS
- **自动降级策略** 📉 - 内存加载失败时自动使用临时文件方案
- **格式验证** 🔒 - 自动验证 ELF/Mach-O/PE 文件格式

## 平台支持详情 🏗️

### ✅ 纯内存加载（无文件系统操作）
| 平台 | 技术方案 | 条件 |
|------|----------|------|
| **Linux** | `memfd_create` + `/proc/self/fd` | 内核支持 memfd |
| **Android** | `memfd_create` + `android_dlopen_ext` | API ≥ 24 (Android 7.0+) |

### 🔄 临时文件降级方案
| 平台 | 降级方案 | 说明 |
|------|----------|------|
| **Android (< 7.0)** | 临时文件 + `dlopen` | 兼容旧版本 |
| **Windows** | 临时文件 + `LoadLibrary` | Windows 系统限制 |
| **macOS/iOS** | 临时文件 + `dlopen` | Apple 平台限制 |
| **Linux (无memfd)** | 临时文件 + `dlopen` | 兼容旧内核 |

## 无法内存加载时的处理策略 📉

### 自动降级流程
```c++
// 1. 尝试内存加载（如果平台支持）
if (platform_supports_memfd) {
    handle = try_memfd_load(data, size);
    if (handle) return handle;  // ✅ 成功
}

// 2. 降级到临时文件方案
handle = try_tempfile_load(data, size);  // 🔄 备用方案
if (handle) return handle;

// 3. 所有方案失败
return NULL;  // ❌ 完全失败
```

### 错误处理示例
```c++
memdl_handle_t handle = memdl_open(data, size, MEMDL_NOW);
if (!handle) {
    const char* error = memdl_error();
    
    if (strstr(error, "temp file")) {
        printf("⚠️ 内存加载失败，已使用临时文件方案\n");
    } else if (strstr(error, "All loading methods failed")) {
        printf("❌ 所有加载方案均失败: %s\n", error);
        return -1;
    }
}
```

## API 使用指南 📖

### 基础用法
```c++
// 加载验证
if (memdl_validate(data, size) != 0) {
    printf("❌ 无效的库格式\n");
    return -1;
}

// 平台检测
printf("当前平台: %s\n", 
    memdl_get_platform() == MEMDL_PLATFORM_ANDROID ? "Android" : "Other");

// 架构检测
int arch = memdl_get_arch(data, size);
printf("库架构: %s\n", arch == MEMDL_ARCH_ARM64 ? "ARM64" : "Other");
```

### 完整工作流程
```c++
// 1. 准备库数据
void* lib_data = load_library_to_memory();
size_t lib_size = get_library_size();

// 2. 验证和检测
if (memdl_validate(lib_data, lib_size) != 0) {
    printf("❌ 库格式验证失败\n");
    return -1;
}

// 3. 内存加载（自动选择最佳方案）
memdl_handle_t handle = memdl_open(lib_data, lib_size, MEMDL_NOW | MEMDL_LOCAL);
if (!handle) {
    printf("❌ 加载失败: %s\n", memdl_error());
    return -1;
}

// 4. 使用库函数
void (*func)() = memdl_sym(handle, "my_function");
if (func) func();

// 5. 清理
memdl_close(handle);
free(lib_data);
```

## 平台特定说明 🔧

### Android 注意事项
```c++
// 检查是否使用内存加载
if (memdl_get_platform() == MEMDL_PLATFORM_ANDROID) {
    char sdk_version[32];
    __system_property_get("ro.build.version.sdk", sdk_version);
    if (atoi(sdk_version) >= 24) {
        printf("✅ 使用 Android 7.0+ 内存加载方案\n");
    } else {
        printf("⚠️ 使用临时文件降级方案 (SDK < 24)\n");
    }
}
```

### 临时文件安全
- 文件权限严格限制 (600)
- 加载后立即删除文件内容
- 使用系统安全临时目录

## 最佳实践 💡

1. **始终验证数据** ✅ - 调用 `memdl_validate()` 提前检测
2. **处理降级情况** ⚠️ - 检查错误信息判断加载方式
3. **资源及时释放** 🧹 - 使用后立即调用 `memdl_close()`
4. **错误信息检查** 🔍 - 通过 `memdl_error()` 获取详细错误

## 总结 🎯

* memdl 在支持内存加载的平台上提供最佳性能，在不支持的平台上自动降级确保可用性。开发者无需关心底层实现细节，API 保持一致性。
