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
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "dobby.h"
#include "xdl.h"
#include "internal/log.h"
#include "patchlib/method.h"
#include "patchlib/type.h"
#include "../patchlib/android/il2cpp_api.h"
#include "internal/runtime.h"
#include "patchlib/android/private.h"
#include "tefstd/vector.h"

#include "internal/terraria/netmanager.h"
#include "internal/kernel_state.h"

patch_handle_t find_and_initialize_make_generic_method_impl() {
    // If already initialized, return immediately
    if (patchlib_MakeGenericMethod_impl != nullptr) {
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

        // Extract namespace and class name
        const char* dot_pos = strrchr(class_name, '.');
        auto namespace_name = "System.Reflection";
        const char* short_class_name = dot_pos ? dot_pos + 1 : class_name;

        patch_handle_t class_handle = patchlib_type_get_type(namespace_name, short_class_name);
        if (!class_handle) {
            continue;
        }

        // Get methods for this class
        tefstd_vector_t methods;
        if (!tefstd_vector_init(&methods, sizeof(patch_handle_t))) {
            continue;
        }

        if (!patchlib_type_get_methods(class_handle, false, &methods)) {
            tefstd_vector_destroy(&methods);
            continue;
        }

        const size_t method_count = tefstd_vector_size(&methods);

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
                patchlib_MakeGenericMethod_impl = make_generic_method;
                tefstd_vector_destroy(&methods);

                TEKLOG_INFO("[MakeGenericMethod] Found MakeGenericMethod_impl in %s", class_name);
                return make_generic_method;
            }
        }

        // Clean up for this class
        tefstd_vector_destroy(&methods);
    }

    TEKLOG_ERROR("[MakeGenericMethod] MakeGenericMethod_impl not found");
    return nullptr;
}

void start_test();

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


    patchlib_MakeGenericType = patchlib_type_get_method_by_param_count(patchlib_type_get_type("System", "RuntimeType"), "MakeGenericType", 2);
    find_and_initialize_make_generic_method_impl();

    terraria_netmanager_init();
    tefkernel_init();
    tefkernel_load();
    tefkernel_start();

    TEKLOG_INFO("TEFKernel core initialization completed");

    return r;
}

JavaVM* GetJavaVMViaJNIStandard() {
    TEKLOG_DEBUG("Attempting to get JavaVM via JNI standard");

    void* jniGetCreatedJavaVMs = DobbySymbolResolver("libart.so", "JNI_GetCreatedJavaVMs");
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

void init_iohook(JavaVM* vm);

// 辅助函数：读取文件的第一行
static char* read_first_line(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        TEKLOG_DEBUG("Cannot open file: %s", filepath);
        return nullptr;
    }

    char* line = nullptr;
    size_t len = 0;
    ssize_t read = getline(&line, &len, file);

    fclose(file);

    if (read <= 0) {
        free(line);
        return nullptr;
    }

    // 去除换行符
    if (line[read - 1] == '\n') {
        line[read - 1] = '\0';
    }

    return line;
}

// 获取当前应用的数据目录
static char* get_current_app_data_dir(JavaVM* vm) {
    JNIEnv* env = nullptr;
    const jint result = vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

    if (result == JNI_EDETACHED) {
        vm->AttachCurrentThread(&env, nullptr);
    }

    if (!env) {
        TEKLOG_ERROR("Failed to get JNIEnv");
        return nullptr;
    }

    jclass activity_thread_class = env->FindClass("android/app/ActivityThread");
    if (!activity_thread_class) {
        TEKLOG_ERROR("Failed to find ActivityThread class");
        return nullptr;
    }

    jmethodID current_activity_thread_method = env->GetStaticMethodID(
        activity_thread_class, "currentActivityThread", "()Landroid/app/ActivityThread;");
    if (!current_activity_thread_method) {
        TEKLOG_ERROR("Failed to get currentActivityThread method");
        env->DeleteLocalRef(activity_thread_class);
        return nullptr;
    }

    jobject current_activity_thread = env->CallStaticObjectMethod(
        activity_thread_class, current_activity_thread_method);
    if (!current_activity_thread) {
        TEKLOG_ERROR("Failed to get current ActivityThread");
        env->DeleteLocalRef(activity_thread_class);
        return nullptr;
    }

    jmethodID get_application_method = env->GetMethodID(
        activity_thread_class, "getApplication", "()Landroid/app/Application;");
    if (!get_application_method) {
        TEKLOG_ERROR("Failed to get getApplication method");
        env->DeleteLocalRef(current_activity_thread);
        env->DeleteLocalRef(activity_thread_class);
        return nullptr;
    }

    jobject application = env->CallObjectMethod(current_activity_thread, get_application_method);
    if (!application) {
        TEKLOG_ERROR("Failed to get Application");
        env->DeleteLocalRef(current_activity_thread);
        env->DeleteLocalRef(activity_thread_class);
        return nullptr;
    }

    jclass context_class = env->FindClass("android/content/Context");
    if (!context_class) {
        TEKLOG_ERROR("Failed to find Context class");
        env->DeleteLocalRef(application);
        env->DeleteLocalRef(current_activity_thread);
        env->DeleteLocalRef(activity_thread_class);
        return nullptr;
    }

    jmethodID get_files_dir_method = env->GetMethodID(
        context_class, "getFilesDir", "()Ljava/io/File;");
    if (!get_files_dir_method) {
        TEKLOG_ERROR("Failed to get getFilesDir method");
        env->DeleteLocalRef(context_class);
        env->DeleteLocalRef(application);
        env->DeleteLocalRef(current_activity_thread);
        env->DeleteLocalRef(activity_thread_class);
        return nullptr;
    }

    jobject files_dir_obj = env->CallObjectMethod(application, get_files_dir_method);
    if (!files_dir_obj) {
        TEKLOG_ERROR("Failed to get files directory");
        env->DeleteLocalRef(context_class);
        env->DeleteLocalRef(application);
        env->DeleteLocalRef(current_activity_thread);
        env->DeleteLocalRef(activity_thread_class);
        return nullptr;
    }

    jclass file_class = env->FindClass("java/io/File");
    if (!file_class) {
        TEKLOG_ERROR("Failed to find File class");
        env->DeleteLocalRef(files_dir_obj);
        env->DeleteLocalRef(context_class);
        env->DeleteLocalRef(application);
        env->DeleteLocalRef(current_activity_thread);
        env->DeleteLocalRef(activity_thread_class);
        return nullptr;
    }

    jmethodID get_absolute_path_method = env->GetMethodID(
        file_class, "getAbsolutePath", "()Ljava/lang/String;");
    if (!get_absolute_path_method) {
        TEKLOG_ERROR("Failed to get getAbsolutePath method");
        env->DeleteLocalRef(file_class);
        env->DeleteLocalRef(files_dir_obj);
        env->DeleteLocalRef(context_class);
        env->DeleteLocalRef(application);
        env->DeleteLocalRef(current_activity_thread);
        env->DeleteLocalRef(activity_thread_class);
        return nullptr;
    }

    const auto path_str = reinterpret_cast<jstring>(env->CallObjectMethod(files_dir_obj, get_absolute_path_method));
    const char* path = env->GetStringUTFChars(path_str, nullptr);

    // 复制到C字符串
    char* data_dir = strdup(path);

    // 释放资源
    env->ReleaseStringUTFChars(path_str, path);
    env->DeleteLocalRef(path_str);
    env->DeleteLocalRef(file_class);
    env->DeleteLocalRef(files_dir_obj);
    env->DeleteLocalRef(context_class);
    env->DeleteLocalRef(application);
    env->DeleteLocalRef(current_activity_thread);
    env->DeleteLocalRef(activity_thread_class);

    if (result == JNI_EDETACHED) {
        vm->DetachCurrentThread();
    }

    return data_dir;
}

char* tefkernel_working_dir = nullptr;


static std::atomic<bool> il2cpp_loaded{false};
static std::atomic<void*> il2cpp_handle{nullptr};
static std::thread il2cpp_watcher_thread;
static std::atomic<bool> il2cpp_watcher_running{false};

// 修改WaitForIL2CppLib，不再阻塞主线程
void* WaitForIL2CppLib(int timeout_seconds = 30) {
    TEKLOG_INFO("[xDL] Starting IL2CPP watcher, timeout: %d seconds", timeout_seconds);

    const int check_interval_ms = 100;
    int total_wait_time = 0;
    const int max_wait_time = timeout_seconds * 1000;

    while (total_wait_time < max_wait_time && il2cpp_watcher_running.load()) {
        // 使用xdl_open查找已加载的il2cpp库
        void* handle = xdl_open("libil2cpp.so", XDL_DEFAULT);

        if (handle != nullptr) {
            // 验证确实是il2cpp库
            void* test_symbol = xdl_sym(handle, "il2cpp_init", nullptr);
            if (test_symbol != nullptr) {
                TEKLOG_INFO("[xDL] Found valid IL2CPP library: %p (il2cpp_init at %p)", handle, test_symbol);
                return handle;
            }
            TEKLOG_DEBUG("[xDL] Library found but not IL2CPP (no il2cpp_init symbol)");;
        }

        // 等待下一次检查
        usleep(check_interval_ms * 1000);
        total_wait_time += check_interval_ms;

        // 每5秒打印一次等待状态
        if (total_wait_time % 5000 == 0) {
            TEKLOG_DEBUG("[xDL] Still waiting for IL2CPP... (%dms/%dms)",
                        total_wait_time, max_wait_time);
        }
    }

    TEKLOG_WARN("[xDL] Timeout waiting for IL2CPP library");
    return nullptr;
}

void IL2CppWatcherThreadFunc() {
    TEKLOG_INFO("[IL2CPP-Watcher] Starting IL2CPP watcher thread");

    // 等待IL2CPP库加载
    void* handle = WaitForIL2CppLib(30); // 30秒超时

    if (handle == nullptr) {
        TEKLOG_ERROR("[IL2CPP-Watcher] Failed to find IL2CPP library");
        il2cpp_watcher_running.store(false);
        return;
    }

    il2cpp_handle.store(handle);
    il2cpp_loaded.store(true);

    TEKLOG_INFO("[IL2CPP-Watcher] IL2CPP library found, handle: %p", handle);

    // 初始化il2cpp API
    TEKLOG_DEBUG("[IL2CPP-Watcher] Initializing IL2CPP API");
    il2cpp_api_init(handle);

    // Hook il2cpp_init
    TEKLOG_DEBUG("[IL2CPP-Watcher] Installing il2cpp_init hook");
    if (!DobbyHook(reinterpret_cast<void*>(il2cpp_init),
                   reinterpret_cast<void*>(hook_il2cpp_init),
                   reinterpret_cast<void**>(&orig_il2cpp_init))) {
        TEKLOG_INFO("[IL2CPP-Watcher] il2cpp_init hook installed successfully");
    } else {
        TEKLOG_ERROR("[IL2CPP-Watcher] Failed to install il2cpp_init hook");
    }

    il2cpp_watcher_running.store(false);
    TEKLOG_INFO("[IL2CPP-Watcher] IL2CPP watcher thread completed");
}

void StartIL2CppWatcher() {
    if (il2cpp_watcher_running.load() || il2cpp_loaded.load()) {
        TEKLOG_DEBUG("[Main] IL2CPP watcher already running or IL2CPP already loaded");
        return;
    }

    TEKLOG_INFO("[Main] Starting IL2CPP watcher in background thread");
    il2cpp_watcher_running.store(true);
    il2cpp_watcher_thread = std::thread(IL2CppWatcherThreadFunc);
    il2cpp_watcher_thread.detach();
}

__attribute__((constructor))
int init_ary() {
    // 初始化日志系统
    tefkernel_log_init(nullptr);
    TEKLOG_INFO("TEFKernel initializing");

    // 获取JavaVM
    JavaVM* vm = GetJavaVMViaJNIStandard();
    if (!vm) {
        TEKLOG_ERROR("Failed to get JavaVM");
        return -1;
    }

    // 检查是否已经有配置文件
    if (char* data_dir = get_current_app_data_dir(vm)) {
        char config_path[512];
        snprintf(config_path, sizeof(config_path), "%s/tefkernel_working_dir", data_dir);

        TEKLOG_DEBUG("Checking config file: %s", config_path);

        if (access(config_path, F_OK) == 0) {
            // 文件存在，读取第一行
            if (char* line = read_first_line(config_path)) {
                TEKLOG_INFO("Found config file, working dir: %s", line);

                // 释放之前的内存（如果有）
                if (tefkernel_working_dir) {
                    free(tefkernel_working_dir);
                }

                // 分配内存并保存
                tefkernel_working_dir = strdup(line);
                TEKLOG_INFO("Skipping iohook as config file exists");
                free(line);
                free(data_dir);
            }
            TEKLOG_WARN("Config file exists but cannot read");
        } else {
            TEKLOG_DEBUG("Config file does not exist, will perform iohook");

            // 初始化IO hook
            TEKLOG_DEBUG("Initializing IO hooks");
            init_iohook(vm);
            free(data_dir);
        }
    } else {
        TEKLOG_WARN("Failed to get data directory, will continue with iohook");

        // 初始化IO hook
        TEKLOG_DEBUG("Initializing IO hooks");
        init_iohook(vm);
    }

    TEKLOG_DEBUG("Setting up crash signal handlers");
    setup_signal_handlers();


    StartIL2CppWatcher();

    TEKLOG_INFO("TEFKernel initialization completed successfully");
    return 0;
}
