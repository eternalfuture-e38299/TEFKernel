# 📦 TEFKernel Module Development Guide

> **⚠️ Core Positioning**: A Module is a **directly managed functional unit** in TEFKernel, with higher priority than ModLoader. Modules are typically used to implement game-level extensions such as texture pack loading, UI enhancements, sound replacement, etc. Modules have complete lifecycle management, support hot-reload, and can depend on low-level symbols provided by Plugins.

---

## 📑 Table of Contents

<details>
<summary><b>📖 Click to expand full table of contents</b></summary>

- [📖 Overview](#-overview)
  - [Core Responsibilities](#core-responsibilities)
  - [Relationship with Plugins/ModLoaders](#relationship-with-pluginsmodloaders)
  - [Lifecycle](#lifecycle)
- [🚀 Quick Start](#-quick-start)
  - [Minimal Module Example](#minimal-module-example)
  - [Build Commands](#build-commands)
- [📚 API Reference](#-api-reference)
  - [Module Information Structure](#module-information-structure)
  - [Operation Function Table](#operation-function-table)
  - [Module Entry Structure](#module-entry-structure)
  - [Mandatory Export Function](#mandatory-export-function)
  - [Using Plugin Symbols](#using-plugin-symbols)
- [💡 Complete Example: Texture Pack Loading Module](#-complete-example-texture-pack-loading-module)
- [📦 Packaging and Deployment](#-packaging-and-deployment)
  - [Directory Structure](#directory-structure)
  - [Using TEFPkg-Tool for Packaging](#using-tefpkg-tool-for-packaging)
  - [Deployment Configuration](#deployment-configuration)
  - [Module Priority Explanation](#module-priority-explanation)
- [🔄 Hot-Reload](#-hot-reload)
  - [Working Principle](#working-principle)
  - [Implementation Support](#implementation-support)
- [⚠️ Important Notes](#-important-notes)
  - [Dependency Management](#dependency-management)
  - [Resource Management](#resource-management)
  - [Thread Safety](#thread-safety)
- [🔗 Related Links](#-related-links)

</details>

---

## 📖 Overview

### Core Responsibilities

| Responsibility               | Description                                                                            |
|:-----------------------------|:---------------------------------------------------------------------------------------|
| **🎨 Game Extension**        | Implements game-facing features like texture packs, UI enhancements, sound replacement |
| **🔌 Use Plugin Symbols**    | Calls low-level APIs via symbols registered by Plugins                                 |
| **📦 Resource Management**   | Manages its own private and log directories                                            |
| **🔄 Hot-Reload Support**    | Implements `hot_reload` function to respond to runtime updates                         |
| **📋 Dependency Management** | Declares Plugin dependencies (`plugin_dependencies`)                                   |

### Relationship with Plugins/ModLoaders

| Type          | Positioning               | Dependency                          |
|:--------------|:--------------------------|:------------------------------------|
| **Plugin**    | Low-level symbol provider | Does not depend on other components |
| **Module**    | Game feature extension    | Can depend on Plugin symbols        |
| **ModLoader** | Mod loading container     | Can depend on Module functionality  |

> **Priority Order**: Plugin → Module → ModLoader (loading order)

### Lifecycle

```
Kernel loads .tefpkg
↓
memdl_open() loads dynamic library
↓
Call module_create() to get operation table
↓
Verify Plugin dependencies (plugin_dependencies)
↓
Call init_module() → Initialize module
↓
Module runs normally (during game runtime)
↓
(Optional) Hot-reload triggered → Call hot_reload()
↓
Call cleanup_module() on unload
↓
Unload dynamic library
```

---

## 🚀 Quick Start

### Minimal Module Example

```c
// minimal_module.c
#include "module/module_core.h"
#include <stdio.h>

// ============================================================
// 1. Module Information (Static Constant)
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
// 2. Lifecycle Functions
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
// 3. Operation Table (Static Constant)
// ============================================================
static const module_ops_t g_ops = {
    .init_module = init,
    .cleanup_module = cleanup,
    .hot_reload = hot_reload,
    .get_info = get_info
};

// ============================================================
// 4. Mandatory Export
// ============================================================
API_EXPORT const module_ops_t* API_CALL module_create(void) {
    return &g_ops;
}
```

### Build Commands

```bash
# Linux / Android
gcc -shared -fPIC -I/path/to/tefkernel minimal_module.c -o libminimal_module.so

# Windows (MinGW)
gcc -shared -I/path/to/tefkernel minimal_module.c -o minimal_module.dll

# macOS
gcc -shared -fPIC -I/path/to/tefkernel minimal_module.c -o libminimal_module.dylib
```

---

## 📚 API Reference

### Module Information Structure

```c
typedef struct {
    const char *pkg_id;                    ///< Unique identifier (reverse domain)
    const char *name;                      ///< Display name
    const char *author;                    ///< Author
    const char *version;                   ///< Version string
    int version_code;                      ///< Version code (numeric comparison)
    int api_version;                       ///< API version number
    int plugin_dependencies_sizes;         ///< Number of dependent Plugins
    const char **plugin_dependencies;      ///< List of dependent Plugin pkg_ids
} module_info_t;
```

### Operation Function Table

```c
typedef struct {
    /**
     * @brief Initialize the module
     * @param entry Module entry (contains directory, package handle, etc.)
     * @return true=success, false=failure
     */
    bool (*init_module)(module_entry_t *entry);

    /**
     * @brief Clean up and close the module (called before unload)
     * @param entry Module entry
     * @return true=success, false=failure
     */
    bool (*cleanup_module)(module_entry_t *entry);

    /**
     * @brief Hot-reload operation (runtime update)
     * @param entry Module entry
     */
    void (*hot_reload)(module_entry_t *entry);

    /**
     * @brief Get module information
     * @return Pointer to static information
     */
    const module_info_t *(*get_info)(void);
} module_ops_t;
```

### Module Entry Structure

```c
typedef struct module_entry_t {
    module_info_t *info;        ///< Module information (filled by kernel)
    module_ops_t *ops;          ///< Operation table (provided by module)
    tefpkg_t *pkg_handle;       ///< Package handle (filled by kernel)
    const char *private_dir;    ///< Private directory path (for configuration, etc.)
    const char *logs_dir;       ///< Log directory path (independent logs)
} module_entry_t;
```

### Mandatory Export Function

```c
/**
 * @brief The only function that a module must export
 * @return Pointer to the module operation function table (must be static memory)
 */
API_EXPORT const module_ops_t * API_CALL module_create(void);
```

### Using Plugin Symbols

```c
#include "patchlib/type.h"
#include "patchlib/method.h"

// Assume a Plugin registered the symbol: get_game_version()
// How to obtain it (used in a Module):
typedef int (*get_game_version_t)(void);

// Get function pointer by symbol name
// Note: The actual retrieval method is provided by TEFKernel, this shows the concept
get_game_version_t get_version = (get_game_version_t)tpf_get_symbol("get_game_version");
if (get_version) {
    int version = get_version();
    printf("Game version: %d\n", version);
}
```

---

## 💡 Complete Example: Texture Pack Loading Module

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
// 1. Module Information (Depends on a low-level Plugin)
// ============================================================
static const char* deps[] = {
    "com.example.texture_api"  // Depends on texture processing Plugin
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
// 2. Module Private State
// ============================================================
typedef struct {
    char pack_dir[512];
    tefstd_vector_t loaded_textures;
    int total_packs;
} module_state_t;

static module_state_t g_state = {0};

// ============================================================
// 3. Function: Scan and Load Texture Packs
// ============================================================
static int scan_texture_packs(const char* pack_dir) {
    DIR* dir = opendir(pack_dir);
    if (!dir) return 0;
    
    int count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            // Check if it's a valid texture pack directory
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
// 4. Hook: Inject Custom Textures When Game Loads Textures
// ============================================================
static void texture_load_postfix(patch_handle_t instance, void** args, void* result,
                                  const patch_method_signature_t* sig_info) {
    // Assume args[0] is the texture path, result is the Texture2D object
    if (args && args[0]) {
        const char* path = *(const char**)args[0];
        if (path && strstr(path, ".png")) {
            // Here you can use the texture loading function provided by the Plugin
            // Example: load_custom_texture(path, result);
        }
    }
}

// ============================================================
// 5. Module Lifecycle
// ============================================================

// Initialize
static bool init_module(module_entry_t* entry) {
    printf("[Module] Initializing Texture Pack Loader...\n");
    
    // 1. Save private directory
    snprintf(g_state.pack_dir, sizeof(g_state.pack_dir), "%s/packs", 
             entry->private_dir);
    
    // 2. Create private directory
    mkdir(g_state.pack_dir, 0755);
    mkdir(entry->logs_dir, 0755);
    
    // 3. Scan texture packs
    g_state.total_packs = scan_texture_packs(g_state.pack_dir);
    printf("[Module] Found %d texture packs\n", g_state.total_packs);
    
    // 4. Get game texture loading method and Hook it
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
    
    // 5. Initialize vector
    tefstd_vector_init(&g_state.loaded_textures, sizeof(char*));
    
    return true;
}

// Cleanup
static bool cleanup_module(module_entry_t* entry) {
    printf("[Module] Cleaning up Texture Pack Loader...\n");
    
    // Free loaded texture list
    for (size_t i = 0; i < tefstd_vector_size(&g_state.loaded_textures); i++) {
        char** ptr = tefstd_vector_at(&g_state.loaded_textures, i);
        if (ptr && *ptr) free(*ptr);
    }
    tefstd_vector_destroy(&g_state.loaded_textures);
    
    return true;
}

// Hot-reload: Re-scan texture packs
static void hot_reload(module_entry_t* entry) {
    printf("[Module] Hot reload: Re-scanning texture packs...\n");
    
    // Clear old list
    for (size_t i = 0; i < tefstd_vector_size(&g_state.loaded_textures); i++) {
        char** ptr = tefstd_vector_at(&g_state.loaded_textures, i);
        if (ptr && *ptr) free(*ptr);
    }
    tefstd_vector_clear(&g_state.loaded_textures);
    
    // Re-scan
    g_state.total_packs = scan_texture_packs(g_state.pack_dir);
    printf("[Module] Found %d texture packs (after reload)\n", g_state.total_packs);
}

static const module_info_t* get_info(void) {
    return &g_info;
}

// ============================================================
// 6. Operation Table and Export
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

## 📦 Packaging and Deployment

### Directory Structure

```
Working Directory/
├── module/
│   ├── enables.txt          # List of enabled modules (one pkg_id per line)
│   └── pkg/
│       └── com.example.texture_pack.tefpkg
│
└── mods/                    # Module runtime directory (automatically created by kernel)
    └── com.example.texture_pack/
        ├── private/          # Private data directory (module_entry_t->private_dir)
        ├── logs/             # Log directory (module_entry_t->logs_dir)
        └── packs/            # Texture pack storage directory (custom in the example)
```

### Using TEFPkg-Tool for Packaging

> **Official Tool**: https://github.com/eternalfuture-e38299/TEFPkg-Tool

#### Dynamic Library Naming Constraints

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

#### Executing Packaging

```bash
# Download and build TEFPkg-Tool
git clone https://github.com/eternalfuture-e38299/TEFPkg-Tool.git
cd TEFPkg-Tool
mkdir build && cd build
cmake .. && make

# Package the module
./tefpkg_tool build <dir> <outfile> <fingerprint>
```

### Deployment Configuration

#### enables.txt

* One module ID per line (without the .tefpkg extension)

```text
com.example.texture_pack
com.example.ui_enhance
com.example.audio_engine
```

#### Deployment Commands

```bash
# Create directories
mkdir -p workspace/module/pkg

# Copy package files
cp com.example.texture_pack.tefpkg workspace/module/pkg/

# Add to enable list
echo "com.example.texture_pack" >> workspace/module/enables.txt

# Start the game, the kernel will automatically load
```

### Module Priority Explanation

Loading order (from highest to lowest):
1. **Plugin** (loads symbols first)
2. **Module** (uses Plugin symbols)
3. **ModLoader** (uses Module and Plugin)

---

## 🔄 Hot-Reload

### Working Principle

```
Hot-reload triggered (file modification detected / manual invocation)
       ↓
Kernel calls hot_reload() on all modules
       ↓
Modules reload configurations/resources
       ↓
Takes effect without restarting the game
```

### Implementation Support

```c
static void hot_reload(module_entry_t* entry) {
    // 1. Reload configuration file
    reload_config(entry->private_dir);
    
    // 2. Re-scan resource directory
    scan_resources();
    
    // 3. Refresh game state (if needed)
    refresh_game_state();
}
```

---

## ⚠️ Important Notes

### Dependency Management

```c
// ✅ Correct: Declare dependencies
static const char* deps[] = {
    "com.example.texture_api",
    "com.example.file_system"
};
static const module_info_t g_info = {
    .plugin_dependencies_sizes = 2,
    .plugin_dependencies = deps
};

// ❌ Incorrect: Declare non-existent Plugin dependency
static const char* deps[] = {"non.existent.plugin"};  // Loading will fail

// ✅ Correct: No dependencies
static const module_info_t g_info = {
    .plugin_dependencies_sizes = 0,
    .plugin_dependencies = NULL
};
```

### Resource Management

```c
// ✅ Correct: Release resources in cleanup
static bool cleanup_module(module_entry_t* entry) {
    // Free dynamically allocated memory
    free(g_state.buffer);
    
    // Close file handles
    if (g_state.file) fclose(g_state.file);
    
    return true;
}

// ✅ Correct: Clean up old state in hot_reload
static void hot_reload(module_entry_t* entry) {
    // Clean up old data
    cleanup_old_state();
    // Load new data
    load_new_state();
}
```

### Thread Safety

| Rule                       | Description                                                  |
|:---------------------------|:-------------------------------------------------------------|
| **Main Thread Operations** | All module operations should be completed on the main thread |
| **Avoid Blocking**         | init/hot_reload should not take too long                     |
| **Log Usage**              | Use entry->logs_dir for independent logging                  |

---

## 🔗 Related Links

- [TEFPkg-Tool Official Repository](https://github.com/eternalfuture-e38299/TEFPkg-Tool)
- [PatchLib API Reference](./patchlib-en.md)
- [TEFPKG Format Documentation](./tefpkg-en.md)
- [Plugin Development Guide](./plugin-en.md)
- [ModLoader Development Guide](./modloader-en.md)

---

*Happy Module Development! 🚀📦✨*