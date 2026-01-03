/*******************************************************************************
 * tefkernel - android_runtime_core
 * Copyright (C) 2025 eternalfuture-e38299
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: eternalfuture-e38299
 * GitHub: https://github.com/eternalfuture-e38299
 * Created: 2025/12/14
 *******************************************************************************/

#include <dlfcn.h>
#include <cstdio>
#include <cstring>

#include <jni.h>
#include <string>
#include <unistd.h>

#include <csignal>
#include <asm-generic/signal.h>
#include <asm-generic/siginfo.h>

#include "dobby.h"
#include "internal/log.h"
#include "patchlib/method.h"
#include "patchlib/type.h"
#include "../patchlib/android/il2cpp_api.h"
#include "patchlib/android/private.h"

int (*dobby_hook)(void *address, void *replace_func, void **origin_func);
int (*dobby_destroy)(void *address);

/*
void Initialize_AlmostEverything_pre(patch_handle_t orig_func, patch_handle_t instance, void** args, const patch_method_signature_t* sig_info) {
    TEKLOG_INFO("Hook triggered: Initialize_AlmostEverything_pre called with parameter");
    // TEKLOG_DEBUG("Game initialization hook activated successfully");
}


void (*old_Initialize_AlmostEverything)(void*);
void Initialize_AlmostEverything_post(patch_handle_t orig_func, patch_handle_t instance, void** args, void* result, const patch_method_signature_t* sig_info) {
    // old_Initialize_AlmostEverything(i);
    TEKLOG_INFO("Hook triggered: Initialize_AlmostEverything_post called with parameter");
    // TEKLOG_DEBUG("Game initialization hook activated successfully");
}


void init() {
    TEKLOG_INFO("Starting TEFKernel initialization");

    patch_handle_t method = patchlib_type_get_method_by_param_count(
        patchlib_type_get_type("Terraria", "Main"),
        ".ctor",
        0
    );

    if (method) {
        TEKLOG_DEBUG("Found target method, installing hook");
        patchlib_install_prepost_hook(method, (void*)Initialize_AlmostEverything_pre, (void*)Initialize_AlmostEverything_post);
        // DobbyHook(patchlib_method_get_pointer(method), (void*)Initialize_AlmostEverything_post, (void**)&old_Initialize_AlmostEverything);

        TEKLOG_INFO("Game hook installed successfully");
    } else {
        TEKLOG_ERROR("Failed to find target method for hooking");
    }

    TEKLOG_INFO("TEFKernel initialization completed");
}
*/

void start_test();


patch_handle_t find_and_initialize_make_generic_method_impl() {
    TEKLOG_INFO("[MakeGenericMethod] Starting search for MakeGenericMethod_impl");

    // If already initialized, return immediately
    if (patchlib_MakeGenericMethod_impl != nullptr) {
        TEKLOG_DEBUG("[MakeGenericMethod] Already initialized");
        return patchlib_MakeGenericMethod_impl;
    }

    patch_handle_t make_generic_method = nullptr;
    const char* search_classes[] = {
        "System.Reflection.RuntimeMethodInfo",
        "System.Reflection.MonoMethod",
        nullptr
    };

    // Search in different classes
    for (int class_idx = 0; search_classes[class_idx] != nullptr; class_idx++) {
        const char* class_name = search_classes[class_idx];
        TEKLOG_INFO("[MakeGenericMethod] Searching in class: %s", class_name);

        // Extract namespace and class name
        const char* dot_pos = strrchr(class_name, '.');
        auto namespace_name = "System.Reflection";
        const char* short_class_name = dot_pos ? dot_pos + 1 : class_name;

        patch_handle_t class_handle = patchlib_type_get_type(namespace_name, short_class_name);
        if (!class_handle) {
            TEKLOG_WARN("[MakeGenericMethod] Class not found: %s", class_name);
            continue;
        }

        TEKLOG_DEBUG("[MakeGenericMethod] Found class: %s (%p)", class_name, class_handle);

        // Get methods for this class
        tef_vector_t methods;
        if (!tefstd_vector_init(&methods, sizeof(patch_handle_t))) {
            TEKLOG_ERROR("[MakeGenericMethod] Failed to initialize methods vector");
            continue;
        }

        if (!patchlib_type_get_methods(class_handle, false, &methods)) {
            TEKLOG_WARN("[MakeGenericMethod] Failed to get methods for class: %s", class_name);
            tefstd_vector_destroy(&methods);
            continue;
        }

        const size_t method_count = tefstd_vector_size(&methods);
        TEKLOG_INFO("[MakeGenericMethod] Class %s has %zu methods:", class_name, method_count);

        // Debug: list all methods
        for (size_t i = 0; i < method_count; i++) {
            if (const auto* method_ptr = static_cast<patch_handle_t *>(tefstd_vector_at(&methods, i)); patchlib_is_valid(*method_ptr)) {
                const char* method_name = patchlib_method_get_name(*method_ptr);
                const int param_count = patchlib_method_get_param_count(*method_ptr);
                TEKLOG_INFO("[MakeGenericMethod]   [%zu] %s (params: %d)", i, method_name, param_count);
            }
        }

        // Search for MakeGenericMethod_impl
        for (size_t i = 0; i < method_count; i++) {
            const auto* method_ptr = static_cast<patch_handle_t *>(tefstd_vector_at(&methods, i));

            if (!patchlib_is_valid(*method_ptr)) {
                continue;
            }

            const char* current_name = patchlib_method_get_name(*method_ptr);
            if (!current_name) {
                continue;
            }

            if (strcmp(current_name, "MakeGenericMethod_impl") == 0) {
                make_generic_method = *method_ptr;
                TEKLOG_INFO("[MakeGenericMethod] FOUND: MakeGenericMethod_impl in %s (%p)", class_name, make_generic_method);

                // Verify method signature
                const int param_count = patchlib_method_get_param_count(make_generic_method);
                TEKLOG_INFO("[MakeGenericMethod] Method parameter count: %d", param_count);

                patchlib_MakeGenericMethod_impl = make_generic_method;
                tefstd_vector_destroy(&methods);
                return make_generic_method;
            }
        }

        // Clean up for this class
        tefstd_vector_destroy(&methods);
    }

    TEKLOG_ERROR("[MakeGenericMethod] FAILED: MakeGenericMethod_impl not found in any class");
    return nullptr;
}


int (*orig_il2cpp_init)(const char*) = nullptr;
int hook_il2cpp_init(const char* domain_name) {
    TEKLOG_INFO("il2cpp_init hook called, domain: %s", domain_name);

    if (orig_il2cpp_init == nullptr) {
        TEKLOG_ERROR("Original function pointer is NULL!");
        return -1;
    }

    TEKLOG_DEBUG("Calling original il2cpp_init function");
    const int r = orig_il2cpp_init(domain_name);
    TEKLOG_INFO("Original il2cpp_init returned: %d", r);

    TEKLOG_INFO("Starting TEFKernel core initialization");
    // init();


    patchlib_MakeGenericType = patchlib_type_get_method_by_param_count(patchlib_type_get_type("System", "RuntimeType"), "MakeGenericType", 2);
    find_and_initialize_make_generic_method_impl();



    // patchlib_MakeGenericMethod_impl = patchlib_type_get_methods(patchlib_type_get_type("System.Reflection", "MonoMethod"), "MakeGenericMethod_impl");



    TEKLOG_INFO("TEFKernel core initialization completed");

    start_test();

    return r;
}

static __thread bool in_hook = false;
static bool hooked = false;

void* (*old_dlopen)(const char *filename, int flag);
void* new_dlopen(const char *filename, const int flag) {
    if (in_hook) {
        return old_dlopen(filename, flag);
    }

    in_hook = true;
    TEKLOG_DEBUG("dlopen intercepted, loading: %s", filename);
    void* r = old_dlopen(filename, flag);

    if (!hooked && filename != nullptr && strstr(filename, "/libil2cpp.so") != nullptr) {
        TEKLOG_INFO("Detected il2cpp library loading: %s", filename);
        TEKLOG_DEBUG("Initializing il2cpp API");
        il2cpp_api_init(r);

        if (!DobbyHook(reinterpret_cast<void*>(il2cpp_init), reinterpret_cast<void*>(hook_il2cpp_init), reinterpret_cast<void **>(&orig_il2cpp_init))) {
            hooked = true;
            TEKLOG_INFO("il2cpp_init hook installed successfully");

            // Unhook after target achieved
            DobbyDestroy(reinterpret_cast<void*>(dlopen));
            DobbyDestroy(reinterpret_cast<void*>(dlsym));
            TEKLOG_INFO("dlopen/dlsym hooks removed");
        } else {
            TEKLOG_ERROR("Failed to install il2cpp_init hook");
        }
    }

    in_hook = false;
    return r;
}

void*(*old_dlsym)(void* handle, const char* sym);
void* new_dlsym(void* handle, const char* sym) {
    if (in_hook) {
        return old_dlsym(handle, sym);
    }

    in_hook = true;
    TEKLOG_TRACE("dlsym intercepted, symbol: %s, handle: %p", sym, handle);
    void* result = old_dlsym(handle, sym);

    // Log important symbol resolutions
    if (sym != nullptr) {
        if (strstr(sym, "il2cpp") != nullptr) {
            TEKLOG_DEBUG("Resolved il2cpp symbol: %s -> %p", sym, result);
        } else if (strstr(sym, "JNI_") != nullptr) {
            TEKLOG_DEBUG("Resolved JNI symbol: %s -> %p", sym, result);
        }
    }

    in_hook = false;
    return result;
}

JavaVM* GetJavaVMViaJNIStandard() {
    TEKLOG_DEBUG("Attempting to get JavaVM via JNI standard");

    void* jniGetCreatedJavaVMs = DobbySymbolResolver(nullptr, "JNI_GetCreatedJavaVMs");
    if (!jniGetCreatedJavaVMs) {
        TEKLOG_ERROR("Could not find JNI_GetCreatedJavaVMs symbol");
        return nullptr;
    }

    typedef jint (*JNI_GetCreatedJavaVMs_t)(JavaVM**, jsize, jsize*);
    const auto JNI_GetCreatedJavaVMs = reinterpret_cast<JNI_GetCreatedJavaVMs_t>(jniGetCreatedJavaVMs);

    JavaVM* vm = nullptr;
    jsize numVMs = 0;

    const jint result = JNI_GetCreatedJavaVMs(&vm, 1, &numVMs);
    if (result == JNI_OK && numVMs > 0 && vm != nullptr) {
        TEKLOG_INFO("Successfully obtained JavaVM: %p (number of VMs: %d)", vm, numVMs);
        return vm;
    }

    TEKLOG_ERROR("JNI_GetCreatedJavaVMs failed: result=%d, number of VMs=%d", result, numVMs);
    return nullptr;
}

static volatile sig_atomic_t in_signal_handler = 0;

void signal_handler(const int sig, [[maybe_unused]] siginfo_t* info, [[maybe_unused]] void* context) {
    if (in_signal_handler) {
        TEKLOG_CRITICAL("Recursive signal handler detected, emergency exit");
        _exit(1);
    }
    in_signal_handler = 1;

    auto sig_name = "UNKNOWN";
    switch(sig) {
        case SIGSEGV: sig_name = "SIGSEGV"; break;
        case SIGABRT: sig_name = "SIGABRT"; break;
        case SIGILL: sig_name = "SIGILL"; break;
        case SIGFPE: sig_name = "SIGFPE"; break;
        case SIGBUS: sig_name = "SIGBUS"; break;
        case SIGTRAP: sig_name = "SIGTRAP"; break;
        default: break;
    }

    TEKLOG_CRITICAL("Signal captured: %d (%s)",
                   sig, sig_name);

    TEKLOG_INFO("Performing emergency cleanup before exit");
    tefkernel_log_cleanup();

    // Restore default handler and re-raise signal
    signal(sig, 0);
    TEKLOG_DEBUG("Signal handler reset to default, re-raising signal");
    raise(sig);
}

void setup_signal_handlers() {
    TEKLOG_DEBUG("Setting up signal handlers for crash protection");

    struct sigaction sa = {};
    sa.sa_sigaction = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGTRAP, &sa, nullptr);

    TEKLOG_INFO("Signal handlers configured successfully");
}

__attribute__((constructor))
int init_ary() {
    TEKLOG_INFO("TEFKernel starting initialization");

    // Initialize logging system first
    TEKLOG_DEBUG("Initializing logging system");
    tefkernel_log_init(nullptr);

    TEKLOG_INFO("Installing system function hooks");
    DobbyHook(reinterpret_cast<void*>(dlopen), reinterpret_cast<void*>(new_dlopen), reinterpret_cast<void**>(&old_dlopen));
    DobbyHook(reinterpret_cast<void*>(dlsym), reinterpret_cast<void*>(new_dlsym), reinterpret_cast<void **>(&old_dlsym));

    TEKLOG_DEBUG("Attempting to get JNI environment");
    JNIEnv *env;
    if (JavaVM* vm = GetJavaVMViaJNIStandard(); vm && vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_OK) {
        TEKLOG_INFO("JNI environment obtained successfully: %p", env);
    } else {
        TEKLOG_WARN("Failed to get JNI environment");
    }

    TEKLOG_DEBUG("Setting up crash signal handlers");
    setup_signal_handlers();

    TEKLOG_INFO("TEFKernel initialization completed successfully");
    return 0;
}