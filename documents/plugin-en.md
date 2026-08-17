# 🔌 TEFKernel Plugin Development Guide

> **⚠️ Core Positioning**: A Plugin is a **pure symbol provider** in the TEFKernel architecture, dedicated to registering function/variable symbols for Modules and ModLoaders. Plugins do not implement business logic or participate in game interactions; their entire value lies in exporting low-level APIs for use by other components.

---

## 📑 Table of Contents

<details>
<summary><b>📖 Click to expand full table of contents</b></summary>

- [📖 Overview](#-overview)
  - [Core Responsibilities](#core-responsibilities)
  - [Lifecycle](#lifecycle)
- [🚀 Quick Start](#-quick-start)
  - [Minimal Plugin Example](#minimal-plugin-example)
  - [Build Commands](#build-commands)
- [📚 API Reference](#-api-reference)
  - [Plugin Information Structure](#plugin-information-structure)
  - [Operation Function Table](#operation-function-table)
  - [Mandatory Export Function](#mandatory-export-function)
  - [Symbol Registration API](#symbol-registration-api)
- [💡 Complete Example: Symbol Provider](#-complete-example-symbol-provider)
- [📦 Packaging and Deployment](#-packaging-and-deployment)
  - [Directory Structure](#directory-structure)
  - [Using TEFPkg-Tool for Packaging](#using-tefpkg-tool-for-packaging)
  - [Deployment Configuration](#deployment-configuration)
- [⚠️ Important Notes](#-important-notes)
  - [Symbol Registration Rules](#symbol-registration-rules)
  - [Memory Management](#memory-management)
  - [Thread Safety](#thread-safety)
- [🔗 Related Links](#-related-links)

</details>

---

## 📖 Overview

### Core Responsibilities

| Responsibility             | Description                                                                |
|:---------------------------|:---------------------------------------------------------------------------|
| **📦 Symbol Registration** | Registers functions/variables via `TPF_SYMBOL()` in `initialize`           |
| **🔌 API Provision**       | Provides low-level function implementations for Modules/ModLoaders to call |
| **🔄 Version Management**  | Manages API compatibility via `version_code`                               |
| **🧹 Resource Cleanup**    | Releases resources allocated by the plugin itself in `cleanup`             |

### Lifecycle

```
Kernel loads .tefpkg
↓
memdl_open() loads dynamic library
↓
Call tpf_create_plugin() to get operation table
↓
Call initialize() → Register all symbols
↓
Symbols are available for Modules/ModLoaders
↓
(During game runtime)
↓
Call cleanup() → Release resources
↓
Unload dynamic library
```

---

## 🚀 Quick Start

### Minimal Plugin Example

```c
// minimal_plugin.c
#include "tefplugin/tpf_core.h"
#include <stdio.h>

// ============================================================
// 1. Plugin Information (Static Constant)
// ============================================================
static const tpf_plugin_info_t g_info = {
    .pkg_id = "com.example.minimal",
    .name = "Minimal Plugin",
    .author = "Your Name",
    .version = "1.0.0",
    .version_code = 1
};

// ============================================================
// 2. Symbol Function to Register
// ============================================================
int get_answer(void) {
    return 42;
}

// ============================================================
// 3. Lifecycle Functions
// ============================================================
static bool init(plugin_handle_t* handle) {
    TPF_SYMBOL(get_answer);  // Register symbol
    return true;
}

static void cleanup(plugin_handle_t* handle) {
    // No resources to release
}

static const tpf_plugin_info_t* get_info(void) {
    return &g_info;
}

// ============================================================
// 4. Operation Table (Static Constant)
// ============================================================
static const tpf_plugin_ops_t g_ops = {
    .initialize = init,
    .cleanup = cleanup,
    .get_info = get_info
};

// ============================================================
// 5. Mandatory Export
// ============================================================
API_EXPORT const tpf_plugin_ops_t* API_CALL tpf_create_plugin(void) {
    return &g_ops;
}
```

### Build Commands

```bash
# Linux / Android
gcc -shared -fPIC -I/path/to/tefkernel minimal_plugin.c -o libminimal.so

# Windows (MinGW)
gcc -shared -I/path/to/tefkernel minimal_plugin.c -o minimal.dll

# macOS
gcc -shared -fPIC -I/path/to/tefkernel minimal_plugin.c -o libminimal.dylib
```

---

## 📚 API Reference

### Plugin Information Structure

```c
typedef struct {
    const char *pkg_id;     ///< Unique plugin identifier (reverse domain format)
    const char *name;       ///< Plugin display name
    const char *author;     ///< Author
    const char *version;    ///< Version string
    int version_code;       ///< Version code (numeric comparison)
} tpf_plugin_info_t;
```

### Operation Function Table

```c
typedef struct {
    /**
     * @brief Initialize the plugin (the only opportunity to register symbols)
     * @param this_handle Plugin handle (passed by the kernel)
     * @return true=success, false=failure (causes plugin unload)
     */
    bool (*initialize)(plugin_handle_t *this_handle);

    /**
     * @brief Clean up the plugin (called before unload)
     * @param this_handle Plugin handle
     */
    void (*cleanup)(plugin_handle_t *this_handle);

    /**
     * @brief Get plugin information
     * @return Pointer to static information
     */
    const tpf_plugin_info_t *(*get_info)(void);
} tpf_plugin_ops_t;
```

### Mandatory Export Function

```c
/**
 * @brief The only function that a plugin must export
 * @return Pointer to the plugin operation function table (must be static memory)
 * @warning Must not dynamically allocate memory for the return value
 */
API_EXPORT const tpf_plugin_ops_t * API_CALL tpf_create_plugin(void);
```

### Symbol Registration API

```c
/**
 * @brief Register a plugin symbol (low-level function)
 * @param this_handle Current plugin handle
 * @param name Symbol name (string)
 * @param addr Symbol address (function or variable pointer)
 * @return true=success, false=failure
 */
DEFINE_FUNCTION(bool, tpf_register_symbol,
                plugin_handle_t* this_handle,
                const char *name,
                const void *addr);

/**
 * @brief Convenience macro for registering symbols
 * @param func Function name (automatically uses #func as the symbol name)
 * @example TPF_SYMBOL(my_function);
 */
#define TPF_SYMBOL(func) \
    tpf_register_symbol(this_handle, #func, (const void *)(func))
```

> **⚠️ Important**: All symbol registration must be done exclusively inside the `initialize` function.

---

## 💡 Complete Example: Symbol Provider

```c
// symbol_provider.c
#include "tefplugin/tpf_core.h"
#include "patchlib/type.h"
#include "patchlib/field.h"
#include <stdio.h>
#include <string.h>

// ============================================================
// 1. Plugin Information
// ============================================================
static const tpf_plugin_info_t g_plugin_info = {
    .pkg_id = "com.example.symbol_provider",
    .name = "Terraria API Provider",
    .author = "TEFKernel Team",
    .version = "2.0.0",
    .version_code = 200
};

// ============================================================
// 2. Symbol Functions to Export
// ============================================================

// Symbol 1: Get game version
int get_game_version(void) {
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    if (!patchlib_is_valid(main_type)) return -1;
    
    patch_handle_t field = patchlib_type_get_field(main_type, "curRelease");
    int version = 0;
    patchlib_field_get_value(field, PATCH_NULL, &version);
    return version;
}

// Symbol 2: Get player health
int get_player_life(void) {
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    patch_handle_t player_field = patchlib_type_get_field(main_type, "player");
    patch_handle_t player = PATCH_NULL;
    patchlib_field_get_value(player_field, PATCH_NULL, &player);
    
    if (!patchlib_is_valid(player)) return -1;
    
    patch_handle_t player_type = patchlib_type_get_type("Terraria", "Player");
    patch_handle_t life_field = patchlib_type_get_field(player_type, "statLife");
    int life = 0;
    patchlib_field_get_value(life_field, player, &life);
    return life;
}

// Symbol 3: Set player health (returns success status)
bool set_player_life(int new_life) {
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    patch_handle_t player_field = patchlib_type_get_field(main_type, "player");
    patch_handle_t player = PATCH_NULL;
    patchlib_field_get_value(player_field, PATCH_NULL, &player);
    
    if (!patchlib_is_valid(player)) return false;
    
    patch_handle_t player_type = patchlib_type_get_type("Terraria", "Player");
    patch_handle_t life_field = patchlib_type_get_field(player_type, "statLife");
    patchlib_field_set_value(life_field, player, &new_life);
    return true;
}

// Symbol 4: API version constant
const int MOD_API_VERSION = 2;

// Symbol 5: Message formatting function
const char* format_message(const char* template, int value) {
    static char buffer[256];
    snprintf(buffer, sizeof(buffer), template, value);
    return buffer;
}

// ============================================================
// 3. Plugin Lifecycle
// ============================================================

static bool my_plugin_initialize(plugin_handle_t* this_handle) {
    printf("[Plugin] Registering symbols...\n");
    
    // Register all exported symbols
    TPF_SYMBOL(get_game_version);
    TPF_SYMBOL(get_player_life);
    TPF_SYMBOL(set_player_life);
    TPF_SYMBOL(MOD_API_VERSION);
    TPF_SYMBOL(format_message);
    
    printf("[Plugin] All symbols registered successfully!\n");
    return true;
}

static void my_plugin_cleanup(plugin_handle_t* this_handle) {
    // Plugin doesn't need to clean up other resources (symbols are managed by kernel)
    printf("[Plugin] Cleanup complete.\n");
}

static const tpf_plugin_info_t* my_plugin_get_info(void) {
    return &g_plugin_info;
}

// ============================================================
// 4. Operation Table
// ============================================================
static const tpf_plugin_ops_t g_plugin_ops = {
    .initialize = my_plugin_initialize,
    .cleanup = my_plugin_cleanup,
    .get_info = my_plugin_get_info
};

// ============================================================
// 5. Mandatory Export
// ============================================================
API_EXPORT const tpf_plugin_ops_t* API_CALL tpf_create_plugin(void) {
    return &g_plugin_ops;
}
```

---

## 📦 Packaging and Deployment

### Directory Structure

```
Working Directory/
├── plugin/
│   ├── enables.txt          # List of enabled plugins (one pkg_id per line)
│   └── pkg/
│       └── com.example.symbol_provider.tefpkg
```

### Using TEFPkg-Tool for Packaging

> **Official Tool**: https://github.com/eternalfuture-e38299/TEFPkg-Tool

#### Dynamic Library Naming Constraints

```text
libplugin.android.arm64.so
libplugin.android.arm.so
libplugin.linux.x64.so
libplugin.linux.x86.so
plugin.windows.x64.dll
plugin.windows.x86.dll
libplugin.mac.arm64.dylib
libplugin.mac.x64.dylib
libplugin.ios.arm64.dylib
libplugin.ios.x64.dylib
libplugin.ios.arm64-simulator.dylib
```

#### Executing Packaging

```bash
# Download and build TEFPkg-Tool
git clone https://github.com/eternalfuture-e38299/TEFPkg-Tool.git
cd TEFPkg-Tool
mkdir build && cd build
cmake .. && make

# Package the plugin
./tefpkg_tool build <dir> <outfile> <fingerprint>
```

### Deployment Configuration

#### enables.txt

* One plugin ID per line (without the .tefpkg extension)
* Generally does not need to be manually enabled; TEFKernel automatically loads the corresponding dependencies
```text
com.example.symbol_provider
com.example.another_plugin
```

#### Deployment Commands

```bash
# Create directories
mkdir -p workspace/plugin/pkg

# Copy package files
cp com.example.symbol_provider.tefpkg workspace/plugin/pkg/

# Add to enable list
echo "com.example.symbol_provider" >> workspace/plugin/enables.txt
```

---

## ⚠️ Important Notes

### Symbol Registration Rules

```c
// ✅ Correct: Register in initialize
static bool init(plugin_handle_t* handle) {
    TPF_SYMBOL(my_function);
    TPF_SYMBOL(my_data);
    return true;
}

// ❌ Incorrect: Register outside a function (won't take effect)
TPF_SYMBOL(my_function);  // Compilation error or runtime invalid

// ❌ Incorrect: Registering a null pointer
TPF_SYMBOL(NULL);  // Invalid

// ✅ Correct: Can register variable addresses
int global_counter = 0;
TPF_SYMBOL(global_counter);
```

### Memory Management

```c
// ✅ Correct: Use static memory
static const tpf_plugin_info_t g_info = {...};
static const tpf_plugin_ops_t g_ops = {...};
static char g_buffer[256];

// ❌ Incorrect: Dynamically allocate memory for return (causes memory leak)
const tpf_plugin_ops_t* tpf_create_plugin(void) {
    tpf_plugin_ops_t* ops = malloc(sizeof(tpf_plugin_ops_t));  // Dangerous!
    return ops;
}

// ✅ Correct: Return static memory address
const tpf_plugin_ops_t* tpf_create_plugin(void) {
    return &g_ops;  // Safe
}
```

### Thread Safety

| Rule                                  | Description                                                        |
|:--------------------------------------|:-------------------------------------------------------------------|
| **Main Thread Operations**            | All plugin-related operations must be on the main thread           |
| **Serial Initialization**             | Plugin loading, initialization, and symbol registration are serial |
| **No Concurrent Symbol Registration** | Cannot register symbols while symbol iteration is in progress      |
| **Cleanup Timing**                    | cleanup is called when the plugin is no longer in use              |

---

## 🔗 Related Links

- [TEFPkg-Tool Official Repository](https://github.com/eternalfuture-e38299/TEFPkg-Tool)
- [PatchLib API Reference](./patchlib-en.md)
- [TEFPKG Format Documentation](./tefpkg-en.md)
- [ModLoader Development Guide](./modloader-en.md)
- [Module Development Guide](./module-en.md)
---

*Happy Plugin Development! 🚀✨*