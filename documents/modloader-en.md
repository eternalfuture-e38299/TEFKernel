# 📋 TEFKernel ModLoader Development Guide

> **⚠️ Core Positioning**: ModLoader is a container component in TEFKernel **specifically responsible for loading and managing game Mods**. It runs after Modules and can leverage game extension features provided by Modules (such as texture packs, UI systems, etc.) to provide a richer runtime environment for Mods. The ModLoader itself does not directly implement game functionality but serves as the "host" for Mods.

---

## 📑 Table of Contents

<details>
<summary><b>📖 Click to expand full table of contents</b></summary>

- [📖 Overview](#-overview)
  - [Core Responsibilities](#core-responsibilities)
  - [Relationship with Plugins/Modules](#relationship-with-pluginsmodules)
  - [Lifecycle](#lifecycle)
- [🚀 Quick Start](#-quick-start)
  - [Minimal ModLoader Example](#minimal-modloader-example)
  - [Build Commands](#build-commands)
- [📚 API Reference](#-api-reference)
  - [ModLoader Information Structure](#modloader-information-structure)
  - [Operation Function Table](#operation-function-table)
  - [Mod Manifest Structure](#mod-manifest-structure)
  - [ModLoader Entry Structure](#modloader-entry-structure)
  - [Multiplayer Information Structure](#multiplayer-information-structure)
  - [Result Codes](#result-codes)
  - [Mandatory Export Function](#mandatory-export-function)
  - [Using Module Functionality](#using-module-functionality)
- [💡 Complete Example: Multi-Format ModLoader](#-complete-example-multi-format-modloader)
- [📦 Packaging and Deployment](#-packaging-and-deployment)
  - [Directory Structure](#directory-structure)
  - [Using TEFPkg-Tool for Packaging](#using-tefpkg-tool-for-packaging)
  - [Deployment Configuration](#deployment-configuration)
  - [Mod Directory Structure](#mod-directory-structure)
- [🔄 Mod Lifecycle Management](#-mod-lifecycle-management)
  - [Loading Process](#loading-process)
  - [Unloading Process](#unloading-process)
  - [Hot-Reload Process](#hot-reload-process)
- [🔒 Multiplayer Security Mechanism](#-multiplayer-security-mechanism)
- [⚠️ Important Notes](#-important-notes)
  - [Memory Management](#memory-management)
  - [Mod Isolation](#mod-isolation)
  - [Error Handling](#error-handling)
  - [Performance Considerations](#performance-considerations)
- [🔗 Related Links](#-related-links)

</details>

---

## 📖 Overview

### Core Responsibilities

| Responsibility               | Description                                                                   |
|:-----------------------------|:------------------------------------------------------------------------------|
| **📂 Mod Loading**           | Loads Mod files (.tefpkg, .zip, etc.) from specified directories              |
| **🔧 Mod Initialization**    | Calls Mod initialization logic, prepares runtime environment                  |
| **🔄 Mod Hot-Reload**        | Supports runtime reloading of Mods                                            |
| **🗑️ Mod Unloading**         | Cleans up resources occupied by Mods                                          |
| **🔒 Multiplayer Security**  | Provides multiplayer security checks, marks whether Mods are multiplayer-safe |
| **📋 Dependency Management** | Declares Plugin dependencies for the ModLoader                                |

### Relationship with Plugins/Modules

| Type          | Positioning               | Dependency                                    |
|:--------------|:--------------------------|:----------------------------------------------|
| **Plugin**    | Low-level symbol provider | Does not depend on other components           |
| **Module**    | Game feature extension    | Can depend on Plugin symbols                  |
| **ModLoader** | Mod loading container     | Can depend on Plugin and Module functionality |
| **Mod**       | End-user mod              | Managed by ModLoader                          |

> **Loading Order**: Plugin → Module → ModLoader → Mod

### Lifecycle

```
Kernel loads .tefpkg
↓
memdl_open() loads dynamic library
↓
Call ml_create() to get operation table
↓
Verify Plugin dependencies (plugin_dependencies)
↓
Call init_ml() → ModLoader self-initialization
↓
Kernel reads modloader/{pkg_id}/enables.txt
↓
For each enabled Mod:
├── Build mod_manifest_t
├── Call load_mod() to load Mod
└── Call init_mod() to initialize Mod
↓
ModLoader runs normally (during game runtime)
↓
(Optional) Hot-reload → Call reload_mod()
↓
On unload:
├── Call unload_mod() to unload all Mods
└── Call cleanup_ml() to clean up ModLoader
↓
Unload dynamic library
```

---

## 🚀 Quick Start

### Minimal ModLoader Example

```c
// minimal_modloader.c
#include "modloader/modloader_core.h"
#include <stdio.h>

// ============================================================
// 1. ModLoader Information (Static Constant)
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
// 2. Mod Operation Functions
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
    // Return static multiplayer information
    static multiplayer_mod_info_t info = {
        .mod_id = "com.example.mod",
        .is_multiplayer_safe = 1,
        .version_code = 1,
        .version = "1.0.0"
    };
    return &info;
}

// ============================================================
// 3. ModLoader Lifecycle
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
// 4. Operation Table (Static Constant)
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
// 5. Mandatory Export
// ============================================================
API_EXPORT const ml_ops_t* API_CALL ml_create(void) {
    return &g_ops;
}
```

### Build Commands

```bash
# Linux / Android
gcc -shared -fPIC -I/path/to/tefkernel minimal_modloader.c -o libminimal_ml.so

# Windows (MinGW)
gcc -shared -I/path/to/tefkernel minimal_modloader.c -o minimal_ml.dll

# macOS
gcc -shared -fPIC -I/path/to/tefkernel minimal_modloader.c -o libminimal_ml.dylib
```

---

## 📚 API Reference

### ModLoader Information Structure

```c
typedef struct {
    const char *pkg_id;                    ///< Unique identifier (reverse domain)
    int version_code;                      ///< Version code (numeric comparison)
    const char *version;                   ///< Version string
    int api_version;                       ///< API version number
    int plugin_dependencies_sizes;         ///< Number of dependent Plugins
    const char **plugin_dependencies;      ///< List of dependent Plugin pkg_ids
} ml_info_t;
```

### Operation Function Table

```c
typedef struct {
    /**
     * @brief Load a single Mod
     * @param mod_manifest Mod description information
     * @return ML_SUCCESS or error code
     */
    ml_result_t (*load_mod)(mod_manifest_t *mod_manifest);

    /**
     * @brief Unload a single Mod
     * @param mod_manifest Mod description information
     * @return ML_SUCCESS or error code
     */
    ml_result_t (*unload_mod)(mod_manifest_t *mod_manifest);

    /**
     * @brief Reload a Mod (hot-reload)
     * @param mod_manifest Mod description information
     * @return ML_SUCCESS or error code
     */
    ml_result_t (*reload_mod)(mod_manifest_t *mod_manifest);

    /**
     * @brief Initialize a single Mod
     * @param mod_manifest Mod description information
     * @return ML_SUCCESS or error code
     */
    ml_result_t (*init_mod)(mod_manifest_t *mod_manifest);

    /**
     * @brief Get multiplayer detection information for a Mod
     * @param mod_manifest Mod description information
     * @return Pointer to multiplayer_mod_info_t (static memory)
     * @warning The kernel will not free the returned information; static memory must be used
     */
    const multiplayer_mod_info_t * (*get_multiplayer_info)(mod_manifest_t *mod_manifest);

    /**
     * @brief Initialize the ModLoader
     * @param ml_entry ModLoader entry
     * @return ML_SUCCESS or error code
     */
    ml_result_t (*init_ml)(ml_entry_t* ml_entry);

    /**
     * @brief Clean up and close the ModLoader
     * @param ml_entry ModLoader entry
     * @return ML_SUCCESS or error code
     */
    ml_result_t (*cleanup_ml)(ml_entry_t* ml_entry);

    /**
     * @brief Get ModLoader information
     * @return Pointer to static ml_info_t
     */
    const ml_info_t *(*get_info)(void);
} ml_ops_t;
```

### Mod Manifest Structure

```c
typedef struct {
    const char *path;          ///< Mod file path
    const char *mod_id;        ///< Mod unique identifier
    const char *private_dir;   ///< Mod private directory
    const char *logs_dir;      ///< Mod log directory
} mod_manifest_t;
```

### ModLoader Entry Structure

```c
typedef struct ml_entry_t {
    ml_info_t *info;           ///< ModLoader information (filled by kernel)
    ml_ops_t *ops;             ///< Operation table (provided by ModLoader)
    tefpkg_t *pkg_handle;      ///< Package handle (filled by kernel)
    const char *private_dir;   ///< ModLoader private directory
    const char *logs_dir;      ///< ModLoader log directory
} ml_entry_t;
```

### Multiplayer Information Structure

```c
typedef struct {
    const char *mod_id;                ///< Mod unique identifier
    int is_multiplayer_safe;           ///< Whether multiplayer-safe (1=safe, 0=unsafe)
    int version_code;                  ///< Version code
    const char *version;               ///< Version string
} multiplayer_mod_info_t;
```

### Result Codes

```c
typedef enum {
    ML_SUCCESS = 0,              ///< Operation successful
    ML_ERROR = -1,               ///< General error
    ML_ERROR_INVALID_PARAM = -2, ///< Invalid parameter
    ML_ERROR_NOT_FOUND = -3      ///< Mod not found
} ml_result_t;
```

### Mandatory Export Function

```c
/**
 * @brief The only function that a ModLoader must export
 * @return Pointer to the ModLoader operation function table (must be static memory)
 */
API_EXPORT const ml_ops_t * API_CALL ml_create(void);
```

### Using Module Functionality

```c
#include "patchlib/type.h"
#include "patchlib/method.h"

// Example: Using texture loading functionality provided by a Module in ModLoader
static ml_result_t load_mod(mod_manifest_t* manifest) {
    // Assume the Module registered a texture loading symbol
    typedef bool (*load_texture_t)(const char* path);
    load_texture_t load_tex = (load_texture_t)tpf_get_symbol("load_texture");
    
    if (load_tex) {
        // Load Mod texture
        load_tex(manifest->path);
    }
    
    return ML_SUCCESS;
}
```

---

## 💡 Complete Example: Multi-Format ModLoader

This example demonstrates a ModLoader that supports both `.tefpkg` and `.zip` formats:

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
// 1. ModLoader Information (Depends on a Module)
// ============================================================
static const char* deps[] = {
    "com.example.texture_pack"  // Depends on texture module
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
// 2. ModLoader Private State
// ============================================================
typedef struct {
    tefstd_vector_t loaded_mods;   // List of loaded Mod IDs
    char mods_dir[512];             // Mod storage directory
    char logs_dir[512];             // Log directory
} ml_state_t;

static ml_state_t g_state = {0};

// ============================================================
// 3. Mod Operation Implementation
// ============================================================

// Check Mod format and load
static ml_result_t load_mod(mod_manifest_t* manifest) {
    printf("[ML] Loading mod: %s\n", manifest->mod_id);
    printf("[ML]   Path: %s\n", manifest->path);
    printf("[ML]   Private: %s\n", manifest->private_dir);
    
    // Check if file exists
    if (access(manifest->path, F_OK) != 0) {
        printf("[ML]   ERROR: File not found!\n");
        return ML_ERROR_NOT_FOUND;
    }
    
    // Select loading method based on extension
    const char* ext = strrchr(manifest->path, '.');
    if (ext && strcmp(ext, ".tefpkg") == 0) {
        // Load .tefpkg format
        tefpkg_t* pkg = NULL;
        tefpkg_result_t result = tefpkg_open_readonly(manifest->path, &pkg);
        if (result == TEF_OK) {
            printf("[ML]   Loaded .tefpkg with %d files\n", 
                   tefpkg_get_entries_count(pkg));
            tefpkg_close(pkg);
            // Save load record
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
        // Load .zip format (example: simulate zip processing)
        printf("[ML]   Loading .zip format (simulated)\n");
        return ML_SUCCESS;
    }
    
    printf("[ML]   Unknown format\n");
    return ML_ERROR;
}

// Unload Mod
static ml_result_t unload_mod(mod_manifest_t* manifest) {
    printf("[ML] Unloading mod: %s\n", manifest->mod_id);
    
    // Remove from list
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

// Hot-reload Mod
static ml_result_t reload_mod(mod_manifest_t* manifest) {
    printf("[ML] Reloading mod: %s\n", manifest->mod_id);
    // Unload then load
    unload_mod(manifest);
    return load_mod(manifest);
}

// Initialize Mod
static ml_result_t init_mod(mod_manifest_t* manifest) {
    printf("[ML] Initializing mod: %s\n", manifest->mod_id);
    // Here you can call the Mod's entry function
    return ML_SUCCESS;
}

// ============================================================
// 4. Multiplayer Information
// ============================================================
static const multiplayer_mod_info_t* get_multiplayer_info(mod_manifest_t* manifest) {
    // Static storage, kernel will use but not free
    static multiplayer_mod_info_t info;
    
    // Return different multiplayer information based on Mod ID
    if (strcmp(manifest->mod_id, "com.example.safe_mod") == 0) {
        info.is_multiplayer_safe = 1;
    } else if (strcmp(manifest->mod_id, "com.example.unsafe_mod") == 0) {
        info.is_multiplayer_safe = 0;
    } else {
        info.is_multiplayer_safe = 1; // Default safe
    }
    
    info.mod_id = manifest->mod_id;
    info.version = "1.0.0";
    info.version_code = 1;
    
    return &info;
}

// ============================================================
// 5. ModLoader Lifecycle
// ============================================================

static ml_result_t init_ml(ml_entry_t* entry) {
    printf("[ML] Initializing Advanced ModLoader...\n");
    
    // 1. Save directories
    snprintf(g_state.mods_dir, sizeof(g_state.mods_dir), "%s/mods", 
             entry->private_dir);
    snprintf(g_state.logs_dir, sizeof(g_state.logs_dir), "%s", 
             entry->logs_dir);
    
    // 2. Create directories
    mkdir(g_state.mods_dir, 0755);
    mkdir(g_state.logs_dir, 0755);
    
    // 3. Initialize state
    tefstd_vector_init(&g_state.loaded_mods, sizeof(char*));
    
    printf("[ML] Mods directory: %s\n", g_state.mods_dir);
    printf("[ML] Logs directory: %s\n", g_state.logs_dir);
    
    return ML_SUCCESS;
}

static ml_result_t cleanup_ml(ml_entry_t* entry) {
    printf("[ML] Cleaning up ModLoader...\n");
    
    // Free all Mod records
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
// 6. Operation Table and Export
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

## 📦 Packaging and Deployment

### Directory Structure

```
Working Directory/
├── modloader/
│   ├── enables.txt          # List of enabled ModLoaders
│   └── pkg/
│       └── com.example.advanced_ml.tefpkg
│
└── mods/                    # ModLoader runtime directory
    └── com.example.advanced_ml/
        ├── private/          # Private directory (ml_entry_t->private_dir)
        ├── logs/             # Log directory (ml_entry_t->logs_dir)
        ├── enables.txt       # List of Mods enabled by this ModLoader
        └── mod/              # Mod storage directory
            ├── com.example.mod1.tefpkg
            ├── com.example.mod2.tefpkg
            └── com.example.mod3.zip
```

### Using TEFPkg-Tool for Packaging

> **Official Tool**: https://github.com/eternalfuture-e38299/TEFPkg-Tool

#### Dynamic Library Naming Constraints

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

#### Executing Packaging

```bash
# Download and build TEFPkg-Tool
git clone https://github.com/eternalfuture-e38299/TEFPkg-Tool.git
cd TEFPkg-Tool
mkdir build && cd build
cmake .. && make

# Package the ModLoader
./tefpkg_tool build <dir> <outfile> <fingerprint>
```

### Deployment Configuration

#### modloader/enables.txt

```text
# List of enabled ModLoaders
com.example.advanced_ml
com.example.another_ml
```

#### ModLoader's enables.txt

* List of Mods enabled by this ModLoader
* Path: mods/{pkg_id}/enables.txt

```text
com.example.mod1
com.example.mod2
com.example.mod3
```

### Mod Directory Structure

Each ModLoader has its own independent Mod directory:

```
mods/com.example.advanced_ml/
├── enables.txt           # List of enabled Mods
├── private/              # Mod private data (passed to mod_manifest_t)
│   ├── com.example.mod1/
│   └── com.example.mod2/
├── logs/                 # Mod logs (passed to mod_manifest_t)
│   ├── com.example.mod1/
│   └── com.example.mod2/
└── mod/                  # Mod file storage
    ├── com.example.mod1.tefpkg
    └── com.example.mod2.tefpkg
```

---

## 🔄 Mod Lifecycle Management

### Loading Process

```
1. Kernel reads mods/{pkg_id}/enables.txt
2. For each Mod ID:
   a. Build mod_manifest_t:
      - path: mods/{pkg_id}/mod/{mod_id}.tefpkg
      - mod_id: read from enables.txt
      - private_dir: mods/{pkg_id}/private/{mod_id}
      - logs_dir: mods/{pkg_id}/logs/{mod_id}
   b. Call ml_ops->load_mod(manifest)
   c. If successful, call ml_ops->init_mod(manifest)
```

### Unloading Process

```
1. For each loaded Mod:
   a. Call ml_ops->unload_mod(manifest)
   b. Clean up Mod resources
```

### Hot-Reload Process

```
1. Kernel detects file changes (or manual trigger)
2. For each enabled Mod:
   a. Call ml_ops->reload_mod(manifest)
   b. Reload Mod resources
```

---

## 🔒 Multiplayer Security Mechanism

```c
// Mod multiplayer information example
static const multiplayer_mod_info_t* get_multiplayer_info(mod_manifest_t* manifest) {
    static multiplayer_mod_info_t info;
    
    // Determine safety based on Mod type
    if (is_mod_trusted(manifest->mod_id)) {
        info.is_multiplayer_safe = 1;  // Multiplayer-safe
    } else {
        info.is_multiplayer_safe = 0;  // Not multiplayer-safe
    }
    
    return &info;
}
```

---

## ⚠️ Important Notes

### Memory Management

```c
// ✅ Correct: Use static memory to return multiplayer information
static multiplayer_mod_info_t g_multiplayer_info;
static const multiplayer_mod_info_t* get_multiplayer_info(mod_manifest_t* manifest) {
    // Fill static structure
    g_multiplayer_info.mod_id = manifest->mod_id;
    g_multiplayer_info.is_multiplayer_safe = 1;
    return &g_multiplayer_info;  // Safe
}

// ❌ Incorrect: Dynamic allocation for return (kernel will not free)
static const multiplayer_mod_info_t* get_multiplayer_info(mod_manifest_t* manifest) {
    multiplayer_mod_info_t* info = malloc(sizeof(multiplayer_mod_info_t));  // Memory leak!
    return info;
}
```

### Mod Isolation

```c
// ✅ Each Mod uses its own independent directory
static ml_result_t load_mod(mod_manifest_t* manifest) {
    // Create Mod-specific directory
    mkdir(manifest->private_dir, 0755);
    mkdir(manifest->logs_dir, 0755);
    
    // Mod data only writes to its own private_dir
    return ML_SUCCESS;
}
```

### Error Handling

```c
// ✅ Proper error handling
static ml_result_t load_mod(mod_manifest_t* manifest) {
    if (!manifest || !manifest->path) {
        return ML_ERROR_INVALID_PARAM;
    }
    
    if (access(manifest->path, F_OK) != 0) {
        return ML_ERROR_NOT_FOUND;
    }
    
    // Loading logic...
    
    return ML_SUCCESS;
}
```

### Performance Considerations

| Suggestion               | Description                                                 |
|:-------------------------|:------------------------------------------------------------|
| **Lazy Initialization**  | Mod initialization is done in init_mod, load_mod only loads |
| **Cache Results**        | Cache Mod metadata to avoid repeated parsing                |
| **Asynchronous Loading** | Large Mods can consider asynchronous loading                |
| **Log Level**            | Reduce debug logs in release versions                       |

---

## 🔗 Related Links

- [TEFPkg-Tool Official Repository](https://github.com/eternalfuture-e38299/TEFPkg-Tool)
- [PatchLib API Reference](./patchlib-en.md)
- [TEFPKG Format Documentation](./tefpkg-en.md)
- [Plugin Development Guide](./plugin-en.md)
- [Module Development Guide](./module-en.md)

---

*Happy ModLoader Development! 🚀📋✨*