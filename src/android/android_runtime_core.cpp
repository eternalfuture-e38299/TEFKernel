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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <atomic>
#include <sys/stat.h>
#include <unistd.h>

#include <jni.h>

#include "dobby.h"
#include "xdl.h"

#include "tef/core/log.hpp"
#include "runtime.hpp"          // tef::runtime::*
#include "crash_handler.hpp"    // tef::crash::*
#include "kernel_state.hpp"     // tef::state::*
#include "patchlib/type.h"
#include "patchlib/method.h"
#include "abi_impl/patchlib/android/private.hpp"
#include "abi_impl/patchlib/il2cpp_api.h"

// C++ 子系统
#include "terraria/main_state.hpp"

using tef::log::debug;
using tef::log::error;
using tef::log::info;
using tef::log::warn;

// ============================================================================
// patchlib 泛型方法初始化
// ============================================================================
static patch_handle_t find_and_initialize_make_generic_method_impl() {
    if (patchlib_make_generic_method_impl != nullptr)
        return patchlib_make_generic_method_impl;

    const char *search_classes[] = {
        "System.Reflection.RuntimeMethodInfo",
        "System.Reflection.MonoMethod",
        nullptr,
    };

    for (int i = 0; search_classes[i] != nullptr; ++i) {
        const char *class_name = search_classes[i];
        const char *dot = std::strrchr(class_name, '.');
        const char *ns = "System.Reflection";
        const char *short_name = dot ? dot + 1 : class_name;

        patch_handle_t class_handle = patchlib_type_get_type(ns, short_name);
        if (!class_handle) continue;

        tefstd_vector_t methods;
        if (!tefstd_vector_init(&methods, sizeof(patch_handle_t))) continue;
        if (!patchlib_type_get_methods(class_handle, false, &methods)) {
            tefstd_vector_destroy(&methods);
            continue;
        }

        const size_t n = tefstd_vector_size(&methods);
        for (size_t j = 0; j < n; ++j) {
            auto *p = static_cast<patch_handle_t *>(tefstd_vector_at(&methods, j));
            if (!patchlib_is_valid(*p)) continue;
            const char *name = patchlib_method_get_name(*p);
            if (name && std::strcmp(name, "MakeGenericMethod_impl") == 0) {
                patch_handle_t make_generic_method = *p;
                patchlib_make_generic_method_impl = make_generic_method;
                tefstd_vector_destroy(&methods);
                info("[MakeGenericMethod] Found in {}", class_name);
                return make_generic_method;
            }
        }
        tefstd_vector_destroy(&methods);
    }

    error("[MakeGenericMethod] MakeGenericMethod_impl not found");
    return nullptr;
}

// ============================================================================
// il2cpp_init hook
// ============================================================================
static int (*orig_il2cpp_init)(const char *) = nullptr;

static int hook_il2cpp_init(const char *domain_name) {
    info("========================================");
    info("TEFKernel loading...");
    info("il2cpp_init hook called, domain: {}", domain_name ? domain_name : "(null)");

    if (!orig_il2cpp_init) {
        error("Original function pointer is NULL!");
        return -1;
    }

    const int r = orig_il2cpp_init(domain_name);
    info("Original il2cpp_init returned: {}", r);

    // patchlib 泛型方法
    patchlib_make_generic_type = patchlib_type_get_method_by_param_count(
        patchlib_type_get_type("System", "RuntimeType"), "MakeGenericType", 2);
    find_and_initialize_make_generic_method_impl();

    // 所有 C++ 子系统：register_all + Main.Initialize 钩子
    terraria::main_state::instance().initialize(false);

    // tefkernel 运行时
    tef::runtime::init();
    tef::runtime::load();
    tef::runtime::start();

    info("TEFKernel core initialization completed");
    info("========================================");
    return r;
}

// ============================================================================
// JavaVM / 数据目录
// ============================================================================
static JavaVM *get_java_vm_via_jni_standard() {
    void *sym = DobbySymbolResolver("libart.so", "JNI_GetCreatedJavaVMs");
    if (!sym) {
        error("Could not find JNI_GetCreatedJavaVMs symbol");
        return nullptr;
    }
    using fn_t = jint (*)(JavaVM **, jsize, jsize *);
    auto fn = reinterpret_cast<fn_t>(sym);

    JavaVM *vm = nullptr;
    jsize n = 0;
    const jint result = fn(&vm, 1, &n);
    if (result == JNI_OK && n > 0 && vm) {
        info("Successfully obtained JavaVM: {} (VMs: {})",
             static_cast<void *>(vm), n);
        return vm;
    }
    error("JNI_GetCreatedJavaVMs failed: result={} VMs={}", result, n);
    return nullptr;
}

// Java 抛异常后必须先 ExceptionClear 再继续 JNI，否则 ART abort
static void tef_jni_clear_pending(JNIEnv *env, const char *where) {
    if (!env) return;
    if (env->ExceptionCheck()) {
        warn("jni: 检测到未清理的异常({})，已 ExceptionClear", where);
        env->ExceptionClear();
    }
}

static char *get_current_app_data_dir(JavaVM *vm) {
    JNIEnv *env = nullptr;
    const jint result = vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    tef_jni_clear_pending(env, "GetEnv");
    bool attached = false;
    if (result == JNI_EDETACHED) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) {
            error("Failed to attach current thread");
            return nullptr;
        }
        attached = true;
    }
    if (!env) {
        error("Failed to get JNIEnv");
        if (attached) vm->DetachCurrentThread();
        return nullptr;
    }

    auto detach_if_needed = [&] {
        if (attached) vm->DetachCurrentThread();
    };

    auto cleanup = [&](jclass at, jobject cat, jobject app, jclass ctx,
                       jobject files, jclass fc, jstring path) {
        if (path) env->DeleteLocalRef(path);
        if (fc) env->DeleteLocalRef(fc);
        if (files) env->DeleteLocalRef(files);
        if (ctx) env->DeleteLocalRef(ctx);
        if (app) env->DeleteLocalRef(app);
        if (cat) env->DeleteLocalRef(cat);
        if (at) env->DeleteLocalRef(at);
    };

    jclass at_cls = env->FindClass("android/app/ActivityThread");
    if (!at_cls) {
        cleanup(at_cls, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        detach_if_needed();
        return nullptr;
    }

    jmethodID cur_at = env->GetStaticMethodID(
        at_cls, "currentActivityThread", "()Landroid/app/ActivityThread;");
    if (!cur_at) {
        cleanup(at_cls, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        detach_if_needed();
        return nullptr;
    }

    jobject at = env->CallStaticObjectMethod(at_cls, cur_at);
    if (!at) {
        cleanup(at_cls, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        detach_if_needed();
        return nullptr;
    }

    jmethodID get_app = env->GetMethodID(at_cls, "getApplication", "()Landroid/app/Application;");
    if (!get_app) {
        cleanup(at_cls, at, nullptr, nullptr, nullptr, nullptr, nullptr);
        detach_if_needed();
        return nullptr;
    }

    jobject app = env->CallObjectMethod(at, get_app);
    tef_jni_clear_pending(env, "getApplication");
    if (!app) {
        cleanup(at_cls, at, nullptr, nullptr, nullptr, nullptr, nullptr);
        detach_if_needed();
        return nullptr;
    }

    jclass ctx_cls = env->FindClass("android/content/Context");
    if (!ctx_cls) {
        cleanup(at_cls, at, app, nullptr, nullptr, nullptr, nullptr);
        detach_if_needed();
        return nullptr;
    }

    jmethodID get_files = env->GetMethodID(ctx_cls, "getFilesDir", "()Ljava/io/File;");
    if (!get_files) {
        cleanup(at_cls, at, app, ctx_cls, nullptr, nullptr, nullptr);
        detach_if_needed();
        return nullptr;
    }

    jobject files = env->CallObjectMethod(app, get_files);
    tef_jni_clear_pending(env, "getFilesDir");
    if (!files) {
        cleanup(at_cls, at, app, ctx_cls, nullptr, nullptr, nullptr);
        detach_if_needed();
        return nullptr;
    }

    jclass fc = env->FindClass("java/io/File");
    if (!fc) {
        cleanup(at_cls, at, app, ctx_cls, files, nullptr, nullptr);
        detach_if_needed();
        return nullptr;
    }

    jmethodID get_abs = env->GetMethodID(fc, "getAbsolutePath", "()Ljava/lang/String;");
    if (!get_abs) {
        cleanup(at_cls, at, app, ctx_cls, files, fc, nullptr);
        detach_if_needed();
        return nullptr;
    }

    auto path_str = reinterpret_cast<jstring>(env->CallObjectMethod(files, get_abs));
    tef_jni_clear_pending(env, "getAbsolutePath");
    if (!path_str) {
        cleanup(at_cls, at, app, ctx_cls, files, fc, nullptr);
        detach_if_needed();
        return nullptr;
    }

    const char *path = env->GetStringUTFChars(path_str, nullptr);
    char *data_dir = path ? strdup(path) : nullptr;
    if (path) env->ReleaseStringUTFChars(path_str, path);

    cleanup(at_cls, at, app, ctx_cls, files, fc, path_str);
    detach_if_needed();
    return data_dir;
}

// ============================================================================
// 配置文件
// ============================================================================
static char *tefkernel_working_dir = nullptr;

static char *read_first_line(const char *filepath) {
    FILE *f = std::fopen(filepath, "r");
    if (!f) return nullptr;
    char *line = nullptr;
    size_t len = 0;
    const ssize_t n = getline(&line, &len, f);
    std::fclose(f);
    if (n <= 0) { std::free(line); return nullptr; }
    if (line[n - 1] == '\n') line[n - 1] = '\0';
    return line;
}

// ============================================================================
// iohook
// ============================================================================
void init_iohook(JavaVM *vm);

// ============================================================================
// IL2CPP 监听
// ============================================================================
static std::atomic<bool> il2cpp_loaded{false};
static std::atomic<void *> il2cpp_handle{nullptr};
static std::thread il2cpp_watcher_thread;
static std::atomic<bool> il2cpp_watcher_running{false};

static void *wait_for_il2cpp_lib(const int timeout_seconds = 30) {
    info("[xDL] Waiting for IL2CPP, timeout {}s", timeout_seconds);
    int waited = 0;
    const int max_wait = timeout_seconds * 1000;

    while (waited < max_wait && il2cpp_watcher_running.load()) {
        constexpr int step = 100;
        if (void *h = xdl_open("libil2cpp.so", XDL_DEFAULT); h) {
            if (void *sym = xdl_sym(h, "il2cpp_init", nullptr); sym) {
                info("[xDL] Found IL2CPP: {} (il2cpp_init at {})", h, sym);
                return h;
            }
        }
        usleep(step * 1000);
        waited += step;
        if (waited % 5000 == 0)
            debug("[xDL] Still waiting... ({}/{}ms)", waited, max_wait);
    }
    warn("[xDL] Timeout waiting for IL2CPP");
    return nullptr;
}

static void il2cpp_watcher_thread_func() {
    info("[IL2CPP-Watcher] Started");
    void *h = wait_for_il2cpp_lib(30);
    if (!h) {
        error("[IL2CPP-Watcher] Failed");
        il2cpp_watcher_running.store(false);
        return;
    }

    il2cpp_handle.store(h);
    il2cpp_loaded.store(true);

    debug("[IL2CPP-Watcher] Initializing il2cpp API");
    il2cpp_api_init(h);

    debug("[IL2CPP-Watcher] Installing il2cpp_init hook");
    if (!DobbyHook(reinterpret_cast<void *>(il2cpp_init),
                   reinterpret_cast<void *>(hook_il2cpp_init),
                   reinterpret_cast<void **>(&orig_il2cpp_init))) {
        info("[IL2CPP-Watcher] il2cpp_init hook installed");
    } else {
        error("[IL2CPP-Watcher] Failed to install il2cpp_init hook");
    }

    il2cpp_watcher_running.store(false);
}

static void start_il2cpp_watcher() {
    if (il2cpp_watcher_running.load() || il2cpp_loaded.load()) return;
    il2cpp_watcher_running.store(true);
    il2cpp_watcher_thread = std::thread(il2cpp_watcher_thread_func);
    il2cpp_watcher_thread.detach();
}

// ============================================================================
// 入口
//
// 注意：__attribute__((constructor)) 在 dlopen 的 call_constructors 阶段执行，
// 此时 ART 尚未就绪，任何 FindClass / CallObjectMethod 等会触发 Java 栈遍历的
// JNI 调用都会导致 "null art-method found ... Visiting Method.invoke" 并 abort
// （见 stack.cc:825 / runtime.cc:769）。因此：
//   - 构造器里只做纯 native 工作（log / crash handler）；
//   - JNI 与依赖 ART 的逻辑全部延迟到独立线程，等 VM 可用后再执行。
// ============================================================================
__attribute__((constructor))
static int tefkernel_entry() {
    tef::log::init(nullptr);
    tef::crash::init();

    std::thread([] {
        info("TEFKernel initializing");

        // 轮询等待 ART 就绪（只调 JNI_GetCreatedJavaVMs，不触发栈遍历）
        JavaVM *vm = nullptr;
        for (int i = 0; i < 300 && !vm; ++i) {
            vm = get_java_vm_via_jni_standard();
            if (!vm) usleep(100 * 1000);
        }
        if (!vm) { error("Failed to obtain JavaVM"); return; }

        if (char *data_dir = get_current_app_data_dir(vm)) {
            char config_path[512];
            std::snprintf(config_path, sizeof(config_path),
                          "%s/tefkernel_working_dir", data_dir);

            if (access(config_path, F_OK) == 0) {
                if (char *line = read_first_line(config_path)) {
                    info("Found config, working dir: {}", line);
                    if (tefkernel_working_dir) std::free(tefkernel_working_dir);
                    tefkernel_working_dir = strdup(line);
                    std::free(line);
                } else {
                    warn("Config exists but cannot read");
                }
            } else {
                debug("Config missing, running iohook");
                init_iohook(vm);
            }
            std::free(data_dir);
        } else {
            warn("Failed to get data dir, running iohook");
            init_iohook(vm);
        }

        // 把工作目录交给 tef::state 管理（接管所有权）
        tef::state::set_working_dir(tefkernel_working_dir);

        // 日志目录（若工作目录已知则切换到文件日志）
        char log_dir[512];
        char log_cfg[512] = {};
        std::snprintf(log_dir, sizeof(log_dir), "%s/logs/tefkernel",
                      tefkernel_working_dir ? tefkernel_working_dir : "");

        struct stat st{};
        if (stat(log_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
            std::snprintf(log_cfg, sizeof(log_cfg), "%s/runtime", log_dir);
            std::fprintf(stderr, "[LOG] Log directory found: %s\n", log_dir);
        }

        if (log_cfg[0] != '\0') tef::log::init(log_cfg);

        start_il2cpp_watcher();
        info("TEFKernel initialization completed");
    }).detach();

    return 0;
}