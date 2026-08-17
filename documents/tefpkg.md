# 📦 TEFPKG 包格式使用指南

TEFPKG 是 TEFKernel 的自描述打包格式，用于分发和存储代码、资源等文件。它支持压缩、校验、签名和预留条目等特性，是 TEFKernel 生态中 Plugin、Module、ModLoader 和 Mod 的基础载体。

- **[打包工具](https://github.com/eternalfuture-e38299/TEFPkg-Tool)**

---

## 📑 目录

<details>
<summary><b>📖 点击展开完整目录</b></summary>

- [📖 概述](#-概述)
  - [特性](#特性)
  - [错误码](#错误码)
- [🔧 生命周期管理](#-生命周期管理)
  - [创建包](#创建包)
  - [打开包](#打开包)
  - [保存包](#保存包)
  - [关闭包](#关闭包)
- [📂 条目操作](#-条目操作)
  - [添加条目](#添加条目)
  - [提取条目](#提取条目)
  - [获取条目信息](#获取条目信息)
- [✅ 验证与签名](#-验证与签名)
  - [完整性验证](#完整性验证)
  - [签名验证](#签名验证)
  - [签名包](#签名包)
- [💡 完整示例](#-完整示例)
  - [创建并保存包](#创建并保存包)
  - [打开并提取包](#打开并提取包)
  - [打包动态库](#打包动态库)
- [⚠️ 注意事项](#-注意事项)

</details>

---

## 📖 概述

### 特性

| 特性              | 说明                                         |
|:------------------|:---------------------------------------------|
| **🗜️ 压缩**       | 支持 LZ4 / LZ4HC 压缩算法，每个条目独立压缩  |
| **✅ 完整性校验** | 头部校验和 + 内容哈希 (CRC64)                |
| **🔐 签名验证**   | 支持包签名，防止篡改                         |
| **📊 预留条目**   | 创建时可预留条目空间，减少后续追加时的重分配 |
| **🔍 快速索引**   | 通过索引快速访问文件                         |
| **💾 多模式访问** | 支持内存模式、只读模式、读写模式             |

### 错误码

| 错误码                    | 值  | 说明                           |
|:--------------------------|:----|:-------------------------------|
| `TEF_OK`                  | 0   | 操作成功 ✅                    |
| `TEF_ERROR`               | -1  | 一般性错误                     |
| `TEF_ERROR_SIGNATURE`     | -2  | 签名验证失败                   |
| `TEF_ERROR_CORRUPT`       | -3  | 数据损坏或格式错误             |
| `TEF_ERROR_MEMORY`        | -4  | 内存分配失败                   |
| `TEF_ERROR_IO`            | -5  | 输入输出错误                   |
| `TEF_ERROR_KEYFILE`       | -6  | 密钥文件错误                   |
| `TEF_ERROR_NOT_FOUND`     | -7  | 文件或资源未找到               |
| `TEF_ERROR_INVALID`       | -8  | 参数无效或状态不正确           |
| `TEF_ERROR_NOT_SIGNATURE` | -9  | 包未签名                       |
| `TEF_ERROR_INTEGRITY`     | -10 | 完整性校验不通过               |
| `TEF_ERROR_NO_SPACE`      | -11 | 没有更多空间（预留条目已用完） |

---

## 🔧 生命周期管理

### 创建包

#### 从文件创建（预留空间）

```c
tefpkg_result_t tefpkg_create_reserved_from_file(
    const char *filename,
    uint16_t reserved_entries,
    tefpkg_t **pkg
);
```

**示例：**

```c
#include "tefpackage/tefpkg.h"

tefpkg_t* pkg = NULL;
tefpkg_result_t result = tefpkg_create_reserved_from_file(
    "my_package.tefpkg",
    10,   // 预留 10 个条目
    &pkg
);

if (result == TEF_OK) {
    printf("Package created successfully!\n");
} else {
    printf("Failed to create package: %d\n", result);
}
```

#### 从内存创建（预留空间）

```c
tefpkg_result_t tefpkg_create_reserved_from_memory(
    uint16_t reserved_entries,
    tefpkg_t **pkg
);
```

**示例：**

```c
tefpkg_t* pkg = NULL;
tefpkg_result_t result = tefpkg_create_reserved_from_memory(10, &pkg);

if (result == TEF_OK) {
    printf("In-memory package created!\n");
}
```

### 打开包

#### 只读方式打开

```c
tefpkg_result_t tefpkg_open_readonly(
    const char *filename,
    tefpkg_t **pkg
);
```

**示例：**

```c
tefpkg_t* pkg = NULL;
tefpkg_result_t result = tefpkg_open_readonly("existing_package.tefpkg", &pkg);

if (result == TEF_OK) {
    printf("Package opened successfully!\n");
    printf("Contains %d files\n", tefpkg_get_entries_count(pkg));
}
```

#### 从内存数据打开

```c
tefpkg_result_t tefpkg_open_from_memory(
    const uint8_t *data,
    uint32_t data_size,
    tefpkg_t **pkg
);
```

**示例：**

```c
// 假设从某处读取了包数据到内存
uint8_t* pkg_data = ...;
uint32_t pkg_size = ...;

tefpkg_t* pkg = NULL;
tefpkg_result_t result = tefpkg_open_from_memory(pkg_data, pkg_size, &pkg);

if (result == TEF_OK) {
    printf("Package opened from memory!\n");
}
```

### 保存包

#### 保存到文件

```c
tefpkg_result_t tefpkg_save_file(
    tefpkg_t *pkg,
    uint64_t fingerprint
);
```

**示例：**

```c
tefpkg_t* pkg = ...; // 已创建的包
uint64_t fingerprint = 0x1234567890ABCDEF;

tefpkg_result_t result = tefpkg_save_file(pkg, fingerprint);

if (result == TEF_OK) {
    printf("Package saved successfully!\n");
}
```

#### 从内存保存到文件

```c
tefpkg_result_t tefpkg_save_memory_file(
    const char *filename,
    tefpkg_t *pkg,
    uint64_t fingerprint
);
```

**示例：**

```c
tefpkg_t* pkg = ...;
tefpkg_result_t result = tefpkg_save_memory_file(
    "output.tefpkg",
    pkg,
    0x1234567890ABCDEF
);
```

### 关闭包

```c
void tefpkg_close(tefpkg_t* pkg);
```

**示例：**

```c
tefpkg_t* pkg = ...;
tefpkg_close(pkg);
pkg = NULL;
```

---

## 📂 条目操作

### 添加条目

#### 从内存添加

```c
tefpkg_result_t tefpkg_add_entry_from_memory(
    tefpkg_t *pkg,
    tefpkg_compress_t compress_type,
    uint8_t compress_level,
    uint8_t *data,
    uint32_t data_size
);
```

**参数说明：**

| 参数             | 说明                                              |
|:-----------------|:--------------------------------------------------|
| `compress_type`  | `COMPRESS_NONE`, `COMPRESS_LZ4`, `COMPRESS_LZ4HC` |
| `compress_level` | 压缩等级 (0-9)，仅对 LZ4HC 有效                   |

**示例：**

```c
tefpkg_t* pkg = ...;
const char* text = "Hello, TEF Package!";
uint8_t* data = (uint8_t*)text;
uint32_t data_size = strlen(text) + 1;

tefpkg_result_t result = tefpkg_add_entry_from_memory(
    pkg,
    COMPRESS_LZ4,    // 使用 LZ4 压缩
    0,               // 默认压缩等级
    data,
    data_size
);

if (result == TEF_OK) {
    printf("Entry added successfully!\n");
}
```

#### 从文件添加

```c
tefpkg_result_t tefpkg_add_entry_from_file(
    tefpkg_t *pkg,
    const char *filepath,
    tefpkg_compress_t compress_type,
    uint8_t compress_level
);
```

**示例：**

```c
tefpkg_t* pkg = ...;

tefpkg_result_t result = tefpkg_add_entry_from_file(
    pkg,
    "image.png",
    COMPRESS_LZ4HC,  // 高压缩比
    9                // 最高压缩等级
);
```

### 提取条目

#### 提取到内存

```c
tefpkg_result_t tefpkg_extract_entry_to_memory(
    const tefpkg_t *pkg,
    uint32_t entry_index,
    uint8_t **data,
    uint32_t *data_size
);
```

**示例：**

```c
tefpkg_t* pkg = ...;
uint8_t* extracted_data = NULL;
uint32_t extracted_size = 0;

tefpkg_result_t result = tefpkg_extract_entry_to_memory(
    pkg,
    0,   // 提取第一个条目
    &extracted_data,
    &extracted_size
);

if (result == TEF_OK) {
    printf("Extracted %u bytes\n", extracted_size);
    // 使用数据...
    free(extracted_data);  // 使用完后释放
}
```

#### 提取到文件

```c
tefpkg_result_t tefpkg_extract_entry_to_file(
    const tefpkg_t *pkg,
    uint32_t entry_index,
    const char *output_path
);
```

**示例：**

```c
tefpkg_t* pkg = ...;

tefpkg_result_t result = tefpkg_extract_entry_to_file(
    pkg,
    1,                     // 第二个条目
    "extracted_file.bin"   // 输出路径
);
```

### 获取条目信息

```c
tefpkg_result_t tefpkg_get_entry_info(
    const tefpkg_t *pkg,
    uint32_t entry_index,
    tefpkg_entry_t **info
);
```

**示例：**

```c
tefpkg_t* pkg = ...;
tefpkg_entry_t* info = NULL;

tefpkg_result_t result = tefpkg_get_entry_info(pkg, 0, &info);

if (result == TEF_OK && info) {
    printf("Entry Info:\n");
    printf("  Index: %u\n", info->index);
    printf("  Offset: %u\n", info->data_offset);
    printf("  Original Size: %u\n", info->original_size);
    printf("  Compressed Size: %u\n", info->compressed_size);
    printf("  Compress Type: %u\n", info->compress_type);
}
```

### 辅助函数

```c
// 获取条目数量
uint16_t count = tefpkg_get_entries_count(pkg);

// 获取预留条目数量
uint16_t reserved = tefpkg_get_reserved_entries(pkg);
```

---

## ✅ 验证与签名

### 完整性验证

#### 验证单个条目

```c
tefpkg_result_t tefpkg_verify_entry(
    const tefpkg_t *pkg,
    uint32_t entry_index
);
```

**示例：**

```c
tefpkg_t* pkg = ...;

tefpkg_result_t result = tefpkg_verify_entry(pkg, 0);

if (result == TEF_OK) {
    printf("Entry 0 is intact!\n");
} else {
    printf("Entry 0 is corrupted!\n");
}
```

#### 验证整个包

```c
tefpkg_result_t tefpkg_verify_pkg(const tefpkg_t *pkg);
```

**示例：**

```c
tefpkg_t* pkg = ...;

tefpkg_result_t result = tefpkg_verify_pkg(pkg);

if (result == TEF_OK) {
    printf("Package integrity verified!\n");
}
```

### 签名验证

```c
tefpkg_result_t tefpkg_verify_signature(
    const tefpkg_t *pkg,
    uint64_t fingerprint
);
```

**示例：**

```c
tefpkg_t* pkg = ...;
uint64_t expected_fingerprint = 0x1234567890ABCDEF;

tefpkg_result_t result = tefpkg_verify_signature(pkg, expected_fingerprint);

if (result == TEF_OK) {
    printf("Signature verified!\n");
} else if (result == TEF_ERROR_NOT_SIGNATURE) {
    printf("Package is not signed!\n");
} else {
    printf("Signature verification failed!\n");
}
```

### 签名包

```c
tefpkg_result_t tefpkg_sign_package(
    tefpkg_t *pkg,
    uint64_t fingerprint
);
```

**示例：**

```c
tefpkg_t* pkg = ...;
uint64_t my_fingerprint = 0x1234567890ABCDEF;

tefpkg_result_t result = tefpkg_sign_package(pkg, my_fingerprint);

if (result == TEF_OK) {
    printf("Package signed successfully!\n");
}
```

---

## 💡 完整示例

### 创建并保存包

```c
#include <stdio.h>
#include <string.h>
#include "tefpackage/tefpkg.h"

int main() {
    tefpkg_t* pkg = NULL;
    
    // 1. 创建包（预留 5 个条目）
    printf("Creating package...\n");
    tefpkg_result_t result = tefpkg_create_reserved_from_memory(5, &pkg);
    if (result != TEF_OK) {
        printf("Failed to create package: %d\n", result);
        return 1;
    }
    
    // 2. 添加条目
    printf("Adding entries...\n");
    
    // 添加文本文件
    const char* text = "Hello, this is a TEF package!";
    result = tefpkg_add_entry_from_memory(
        pkg, COMPRESS_LZ4, 0,
        (uint8_t*)text, strlen(text) + 1
    );
    printf("  Added text entry: %s\n", result == TEF_OK ? "OK" : "FAIL");
    
    // 添加二进制数据
    uint8_t binary_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    result = tefpkg_add_entry_from_memory(
        pkg, COMPRESS_NONE, 0,
        binary_data, sizeof(binary_data)
    );
    printf("  Added binary entry: %s\n", result == TEF_OK ? "OK" : "FAIL");
    
    // 3. 签名包
    uint64_t fingerprint = 0x1234567890ABCDEF;
    result = tefpkg_sign_package(pkg, fingerprint);
    printf("Signing package: %s\n", result == TEF_OK ? "OK" : "FAIL");
    
    // 4. 保存到文件
    result = tefpkg_save_file(pkg, fingerprint);
    printf("Saving package: %s\n", result == TEF_OK ? "OK" : "FAIL");
    
    // 5. 关闭包
    tefpkg_close(pkg);
    
    printf("Done!\n");
    return 0;
}
```

### 打开并提取包

```c
#include <stdio.h>
#include "tefpackage/tefpkg.h"

int main() {
    tefpkg_t* pkg = NULL;
    
    // 1. 打开包
    printf("Opening package...\n");
    tefpkg_result_t result = tefpkg_open_readonly("my_package.tefpkg", &pkg);
    if (result != TEF_OK) {
        printf("Failed to open package: %d\n", result);
        return 1;
    }
    
    // 2. 获取基本信息
    uint16_t count = tefpkg_get_entries_count(pkg);
    uint16_t reserved = tefpkg_get_reserved_entries(pkg);
    printf("Entries: %d, Reserved: %d\n", count, reserved);
    
    // 3. 验证包完整性
    result = tefpkg_verify_pkg(pkg);
    if (result == TEF_OK) {
        printf("Package integrity: OK\n");
    } else {
        printf("Package integrity: FAIL (%d)\n", result);
        tefpkg_close(pkg);
        return 1;
    }
    
    // 4. 遍历并提取所有条目
    for (uint32_t i = 0; i < count; i++) {
        // 获取条目信息
        tefpkg_entry_t* info = NULL;
        tefpkg_get_entry_info(pkg, i, &info);
        
        if (info) {
            printf("\nEntry %u:\n", i);
            printf("  Original size: %u\n", info->original_size);
            printf("  Compressed size: %u\n", info->compressed_size);
            printf("  Compression: %s\n", 
                   info->compress_type == COMPRESS_NONE ? "None" :
                   info->compress_type == COMPRESS_LZ4 ? "LZ4" : "LZ4HC");
        }
        
        // 提取到内存
        uint8_t* data = NULL;
        uint32_t size = 0;
        result = tefpkg_extract_entry_to_memory(pkg, i, &data, &size);
        
        if (result == TEF_OK && data) {
            printf("  Extracted: %u bytes\n", size);
            
            // 如果是文本，打印内容
            if (size > 0 && data[size - 1] == '\0') {
                printf("  Content: %s\n", (char*)data);
            }
            
            free(data);
        }
    }
    
    // 5. 验证签名
    uint64_t fingerprint = 0x1234567890ABCDEF;
    result = tefpkg_verify_signature(pkg, fingerprint);
    if (result == TEF_OK) {
        printf("\nSignature: Valid\n");
    } else {
        printf("\nSignature: Invalid (or not signed)\n");
    }
    
    // 6. 关闭包
    tefpkg_close(pkg);
    
    return 0;
}
```

### 打包动态库

这是 TEFKernel 中最常见的场景：将 Plugin/Module/ModLoader 的动态库打包成 `.tefpkg` 文件。

```c
#include <stdio.h>
#include "tefpackage/tefpkg.h"

// 预定义的条目 ID（在 TEFKernel 中使用）
#define TEFPKG_ID_DYLIB    0  // 动态库文件
#define TEFPKG_ID_MANIFEST 1  // 清单文件
#define TEFPKG_ID_ICON     2  // 图标文件

int package_plugin(const char* dylib_path, const char* output_path) {
    tefpkg_t* pkg = NULL;
    
    // 创建包（预留 5 个条目）
    tefpkg_result_t result = tefpkg_create_reserved_from_memory(5, &pkg);
    if (result != TEF_OK) {
        printf("Failed to create package\n");
        return 1;
    }
    
    // 添加动态库（使用 LZ4 压缩）
    result = tefpkg_add_entry_from_file(
        pkg, dylib_path, COMPRESS_LZ4, 0
    );
    if (result != TEF_OK) {
        printf("Failed to add dynamic library\n");
        tefpkg_close(pkg);
        return 1;
    }
    printf("Added dynamic library\n");
    
    // 添加清单文件（JSON 格式的元数据）
    const char* manifest = 
        "{\n"
        "  \"id\": \"com.example.myplugin\",\n"
        "  \"name\": \"My Plugin\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"author\": \"Your Name\"\n"
        "}\n";
    
    result = tefpkg_add_entry_from_memory(
        pkg, COMPRESS_LZ4, 0,
        (uint8_t*)manifest, strlen(manifest) + 1
    );
    if (result != TEF_OK) {
        printf("Failed to add manifest\n");
        tefpkg_close(pkg);
        return 1;
    }
    printf("Added manifest\n");
    
    // 签名包
    uint64_t fingerprint = 0x1234567890ABCDEF;
    result = tefpkg_sign_package(pkg, fingerprint);
    if (result != TEF_OK) {
        printf("Failed to sign package\n");
        tefpkg_close(pkg);
        return 1;
    }
    
    // 保存到文件
    result = tefpkg_save_file(pkg, fingerprint);
    if (result != TEF_OK) {
        printf("Failed to save package\n");
        tefpkg_close(pkg);
        return 1;
    }
    printf("Package saved to: %s\n", output_path);
    
    tefpkg_close(pkg);
    return 0;
}

int main() {
    return package_plugin("libmyplugin.so", "myplugin.tefpkg");
}
```

---

## ⚠️ 注意事项

### 访问模式

| 模式                   | 用途           | 注意事项                             |
|:-----------------------|:---------------|:-------------------------------------|
| `TEF_ACCESS_MEMORY`    | 内存中创建包   | 数据在内存中，需要保存到文件才持久化 |
| `TEF_ACCESS_READONLY`  | 只读打开文件   | 不能添加或修改条目                   |
| `TEF_ACCESS_READWRITE` | 读写模式       | 支持添加条目，需要预留空间           |
| `TEF_ACCESS_MEMDATA`   | 从内存数据打开 | 数据源是只读的                       |

### 预留条目

```c
// ❌ 如果没有预留空间，添加条目会失败
tefpkg_t* pkg;
tefpkg_create_reserved_from_memory(0, &pkg);  // 没有预留
tefpkg_add_entry_from_memory(pkg, ...);  // 返回 TEF_ERROR_NO_SPACE

// ✅ 创建时预留足够空间
tefpkg_create_reserved_from_memory(10, &pkg);  // 预留 10 个
tefpkg_add_entry_from_memory(pkg, ...);  // OK
```

### 内存管理

```c
// 提取到内存的数据需要手动释放
uint8_t* data = NULL;
uint32_t size = 0;
tefpkg_extract_entry_to_memory(pkg, 0, &data, &size);
// ... 使用数据 ...
free(data);  // ✅ 必须释放
```

### 压缩选择

| 压缩类型         | 压缩比 | 速度 | 适用场景           |
|:-----------------|:-------|:-----|:-------------------|
| `COMPRESS_NONE`  | 1x     | 最快 | 小文件、已压缩数据 |
| `COMPRESS_LZ4`   | 2-3x   | 快   | 通用场景 ✅        |
| `COMPRESS_LZ4HC` | 3-5x   | 慢   | 需要最小体积       |

### 签名

```c
// 签名前必须添加完所有条目
tefpkg_sign_package(pkg, fingerprint);  // ✅ 最后调用

// 签名后不能再添加条目
tefpkg_sign_package(pkg, fingerprint);
tefpkg_add_entry_from_memory(pkg, ...);  // ❌ 会失败
```

---

## 📊 常用工作流

### 开发者工作流

```
1. 创建包 (tefpkg_create_reserved_from_memory)
2. 添加代码/资源 (tefpkg_add_entry_from_file/memory)
3. 签名包 (tefpkg_sign_package)
4. 保存包 (tefpkg_save_file)
5. 分发 .tefpkg 文件
```

### 用户工作流

```
1. 打开包 (tefpkg_open_readonly)
2. 验证包 (tefpkg_verify_pkg)
3. 验证签名 (tefpkg_verify_signature)
4. 提取动态库 (tefpkg_extract_entry_to_memory)
5. 加载动态库 (memdl_open)
6. 关闭包 (tefpkg_close)
```

*Happy Packaging! 📦✨*