# 📚 PatchLib User Guide

> **Quick Navigation**: Click the links below to jump to the corresponding sections

## 📑 Table of Contents

<details>
<summary><b>📖 Click to expand full table of contents</b></summary>

- [🚀 Quick Start](#-quick-start) - Includes headers + basic usage workflow

#### Type System (type.h)
- [📦 Type System](#-type-system-typeh)
  - [Getting Types](#getting-types)
  - [Type Information Queries](#type-information-queries)
  - [Batch Member Retrieval](#batch-member-retrieval)

#### Method Operations (method.h)
- [⚡ Method Operations](#-method-operations-methodh)
  - [Getting Methods](#getting-methods)
  - [Method Information Queries](#method-information-queries)
  - [Invoking Methods](#invoking-methods)
  - [Generic Methods](#generic-methods)

#### Hook System
- [🪝 Hook System](#-hook-system)
  - [Prefix/Postfix Hook](#prefixpostfix-hook)
  - [Skip Original Method Hook](#skip-original-method-hook)

#### Field Operations (field.h)
- [📊 Field Operations](#-field-operations-fieldh)

#### Property Operations (property.h)
- [🏷️ Property Operations](#-property-operations-propertyh)

#### Data Structure Operations
- [🏗️ Data Structure Operations](#-data-structure-operations)
  - [Array](#array)
  - [List](#list)
  - [Dictionary](#dictionary)
  - [String](#string)

#### Thread Operations (thread.h)
- [🧵 Thread Operations](#-thread-operations-threadh)

#### Practice and Reference
- [💡 Complete Example](#-complete-example-typical-usage-in-mod-development)
- [⚠ Important Notes](#-important-notes)

</details>

## 🚀 Quick Start

### 1. Include Headers

```c
// Only include the headers you need
#include "patchlib/type.h"      // Type system
#include "patchlib/method.h"    // Method operations
#include "patchlib/field.h"     // Field operations
#include "patchlib/property.h"  // Property operations

// Data structure operations
#include "patchlib/struct/array.h"
#include "patchlib/struct/list.h"
#include "patchlib/struct/dictionary.h"
#include "patchlib/struct/string.h"

// Thread operations (Android)
#include "patchlib/thread.h"
```

### 2. Basic Usage Workflow

```c
// 1. Get type
patch_handle_t type = patchlib_type_get_type("Terraria", "Main");

// 2. Get method
patch_handle_t method = patchlib_type_get_method_by_param_count(type, "Initialize", 0);

// 3. Invoke method
patchlib_method_invoke_args(method, PATCH_NULL, NULL, NULL);

// 4. Clean up resources (Desktop)
#ifndef __ANDROID__
patchlib_free(type);
patchlib_free(method);
#endif
```

**[⬆ Back to top](#-patchlib-user-guide)**

---

## 📦 Type System (type.h)

### Getting Types

```c
// Basic types
patch_handle_t int_type = patchlib_get_basic_type(PATCH_INT32);
patch_handle_t float_type = patchlib_get_basic_type(PATCH_FLOAT);
patch_handle_t string_type = patchlib_get_basic_type(PATCH_OBJECT);

// Game types
patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
patch_handle_t player_type = patchlib_type_get_type("Terraria", "Player");
patch_handle_t npc_type = patchlib_type_get_type("Terraria", "NPC");

// Nested types
patch_handle_t inner_type = patchlib_type_get_inner_type(main_type, "MyInnerClass");

// Generic types (e.g., List<string>)
patch_handle_t list_type_def = patchlib_type_get_type("System.Collections.Generic", "List`1");
tefstd_vector_t type_args;
tefstd_vector_init(&type_args, sizeof(patch_handle_t));
patch_handle_t string_type = patchlib_get_basic_type(PATCH_OBJECT);
tefstd_vector_push_back(&type_args, &string_type);
patch_handle_t list_string_type = patchlib_type_make_generic_type(list_type_def, &type_args);
tefstd_vector_destroy(&type_args);
```

### Type Information Queries

```c
// Get type name
const char* name = patchlib_type_get_name(main_type);
printf("Type name: %s\n", name);

// Get namespace
const char* ns = patchlib_type_get_namespace(main_type);
printf("Namespace: %s\n", ns);

// Get full name (needs free)
char* full_name = patchlib_type_get_full_name(main_type);
printf("Full name: %s\n", full_name);
free(full_name);

// Get parent type
patch_handle_t parent = patchlib_type_get_parent(player_type);
if (patchlib_is_valid(parent)) {
    const char* parent_name = patchlib_type_get_name(parent);
    printf("Parent: %s\n", parent_name);
}

// Create instance
patch_handle_t instance = patchlib_type_new_instance(player_type);
```

### Batch Member Retrieval

```c
tefstd_vector_t methods;
tefstd_vector_t fields;
tefstd_vector_t properties;

// Get all methods (including parent)
patchlib_type_get_methods(main_type, true, &methods);
patchlib_type_get_fields(main_type, true, &fields);
patchlib_type_get_properties(main_type, true, &properties);

// Iterate methods
size_t count = tefstd_vector_size(&methods);
for (size_t i = 0; i < count; i++) {
    patch_handle_t* method_ptr = tefstd_vector_at(&methods, i);
    const char* method_name = patchlib_method_get_name(*method_ptr);
    printf("Method: %s\n", method_name);
}

// Clean up resources
tefstd_vector_destroy(&methods);
tefstd_vector_destroy(&fields);
tefstd_vector_destroy(&properties);
```

**[⬆ Back to top](#-patchlib-user-guide)**

---

## ⚡ Method Operations (method.h)

### Getting Methods

```c
// 1. Get by name (may have overloads)
patch_handle_t method = patchlib_type_get_method(main_type, "Initialize");

// 2. Get by name + parameter count (recommended)
patch_handle_t init_method = patchlib_type_get_method_by_param_count(
    main_type, "Initialize", 0
);

// 3. Get by name + parameter types (most precise)
patch_handle_t string_type = patchlib_get_basic_type(PATCH_OBJECT);
patch_handle_t method = patchlib_type_get_method_by_param_types(
    main_type, 
    "SomeMethod", 
    1, 
    &string_type
);

// 4. Get by signature
const char* arg_names[] = {"message"};
patch_handle_t method = patchlib_type_get_method_by_signature(
    main_type,
    "PrintMessage",
    1,
    &string_type,
    arg_names
);
```

### Method Information Queries

```c
// Get method name
const char* name = patchlib_method_get_name(method);

// Get parameter count
int param_count = patchlib_method_get_param_count(method);

// Check method type
bool is_static = patchlib_method_is_static(method);
bool is_instance = patchlib_method_is_instance(method);

// Get return type
patch_type_t return_type = patchlib_method_get_return_type(method);

// Get method token
int token = patchlib_method_get_token(method);
```

### Invoking Methods

```c
// Invoke static method (no parameters)
patch_handle_t method = patchlib_type_get_method_by_param_count(
    main_type, "Initialize", 0
);
patchlib_method_invoke_args(method, PATCH_NULL, NULL, NULL);

// Invoke static method (with parameters)
patch_handle_t method = patchlib_type_get_method_by_param_types(
    main_type, "PrintMessage", 1, &string_type
);
const char* msg = "Hello World";
void* args[] = { (void*)&msg };
patchlib_method_invoke_args(method, PATCH_NULL, NULL, args);

// Invoke instance method
patch_handle_t player = patchlib_type_new_instance(player_type);
patch_handle_t method = patchlib_type_get_method_by_param_count(
    player_type, "Update", 0
);
patchlib_method_invoke_args(method, player, NULL, NULL);

// Invoke method with return value
patch_handle_t method = patchlib_type_get_method_by_param_count(
    player_type, "GetLife", 0
);
int life = 0;
patchlib_method_invoke_args(method, player, &life, NULL);
printf("Life: %d\n", life);

// Invoke constructor
patch_handle_t constructor = patchlib_type_get_method_by_param_count(
    string_type, ".ctor", 1
);
patch_handle_t new_string = PATCH_NULL;
const char* text = "Hello";
void* args[] = { (void*)&text };
patchlib_constructor_invoke(constructor, &new_string, args);
```

### Generic Methods

```c
// Assume there is a generic method: T GetValue<T>()
patch_handle_t generic_method = patchlib_type_get_method(
    some_type, "GetValue"
);

// Prepare generic parameters
tefstd_vector_t template_types;
tefstd_vector_init(&template_types, sizeof(patch_handle_t));
patch_handle_t int_type = patchlib_get_basic_type(PATCH_INT32);
tefstd_vector_push_back(&template_types, &int_type);

// Instantiate generic method
patch_handle_t concrete_method = patchlib_method_make_generic_instance(
    generic_method, &template_types
);

tefstd_vector_destroy(&template_types);

// Invoke instantiated method
int result = 0;
patchlib_method_invoke_args(concrete_method, PATCH_NULL, &result, NULL);
```

**[⬆ Back to top](#-patchlib-user-guide)**

---

## 🪝 Hook System

### Prefix/Postfix Hook

```c
#include "patchlib/method.h"

// Prefix callback: Called before target method execution
// Return true to skip original method, false to continue execution
bool my_prefix(patch_handle_t instance, void** args, 
               const patch_method_signature_t* sig_info, void* result) {
    printf("Prefix: Method about to be called\n");
    
    // Modify parameters (e.g., first parameter)
    if (args && args[0]) {
        int* param = (int*)args[0];
        *param = 100;  // Modify parameter value
    }
    
    // Return false to continue executing original method
    return false;
}

// Postfix callback: Called after target method execution
void my_postfix(patch_handle_t instance, void** args, void* result,
                const patch_method_signature_t* sig_info) {
    printf("Postfix: Method executed\n");
    
    // Modify return value
    if (result) {
        int* ret = (int*)result;
        *ret = *ret * 2;  // Double return value
    }
}

// Install Hook
patch_handle_t target_method = ...;
patch_hook_id_t hook_id = patchlib_install_prepost_hook(
    target_method,
    my_prefix,   // Prefix callback
    my_postfix   // Postfix callback
);

if (hook_id != PATCH_HOOK_INVALID_ID) {
    printf("Hook installed successfully! ID: %d\n", hook_id);
}

// Uninstall Hook
if (patchlib_uninstall_hook(hook_id)) {
    printf("Hook uninstalled successfully\n");
}
```

### Skip Original Method Hook

```c
// Prefix returning true will skip original method
bool skip_prefix(patch_handle_t instance, void** args,
                 const patch_method_signature_t* sig_info, void* result) {
    printf("Skipping original method!\n");
    
    // Directly set return value
    if (result) {
        int* ret = (int*)result;
        *ret = 999;  // Custom return value
    }
    
    return true;  // Skip original method
}

patch_hook_id_t hook_id = patchlib_install_prepost_hook(
    target_method,
    skip_prefix,
    NULL  // No Postfix needed
);
```

**[⬆ Back to top](#-patchlib-user-guide)**

---

## 📊 Field Operations (field.h)

### Getting and Manipulating Fields

```c
// Get fields
patch_handle_t life_field = patchlib_type_get_field(player_type, "statLife");
patch_handle_t name_field = patchlib_type_get_field(player_type, "name");

// Get field information
const char* field_name = patchlib_field_get_name(life_field);
bool is_static = patchlib_field_is_static(life_field);
bool is_const = patchlib_field_is_const(life_field);
patch_type_t field_type = patchlib_field_get_type(life_field);
size_t field_size = patchlib_field_get_size(life_field);

// Read field value
patch_handle_t player_instance = ...;
int life = 0;
patchlib_field_get_value(life_field, player_instance, &life);
printf("Life: %d\n", life);

// Set field value
int new_life = 100;
patchlib_field_set_value(life_field, player_instance, &new_life);

// Static field operations
patch_handle_t static_field = patchlib_type_get_field(main_type, "SomeStaticField");
int static_value = 0;
patchlib_field_get_value(static_field, PATCH_NULL, &static_value);

// Android platform: Get pointer directly (high performance scenarios)
#if __ANDROID__
void* field_ptr = patchlib_field_get_pointer(life_field, player_instance);
if (field_ptr) {
    int* life_ptr = (int*)field_ptr;
    *life_ptr = 200;  // Direct memory modification
}
#endif
```

**[⬆ Back to top](#-patchlib-user-guide)**

---

## 🏷️ Property Operations (property.h)

### Getting and Manipulating Properties

```c
// Get property
patch_handle_t property = patchlib_type_get_property(player_type, "Life");

// Get property name
const char* prop_name = patchlib_property_get_name(property);

// Get getter/setter methods
patch_handle_t get_method = patchlib_property_get_get_method(property);
patch_handle_t set_method = patchlib_property_get_set_method(property);

// Read property value using getter
int life = 0;
patchlib_method_invoke_args(get_method, player_instance, &life, NULL);

// Set property value using setter
int new_life = 150;
void* args[] = { &new_life };
patchlib_method_invoke_args(set_method, player_instance, NULL, args);
```

**[⬆ Back to top](#-patchlib-user-guide)**

---

## 🏗️ Data Structure Operations

### Array

```c
#include "patchlib/struct/array.h"

// Create array
patch_handle_t int_type = patchlib_get_basic_type(PATCH_INT32);
patch_handle_t array = patchlib_array_create(10, int_type);

// Get array information
size_t length = patchlib_array_length(array);
bool empty = patchlib_array_empty(array);

// Read/Set elements
int value = 0;
patchlib_array_at(array, 0, &value);
printf("Array[0]: %d\n", value);

int new_value = 42;
patchlib_array_set(array, 0, &new_value);

// Fill array
int fill_value = 99;
patchlib_array_fill(array, &fill_value);

// Convert between C array and game array
int c_array[5] = {1, 2, 3, 4, 5};
patchlib_array_copy_from_c(array, c_array, 5);

int c_array_out[5];
patchlib_array_copy_to_c(c_array_out, array, 5);
```

### List

```c
#include "patchlib/struct/list.h"

// Create List<int>
patch_handle_t int_type = patchlib_get_basic_type(PATCH_INT32);
patch_handle_t list = patchlib_list_create(10, int_type);

// Add element
int value = 42;
patchlib_list_add(list, &value);

// Remove element
patchlib_list_remove_at(list, 0);

// Clear list
patchlib_list_clear(list);

// Copy from array
patch_handle_t array = patchlib_array_create(5, int_type);
patchlib_list_copy_from(list, array);

// Get internal array
patch_handle_t internal_array = patchlib_list_get_array(list);
size_t length = patchlib_array_length(internal_array);
```

### Dictionary

```c
#include "patchlib/struct/dictionary.h"

// Create Dictionary<string, int>
patch_handle_t string_type = patchlib_get_basic_type(PATCH_OBJECT);
patch_handle_t int_type = patchlib_get_basic_type(PATCH_INT32);
patch_handle_t dict = patchlib_dictionary_create(string_type, int_type, 10);

// Add key-value pair
const char* key = "score";
int value = 100;
patchlib_dictionary_add(dict, (void*)&key, &value);

// Get value
int out_value = 0;
patchlib_dictionary_get_value(dict, (void*)&key, &out_value);
printf("score = %d\n", out_value);

// Modify value
int new_value = 200;
patchlib_dictionary_set_value(dict, (void*)&key, &new_value);

// Remove key-value pair
patchlib_dictionary_remove(dict, (void*)&key);

// Get length
size_t length = patchlib_dictionary_length(dict);
```

### String

```c
#include "patchlib/struct/string.h"

// Create string
patch_handle_t str = patchlib_string_create("Hello World");

// Convert back to C string (needs free)
char* c_str = patchlib_string_cstr(str);
printf("String: %s\n", c_str);
free(c_str);

// Check if empty
bool empty = patchlib_string_empty(str);

// Get length
size_t len = patchlib_string_length(str);
```

**[⬆ Back to top](#-patchlib-user-guide)**

---

## 🧵 Thread Operations (thread.h)

```c
#include "patchlib/thread.h"

// Only effective on Android platform
#if __ANDROID__
// Get current thread handle
patch_handle_t thread = patchlib_thread_current();

// Attach current thread to runtime (use when calling from child thread)
patch_handle_t attached = patchlib_thread_attach();

// Detach from runtime
patchlib_thread_detach(attached);
#endif
```

**[⬆ Back to top](#-patchlib-user-guide)**

---

## 💡 Complete Example: Typical Usage in Mod Development

```c
#include "patchlib/type.h"
#include "patchlib/method.h"
#include "patchlib/field.h"
#include "patchlib/struct/list.h"
#include "patchlib/struct/string.h"

// Global variables
static patch_hook_id_t g_hook_id = PATCH_HOOK_INVALID_ID;
static patch_handle_t g_player_type = PATCH_NULL;
static patch_handle_t g_life_field = PATCH_NULL;

// ============ Hook Callback Functions ============

// Damage handling Postfix
void damage_postfix(patch_handle_t instance, void** args, void* result,
                    const patch_method_signature_t* sig_info) {
    // Get damage value (second parameter)
    if (args && args[1]) {
        int* damage = (int*)args[1];
        printf("[Mod] Player took %d damage\n", *damage);
        
        // Halve damage (demonstration)
        *damage /= 2;
        printf("[Mod] Damage reduced to %d\n", *damage);
    }
}

// ============ Game Initialization Callback ============

void on_game_initialized() {
    printf("[Mod] Game initialized!\n");
    
    // 1. Get type
    g_player_type = patchlib_type_get_type("Terraria", "Player");
    if (!patchlib_is_valid(g_player_type)) {
        printf("[Mod] Failed to get Player type!\n");
        return;
    }
    
    // 2. Get Main type and get current player
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    if (!patchlib_is_valid(main_type)) {
        printf("[Mod] Failed to get Main type!\n");
        return;
    }
    
    patch_handle_t player_field = patchlib_type_get_field(main_type, "player");
    if (!patchlib_is_valid(player_field)) {
        printf("[Mod] Failed to get player field!\n");
        return;
    }
    
    // 3. Read player instance
    patch_handle_t player = PATCH_NULL;
    patchlib_field_get_value(player_field, PATCH_NULL, &player);
    
    if (!patchlib_is_valid(player)) {
        printf("[Mod] No player instance yet!\n");
        return;
    }
    
    // 4. Get player life field
    g_life_field = patchlib_type_get_field(g_player_type, "statLife");
    if (!patchlib_is_valid(g_life_field)) {
        printf("[Mod] Failed to get statLife field!\n");
        return;
    }
    
    int life = 0;
    patchlib_field_get_value(g_life_field, player, &life);
    printf("[Mod] Current life: %d\n", life);
    
    // 5. Create Hook to monitor player damage
    patch_handle_t hurt_method = patchlib_type_get_method_by_param_count(
        g_player_type, "Hurt", 2
    );
    
    if (patchlib_is_valid(hurt_method)) {
        g_hook_id = patchlib_install_prepost_hook(
            hurt_method,
            NULL,           // No Prefix needed
            damage_postfix  // Postfix handling
        );
        
        if (g_hook_id != PATCH_HOOK_INVALID_ID) {
            printf("[Mod] Hook installed! ID: %d\n", g_hook_id);
        }
    }
}

// ============ Mod Initialization Entry ============

bool my_mod_init(module_entry_t* entry) {
    printf("[Mod] Initializing MyMod...\n");
    
    // Find game initialization method
    patch_handle_t main_type = patchlib_type_get_type("Terraria", "Main");
    patch_handle_t init_method = patchlib_type_get_method_by_param_count(
        main_type, "Initialize", 0
    );
    
    if (patchlib_is_valid(init_method)) {
        // Hook game initialization, execute our logic after game initialization completes
        patchlib_install_prepost_hook(init_method, NULL, 
            (postfix_callback_t)on_game_initialized
        );
        printf("[Mod] Game init hook installed!\n");
    }
    
    return true;
}

// ============ Mod Cleanup ============

void my_mod_cleanup(module_entry_t* entry) {
    printf("[Mod] Cleaning up MyMod...\n");
    
    // Uninstall Hook
    if (g_hook_id != PATCH_HOOK_INVALID_ID) {
        patchlib_uninstall_hook(g_hook_id);
        g_hook_id = PATCH_HOOK_INVALID_ID;
    }
    
    // Release resources (Desktop)
#ifndef __ANDROID__
    if (patchlib_is_valid(g_player_type)) {
        patchlib_free(g_player_type);
        g_player_type = PATCH_NULL;
    }
#endif
}
```

**[⬆ Back to top](#-patchlib-user-guide)**

---

## ⚠ Important Notes

### Platform Differences

| Feature                       | Android (IL2CPP) | Desktop (Mono)                     |
|:------------------------------|:-----------------|:-----------------------------------|
| `patchlib_free`               | No-op (auto GC)  | Must call manually                 |
| `patchlib_handle_copy`        | No-op            | Must copy manually                 |
| `patchlib_field_get_pointer`  | ✅ Supported     | ❌ Not supported                   |
| `patchlib_method_get_pointer` | ✅ Supported     | ❌ Not supported                   |
| `patchlib_thread_*`           | ✅ Supported     | ✅ Supported (no-op but available) |

### Best Practices

#### ✅ Always check handle validity

```c
patch_handle_t type = patchlib_type_get_type("Terraria", "Main");
if (!patchlib_is_valid(type)) {
    // Handle error
    return;
}
```

#### ✅ Remember to release resources on desktop

```c
#ifndef __ANDROID__
patchlib_free(type);
patchlib_free(method);
patchlib_free(instance);
#endif
```

#### ✅ Use `tefstd_vector_t` for batch operations

```c
tefstd_vector_t methods;
patchlib_type_get_methods(type, false, &methods);
// ... process
tefstd_vector_destroy(&methods);
```

#### ✅ Hook callbacks should avoid time-consuming operations

```c
// ❌ Don't do this
bool my_prefix(...) {
    sleep(1);  // Will freeze the game
    return false;
}

// ✅ Should return quickly
bool my_prefix(...) {
    // Quick operation
    return false;
}
```

#### ✅ Attach before using in child threads

```c
#if __ANDROID__
void my_thread_func() {
    patch_handle_t thread = patchlib_thread_attach();
    // ... operations
    patchlib_thread_detach(thread);
}
#endif
```

### Common Error Codes

| Error                   | Cause                            | Solution                                          |
|:------------------------|:---------------------------------|:--------------------------------------------------|
| `PATCH_NULL`            | Type/Method/Field does not exist | Check namespace and name correctness              |
| `PATCH_HOOK_INVALID_ID` | Hook installation failed         | Check if method is valid                          |
| Crash                   | Parameter type mismatch          | Use correct parameter types when invoking methods |

**[⬆ Back to top](#-patchlib-user-guide)**

*Happy Modding! 🎮✨*