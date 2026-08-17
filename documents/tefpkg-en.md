# 📦 TEFPKG Package Format User Guide

TEFPKG is TEFKernel's self-describing packaging format, used for distributing and storing code, resources, and other files. It supports features such as compression, integrity verification, signing, and reserved entries, serving as the foundational carrier for Plugins, Modules, ModLoaders, and Mods in the TEFKernel ecosystem.

- **[Packaging Tool](https://github.com/eternalfuture-e38299/TEFPkg-Tool)**

---

## 📑 Table of Contents

<details>
<summary><b>📖 Click to expand full table of contents</b></summary>

- [📖 Overview](#-overview)
  - [Features](#features)
  - [Error Codes](#error-codes)
- [🔧 Lifecycle Management](#-lifecycle-management)
  - [Creating a Package](#creating-a-package)
  - [Opening a Package](#opening-a-package)
  - [Saving a Package](#saving-a-package)
  - [Closing a Package](#closing-a-package)
- [📂 Entry Operations](#-entry-operations)
  - [Adding Entries](#adding-entries)
  - [Extracting Entries](#extracting-entries)
  - [Getting Entry Information](#getting-entry-information)
- [✅ Verification and Signing](#-verification-and-signing)
  - [Integrity Verification](#integrity-verification)
  - [Signature Verification](#signature-verification)
  - [Signing a Package](#signing-a-package)
- [💡 Complete Examples](#-complete-examples)
  - [Creating and Saving a Package](#creating-and-saving-a-package)
  - [Opening and Extracting a Package](#opening-and-extracting-a-package)
  - [Packaging a Dynamic Library](#packaging-a-dynamic-library)
- [⚠️ Important Notes](#-important-notes)

</details>

---

## 📖 Overview

### Features

| Feature                       | Description                                                                      |
|:------------------------------|:---------------------------------------------------------------------------------|
| **🗜️ Compression**            | Supports LZ4 / LZ4HC compression algorithms, each entry compressed independently |
| **✅ Integrity Check**        | Header checksum + content hash (CRC64)                                           |
| **🔐 Signature Verification** | Supports package signing to prevent tampering                                    |
| **📊 Reserved Entries**       | Can reserve entry slots at creation to reduce reallocation when appending        |
| **🔍 Fast Indexing**          | Quick file access via index                                                      |
| **💾 Multi-mode Access**      | Supports memory mode, read-only mode, and read-write mode                        |

### Error Codes

| Error Code                | Value | Description                                |
|:--------------------------|:------|:-------------------------------------------|
| `TEF_OK`                  | 0     | Operation successful ✅                    |
| `TEF_ERROR`               | -1    | General error                              |
| `TEF_ERROR_SIGNATURE`     | -2    | Signature verification failed              |
| `TEF_ERROR_CORRUPT`       | -3    | Data corruption or format error            |
| `TEF_ERROR_MEMORY`        | -4    | Memory allocation failed                   |
| `TEF_ERROR_IO`            | -5    | Input/output error                         |
| `TEF_ERROR_KEYFILE`       | -6    | Key file error                             |
| `TEF_ERROR_NOT_FOUND`     | -7    | File or resource not found                 |
| `TEF_ERROR_INVALID`       | -8    | Invalid parameter or incorrect state       |
| `TEF_ERROR_NOT_SIGNATURE` | -9    | Package is not signed                      |
| `TEF_ERROR_INTEGRITY`     | -10   | Integrity check failed                     |
| `TEF_ERROR_NO_SPACE`      | -11   | No more space (reserved entries exhausted) |

---

## 🔧 Lifecycle Management

### Creating a Package

#### Create from File (with Reserved Space)

```c
tefpkg_result_t tefpkg_create_reserved_from_file(
    const char *filename,
    uint16_t reserved_entries,
    tefpkg_t **pkg
);
```

**Example:**

```c
#include "tefpackage/tefpkg.h"

tefpkg_t* pkg = NULL;
tefpkg_result_t result = tefpkg_create_reserved_from_file(
    "my_package.tefpkg",
    10,   // Reserve 10 entries
    &pkg
);

if (result == TEF_OK) {
    printf("Package created successfully!\n");
} else {
    printf("Failed to create package: %d\n", result);
}
```

#### Create from Memory (with Reserved Space)

```c
tefpkg_result_t tefpkg_create_reserved_from_memory(
    uint16_t reserved_entries,
    tefpkg_t **pkg
);
```

**Example:**

```c
tefpkg_t* pkg = NULL;
tefpkg_result_t result = tefpkg_create_reserved_from_memory(10, &pkg);

if (result == TEF_OK) {
    printf("In-memory package created!\n");
}
```

### Opening a Package

#### Open Read-Only

```c
tefpkg_result_t tefpkg_open_readonly(
    const char *filename,
    tefpkg_t **pkg
);
```

**Example:**

```c
tefpkg_t* pkg = NULL;
tefpkg_result_t result = tefpkg_open_readonly("existing_package.tefpkg", &pkg);

if (result == TEF_OK) {
    printf("Package opened successfully!\n");
    printf("Contains %d files\n", tefpkg_get_entries_count(pkg));
}
```

#### Open from Memory Data

```c
tefpkg_result_t tefpkg_open_from_memory(
    const uint8_t *data,
    uint32_t data_size,
    tefpkg_t **pkg
);
```

**Example:**

```c
// Assume package data was read into memory from somewhere
uint8_t* pkg_data = ...;
uint32_t pkg_size = ...;

tefpkg_t* pkg = NULL;
tefpkg_result_t result = tefpkg_open_from_memory(pkg_data, pkg_size, &pkg);

if (result == TEF_OK) {
    printf("Package opened from memory!\n");
}
```

### Saving a Package

#### Save to File

```c
tefpkg_result_t tefpkg_save_file(
    tefpkg_t *pkg,
    uint64_t fingerprint
);
```

**Example:**

```c
tefpkg_t* pkg = ...; // Already created package
uint64_t fingerprint = 0x1234567890ABCDEF;

tefpkg_result_t result = tefpkg_save_file(pkg, fingerprint);

if (result == TEF_OK) {
    printf("Package saved successfully!\n");
}
```

#### Save from Memory to File

```c
tefpkg_result_t tefpkg_save_memory_file(
    const char *filename,
    tefpkg_t *pkg,
    uint64_t fingerprint
);
```

**Example:**

```c
tefpkg_t* pkg = ...;
tefpkg_result_t result = tefpkg_save_memory_file(
    "output.tefpkg",
    pkg,
    0x1234567890ABCDEF
);
```

### Closing a Package

```c
void tefpkg_close(tefpkg_t* pkg);
```

**Example:**

```c
tefpkg_t* pkg = ...;
tefpkg_close(pkg);
pkg = NULL;
```

---

## 📂 Entry Operations

### Adding Entries

#### Add from Memory

```c
tefpkg_result_t tefpkg_add_entry_from_memory(
    tefpkg_t *pkg,
    tefpkg_compress_t compress_type,
    uint8_t compress_level,
    uint8_t *data,
    uint32_t data_size
);
```

**Parameter Description:**

| Parameter        | Description                                        |
|:-----------------|:---------------------------------------------------|
| `compress_type`  | `COMPRESS_NONE`, `COMPRESS_LZ4`, `COMPRESS_LZ4HC`  |
| `compress_level` | Compression level (0-9), only effective for LZ4HC  |

**Example:**

```c
tefpkg_t* pkg = ...;
const char* text = "Hello, TEF Package!";
uint8_t* data = (uint8_t*)text;
uint32_t data_size = strlen(text) + 1;

tefpkg_result_t result = tefpkg_add_entry_from_memory(
    pkg,
    COMPRESS_LZ4,    // Use LZ4 compression
    0,               // Default compression level
    data,
    data_size
);

if (result == TEF_OK) {
    printf("Entry added successfully!\n");
}
```

#### Add from File

```c
tefpkg_result_t tefpkg_add_entry_from_file(
    tefpkg_t *pkg,
    const char *filepath,
    tefpkg_compress_t compress_type,
    uint8_t compress_level
);
```

**Example:**

```c
tefpkg_t* pkg = ...;

tefpkg_result_t result = tefpkg_add_entry_from_file(
    pkg,
    "image.png",
    COMPRESS_LZ4HC,  // High compression ratio
    9                // Maximum compression level
);
```

### Extracting Entries

#### Extract to Memory

```c
tefpkg_result_t tefpkg_extract_entry_to_memory(
    const tefpkg_t *pkg,
    uint32_t entry_index,
    uint8_t **data,
    uint32_t *data_size
);
```

**Example:**

```c
tefpkg_t* pkg = ...;
uint8_t* extracted_data = NULL;
uint32_t extracted_size = 0;

tefpkg_result_t result = tefpkg_extract_entry_to_memory(
    pkg,
    0,   // Extract the first entry
    &extracted_data,
    &extracted_size
);

if (result == TEF_OK) {
    printf("Extracted %u bytes\n", extracted_size);
    // Use the data...
    free(extracted_data);  // Free after use
}
```

#### Extract to File

```c
tefpkg_result_t tefpkg_extract_entry_to_file(
    const tefpkg_t *pkg,
    uint32_t entry_index,
    const char *output_path
);
```

**Example:**

```c
tefpkg_t* pkg = ...;

tefpkg_result_t result = tefpkg_extract_entry_to_file(
    pkg,
    1,                     // Second entry
    "extracted_file.bin"   // Output path
);
```

### Getting Entry Information

```c
tefpkg_result_t tefpkg_get_entry_info(
    const tefpkg_t *pkg,
    uint32_t entry_index,
    tefpkg_entry_t **info
);
```

**Example:**

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

### Helper Functions

```c
// Get entry count
uint16_t count = tefpkg_get_entries_count(pkg);

// Get reserved entry count
uint16_t reserved = tefpkg_get_reserved_entries(pkg);
```

---

## ✅ Verification and Signing

### Integrity Verification

#### Verify a Single Entry

```c
tefpkg_result_t tefpkg_verify_entry(
    const tefpkg_t *pkg,
    uint32_t entry_index
);
```

**Example:**

```c
tefpkg_t* pkg = ...;

tefpkg_result_t result = tefpkg_verify_entry(pkg, 0);

if (result == TEF_OK) {
    printf("Entry 0 is intact!\n");
} else {
    printf("Entry 0 is corrupted!\n");
}
```

#### Verify the Entire Package

```c
tefpkg_result_t tefpkg_verify_pkg(const tefpkg_t *pkg);
```

**Example:**

```c
tefpkg_t* pkg = ...;

tefpkg_result_t result = tefpkg_verify_pkg(pkg);

if (result == TEF_OK) {
    printf("Package integrity verified!\n");
}
```

### Signature Verification

```c
tefpkg_result_t tefpkg_verify_signature(
    const tefpkg_t *pkg,
    uint64_t fingerprint
);
```

**Example:**

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

### Signing a Package

```c
tefpkg_result_t tefpkg_sign_package(
    tefpkg_t *pkg,
    uint64_t fingerprint
);
```

**Example:**

```c
tefpkg_t* pkg = ...;
uint64_t my_fingerprint = 0x1234567890ABCDEF;

tefpkg_result_t result = tefpkg_sign_package(pkg, my_fingerprint);

if (result == TEF_OK) {
    printf("Package signed successfully!\n");
}
```

---

## 💡 Complete Examples

### Creating and Saving a Package

```c
#include <stdio.h>
#include <string.h>
#include "tefpackage/tefpkg.h"

int main() {
    tefpkg_t* pkg = NULL;
    
    // 1. Create a package (reserve 5 entries)
    printf("Creating package...\n");
    tefpkg_result_t result = tefpkg_create_reserved_from_memory(5, &pkg);
    if (result != TEF_OK) {
        printf("Failed to create package: %d\n", result);
        return 1;
    }
    
    // 2. Add entries
    printf("Adding entries...\n");
    
    // Add text file
    const char* text = "Hello, this is a TEF package!";
    result = tefpkg_add_entry_from_memory(
        pkg, COMPRESS_LZ4, 0,
        (uint8_t*)text, strlen(text) + 1
    );
    printf("  Added text entry: %s\n", result == TEF_OK ? "OK" : "FAIL");
    
    // Add binary data
    uint8_t binary_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    result = tefpkg_add_entry_from_memory(
        pkg, COMPRESS_NONE, 0,
        binary_data, sizeof(binary_data)
    );
    printf("  Added binary entry: %s\n", result == TEF_OK ? "OK" : "FAIL");
    
    // 3. Sign the package
    uint64_t fingerprint = 0x1234567890ABCDEF;
    result = tefpkg_sign_package(pkg, fingerprint);
    printf("Signing package: %s\n", result == TEF_OK ? "OK" : "FAIL");
    
    // 4. Save to file
    result = tefpkg_save_file(pkg, fingerprint);
    printf("Saving package: %s\n", result == TEF_OK ? "OK" : "FAIL");
    
    // 5. Close the package
    tefpkg_close(pkg);
    
    printf("Done!\n");
    return 0;
}
```

### Opening and Extracting a Package

```c
#include <stdio.h>
#include "tefpackage/tefpkg.h"

int main() {
    tefpkg_t* pkg = NULL;
    
    // 1. Open the package
    printf("Opening package...\n");
    tefpkg_result_t result = tefpkg_open_readonly("my_package.tefpkg", &pkg);
    if (result != TEF_OK) {
        printf("Failed to open package: %d\n", result);
        return 1;
    }
    
    // 2. Get basic information
    uint16_t count = tefpkg_get_entries_count(pkg);
    uint16_t reserved = tefpkg_get_reserved_entries(pkg);
    printf("Entries: %d, Reserved: %d\n", count, reserved);
    
    // 3. Verify package integrity
    result = tefpkg_verify_pkg(pkg);
    if (result == TEF_OK) {
        printf("Package integrity: OK\n");
    } else {
        printf("Package integrity: FAIL (%d)\n", result);
        tefpkg_close(pkg);
        return 1;
    }
    
    // 4. Iterate and extract all entries
    for (uint32_t i = 0; i < count; i++) {
        // Get entry information
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
        
        // Extract to memory
        uint8_t* data = NULL;
        uint32_t size = 0;
        result = tefpkg_extract_entry_to_memory(pkg, i, &data, &size);
        
        if (result == TEF_OK && data) {
            printf("  Extracted: %u bytes\n", size);
            
            // If it's text, print the content
            if (size > 0 && data[size - 1] == '\0') {
                printf("  Content: %s\n", (char*)data);
            }
            
            free(data);
        }
    }
    
    // 5. Verify signature
    uint64_t fingerprint = 0x1234567890ABCDEF;
    result = tefpkg_verify_signature(pkg, fingerprint);
    if (result == TEF_OK) {
        printf("\nSignature: Valid\n");
    } else {
        printf("\nSignature: Invalid (or not signed)\n");
    }
    
    // 6. Close the package
    tefpkg_close(pkg);
    
    return 0;
}
```

### Packaging a Dynamic Library

This is the most common scenario in TEFKernel: packaging a Plugin/Module/ModLoader dynamic library into a `.tefpkg` file.

```c
#include <stdio.h>
#include "tefpackage/tefpkg.h"

// Predefined entry IDs (used in TEFKernel)
#define TEFPKG_ID_DYLIB    0  // Dynamic library file
#define TEFPKG_ID_MANIFEST 1  // Manifest file
#define TEFPKG_ID_ICON     2  // Icon file

int package_plugin(const char* dylib_path, const char* output_path) {
    tefpkg_t* pkg = NULL;
    
    // Create package (reserve 5 entries)
    tefpkg_result_t result = tefpkg_create_reserved_from_memory(5, &pkg);
    if (result != TEF_OK) {
        printf("Failed to create package\n");
        return 1;
    }
    
    // Add dynamic library (using LZ4 compression)
    result = tefpkg_add_entry_from_file(
        pkg, dylib_path, COMPRESS_LZ4, 0
    );
    if (result != TEF_OK) {
        printf("Failed to add dynamic library\n");
        tefpkg_close(pkg);
        return 1;
    }
    printf("Added dynamic library\n");
    
    // Add manifest file (JSON format metadata)
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
    
    // Sign the package
    uint64_t fingerprint = 0x1234567890ABCDEF;
    result = tefpkg_sign_package(pkg, fingerprint);
    if (result != TEF_OK) {
        printf("Failed to sign package\n");
        tefpkg_close(pkg);
        return 1;
    }
    
    // Save to file
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

## ⚠️ Important Notes

### Access Modes

| Mode                   | Use Case                 | Notes                                                |
|:-----------------------|:-------------------------|:-----------------------------------------------------|
| `TEF_ACCESS_MEMORY`    | Create package in memory | Data is in memory, must save to file for persistence |
| `TEF_ACCESS_READONLY`  | Open file read-only      | Cannot add or modify entries                         |
| `TEF_ACCESS_READWRITE` | Read-write mode          | Supports adding entries, requires reserved space     |
| `TEF_ACCESS_MEMDATA`   | Open from memory data    | Data source is read-only                             |

### Reserved Entries

```c
// ❌ Without reserved space, adding entries will fail
tefpkg_t* pkg;
tefpkg_create_reserved_from_memory(0, &pkg);  // No reservation
tefpkg_add_entry_from_memory(pkg, ...);  // Returns TEF_ERROR_NO_SPACE

// ✅ Reserve enough space at creation
tefpkg_create_reserved_from_memory(10, &pkg);  // Reserve 10
tefpkg_add_entry_from_memory(pkg, ...);  // OK
```

### Memory Management

```c
// Data extracted to memory must be manually freed
uint8_t* data = NULL;
uint32_t size = 0;
tefpkg_extract_entry_to_memory(pkg, 0, &data, &size);
// ... Use the data ...
free(data);  // ✅ Must free
```

### Compression Selection

| Compression Type | Compression Ratio | Speed   | Use Case                             |
|:-----------------|:------------------|:--------|:-------------------------------------|
| `COMPRESS_NONE`  | 1x                | Fastest | Small files, already compressed data |
| `COMPRESS_LZ4`   | 2-3x              | Fast    | General purpose ✅                   |
| `COMPRESS_LZ4HC` | 3-5x              | Slow    | When minimum size is needed          |

### Signing

```c
// All entries must be added before signing
tefpkg_sign_package(pkg, fingerprint);  // ✅ Called last

// Cannot add entries after signing
tefpkg_sign_package(pkg, fingerprint);
tefpkg_add_entry_from_memory(pkg, ...);  // ❌ Will fail
```

---

## 📊 Common Workflows

### Developer Workflow

```
1. Create package (tefpkg_create_reserved_from_memory)
2. Add code/resources (tefpkg_add_entry_from_file/memory)
3. Sign package (tefpkg_sign_package)
4. Save package (tefpkg_save_file)
5. Distribute .tefpkg file
```

### User Workflow

```
1. Open package (tefpkg_open_readonly)
2. Verify package (tefpkg_verify_pkg)
3. Verify signature (tefpkg_verify_signature)
4. Extract dynamic library (tefpkg_extract_entry_to_memory)
5. Load dynamic library (memdl_open)
6. Close package (tefpkg_close)
```

*Happy Packaging! 📦✨*