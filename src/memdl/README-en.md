# memdl - Memory Dynamic Loading Library 📚

## Introduction 🚀

* memdl is a cross-platform C library that supports **loading shared libraries (.so/.dll) directly from memory data** without writing to physical files. It provides a `dlopen`-like API but operates on library data in memory rather than file paths.

## Core Features ✨

- **True Memory Loading** 🔄 - Some platforms support pure memory loading without creating physical files
- **Cross-Platform Support** 🌍 - Windows, Linux, Android, macOS, iOS
- **Automatic Fallback Strategy** 📉 - Automatically uses temporary file approach when memory loading fails
- **Format Validation** 🔒 - Automatically validates ELF/Mach-O/PE file formats

## Platform Support Details 🏗️

### ✅ Pure Memory Loading (No Filesystem Operations)
| Platform | Technical Solution | Conditions |
|----------|-------------------|------------|
| **Linux** | `memfd_create` + `/proc/self/fd` | Kernel supports memfd |
| **Android** | `memfd_create` + `android_dlopen_ext` | API ≥ 24 (Android 7.0+) |

### 🔄 Temporary File Fallback Solution
| Platform | Fallback Solution | Description |
|----------|------------------|-------------|
| **Android (< 7.0)** | Temporary file + `dlopen` | Backward compatibility |
| **Windows** | Temporary file + `LoadLibrary` | Windows system limitations |
| **macOS/iOS** | Temporary file + `dlopen` | Apple platform restrictions |
| **Linux (no memfd)** | Temporary file + `dlopen` | Legacy kernel compatibility |

## Handling Strategy When Memory Loading Fails 📉

### Automatic Fallback Process
```c++
// 1. Try memory loading (if platform supports)
if (platform_supports_memfd) {
    handle = try_memfd_load(data, size);
    if (handle) return handle;  // ✅ Success
}

// 2. Fallback to temporary file solution
handle = try_tempfile_load(data, size);  // 🔄 Backup solution
if (handle) return handle;

// 3. All solutions failed
return NULL;  // ❌ Complete failure
```

### Error Handling Example
```c++
memdl_handle_t handle = memdl_open(data, size, MEMDL_NOW);
if (!handle) {
    const char* error = memdl_error();
    
    if (strstr(error, "temp file")) {
        printf("⚠️ Memory loading failed, using temporary file solution\n");
    } else if (strstr(error, "All loading methods failed")) {
        printf("❌ All loading solutions failed: %s\n", error);
        return -1;
    }
}
```

## API Usage Guide 📖

### Basic Usage
```c++
// Load validation
if (memdl_validate(data, size) != 0) {
    printf("❌ Invalid library format\n");
    return -1;
}

// Platform detection
printf("Current platform: %s\n", 
    memdl_get_platform() == MEMDL_PLATFORM_ANDROID ? "Android" : "Other");

// Architecture detection
int arch = memdl_get_arch(data, size);
printf("Library architecture: %s\n", arch == MEMDL_ARCH_ARM64 ? "ARM64" : "Other");
```

### Complete Workflow
```c++
// 1. Prepare library data
void* lib_data = load_library_to_memory();
size_t lib_size = get_library_size();

// 2. Validation and detection
if (memdl_validate(lib_data, lib_size) != 0) {
    printf("❌ Library format validation failed\n");
    return -1;
}

// 3. Memory loading (automatically selects best solution)
memdl_handle_t handle = memdl_open(lib_data, lib_size, MEMDL_NOW | MEMDL_LOCAL);
if (!handle) {
    printf("❌ Loading failed: %s\n", memdl_error());
    return -1;
}

// 4. Use library functions
void (*func)() = memdl_sym(handle, "my_function");
if (func) func();

// 5. Cleanup
memdl_close(handle);
free(lib_data);
```

## Platform-Specific Notes 🔧

### Android Considerations
```c++
// Check if using memory loading
if (memdl_get_platform() == MEMDL_PLATFORM_ANDROID) {
    char sdk_version[32];
    __system_property_get("ro.build.version.sdk", sdk_version);
    if (atoi(sdk_version) >= 24) {
        printf("✅ Using Android 7.0+ memory loading solution\n");
    } else {
        printf("⚠️ Using temporary file fallback solution (SDK < 24)\n");
    }
}
```

### Temporary File Security
- Strict file permission restrictions (600)
- Immediate file content deletion after loading
- Uses system secure temporary directories

## Best Practices 💡

1. **Always Validate Data** ✅ - Call `memdl_validate()` for pre-detection
2. **Handle Fallback Scenarios** ⚠️ - Check error messages to determine loading method
3. **Timely Resource Release** 🧹 - Call `memdl_close()` immediately after use
4. **Error Message Checking** 🔍 - Get detailed errors through `memdl_error()`

## Summary 🎯

* memdl provides optimal performance on platforms that support memory loading, while automatically falling back to ensure availability on unsupported platforms. Developers don't need to worry about underlying implementation details, as the API remains consistent.