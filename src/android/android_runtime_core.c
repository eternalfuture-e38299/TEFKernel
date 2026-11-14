/*******************************************************************************
 * tefkernel - android_runtime_core
 * Copyright (C) 2026 eternalfuture-e38299
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
 * Created: 2026/11/14
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>

#include <jni.h>

#include "shadowhook.h"
#include "xdl.h"
#include "internal/log.h"
#include "patchlib/method.h"
#include "patchlib/type.h"
#include "../patchlib/il2cpp_api.h"
#include "internal/runtime.h"
#include "patchlib/android/private.h"
#include "tefstd/vector.h"
#include "tefstd/scope.h"

#include "internal/terraria/netmanager.h"
#include "internal/kernel_state.h"
#include "internal/crash_handler.h"
#include "internal/terraria/asset.h"
#include "internal/terraria/main.h"
#include "internal/terraria/texture2d.h"
#include "internal/terraria/content/item_manager.h"
#include "internal/terraria/content/projectile_manager.h"
#include "internal/terraria/content/npc_manager.h"
#include "internal/terraria/content/buff_manager.h"
#include "internal/terraria/content/prefix_manager.h"
#include "internal/terraria/gui.h"
#include "internal/terraria/save/tef_save_game_handles.h"
#include "internal/terraria/save/tef_player_save.h"
#include "internal/terraria/save/tef_world_save.h"
#include "internal/terraria/recipe_manager.h"
#include "internal/terraria/language_manager.h"
#include "terraria/content/button_manager.h"

/* ---------- 全局状态 ---------- */
char *tefkernel_working_dir = NULL;

static atomic_bool il2cpp_loaded = false;
static atomic_bool il2cpp_watcher_running = false;
static atomic_uintptr_t il2cpp_handle = 0;
static pthread_t il2cpp_watcher_thread;

/* ---------- JNI 异常清理 ---------- */
static void tef_jni_clear_pending(JNIEnv *env, const char *where) {
    if (!env) return;
    if ((*env)->ExceptionCheck(env)) {
        TEKLOG_WARN("jni: 检测到未清理的异常(%s)，已 ExceptionClear", where);
        (*env)->ExceptionClear(env);
    }
}

/* ---------- MakeGenericMethod_impl 查找 ---------- */
static patch_handle_t find_and_initialize_make_generic_method_impl(void) {
    if (patchlib_MakeGenericMethod_impl != NULL) {
        return patchlib_MakeGenericMethod_impl;
    }

    const char *search_classes[] = {
        "System.Reflection.RuntimeMethodInfo",
        "System.Reflection.MonoMethod",
        NULL
    };

    for (int class_idx = 0; search_classes[class_idx] != NULL; class_idx++) {
        const char *class_name = search_classes[class_idx];
        const char *dot_pos = strrchr(class_name, '.');
        const char *namespace_name = "System.Reflection";
        const char *short_class_name = dot_pos ? dot_pos + 1 : class_name;

        patch_handle_t class_handle = patchlib_type_get_type(namespace_name, short_class_name);
        if (!class_handle) continue;

        tefstd_vector_t methods;
        if (!tefstd_vector_init(&methods, sizeof(patch_handle_t))) continue;

        if (!patchlib_type_get_methods(class_handle, false, &methods)) {
            tefstd_vector_destroy(&methods);
            continue;
        }

        const size_t method_count = tefstd_vector_size(&methods);
        for (size_t i = 0; i < method_count; i++) {
            patch_handle_t *method_ptr = tefstd_vector_at(&methods, i);
            if (!patchlib_is_valid(*method_ptr)) continue;

            const char *current_name = patchlib_method_get_name(*method_ptr);
            if (!current_name) continue;

            if (strcmp(current_name, "MakeGenericMethod_impl") == 0) {
                patchlib_MakeGenericMethod_impl = *method_ptr;
                tefstd_vector_destroy(&methods);
                TEKLOG_INFO("[MakeGenericMethod] Found MakeGenericMethod_impl in %s", class_name);
                return patchlib_MakeGenericMethod_impl;
            }
        }
        tefstd_vector_destroy(&methods);
    }

    TEKLOG_ERROR("[MakeGenericMethod] MakeGenericMethod_impl not found");
    return NULL;
}

/* ---------- il2cpp_init hook ---------- */
static int (*orig_il2cpp_init)(const char *) = NULL;

static int hook_il2cpp_init(const char *domain_name) {
    TEKLOG_INFO("========================================");
    TEKLOG_INFO("TEFKernel loading...");
    TEKLOG_INFO("il2cpp_init hook called, domain: %s", domain_name);

    if (!orig_il2cpp_init) {
        TEKLOG_ERROR("Original function pointer is NULL!");
        return -1;
    }

    const int r = orig_il2cpp_init(domain_name);
    TEKLOG_INFO("Original il2cpp_init returned: %d", r);

    TEKLOG_INFO("Starting TEFKernel core initialization");

    patchlib_MakeGenericType = patchlib_type_get_method_by_param_count(
        patchlib_type_get_type("System", "RuntimeType"),
        "MakeGenericType", 2);
    find_and_initialize_make_generic_method_impl();

    terraria_main_init(false);
    terraria_texture2d_init(false);
    terraria_asset_init();
    terraria_item_manager_init();
    terraria_projectile_manager_init();
    terraria_npc_manager_init();
    terraria_buff_manager_init();
    terraria_prefix_manager_init();
    terraria_gui_init();
    terraria_button_manager_install();

    tef_save_game_handles_init();
    tef_player_save_init();
    tef_world_save_init();

    terraria_recipe_manager_init();
    terraria_language_manager_init();

    tefkernel_init();
    tefkernel_load();
    tefkernel_start();
    terraria_netmanager_init();

    // start_test();

    TEKLOG_INFO("TEFKernel core initialization completed");
    TEKLOG_INFO("========================================");
    return r;
}

/* ---------- JavaVM ---------- */
static JavaVM *get_java_vm_via_jni_standard(void) {
    void *handle = xdl_open("libart.so", XDL_DEFAULT);
    if (!handle) {
        TEKLOG_ERROR("Could not open libart.so");
        return NULL;
    }

    void *jniGetCreatedJavaVMs = xdl_sym(handle, "JNI_GetCreatedJavaVMs", NULL);
    xdl_close(handle);
    if (!jniGetCreatedJavaVMs) {
        TEKLOG_ERROR("Could not find JNI_GetCreatedJavaVMs symbol");
        return NULL;
    }

    typedef jint (*JNI_GetCreatedJavaVMs_t)(JavaVM **, jsize, jsize *);
    const JNI_GetCreatedJavaVMs_t fn = jniGetCreatedJavaVMs;

    JavaVM *vm = NULL;
    jsize numVMs = 0;
    const jint result = fn(&vm, 1, &numVMs);
    if (result == JNI_OK && numVMs > 0 && vm) {
        TEKLOG_INFO("Successfully obtained JavaVM: %p (number of VMs: %d)", (void *)vm, numVMs);
        return vm;
    }

    TEKLOG_ERROR("JNI_GetCreatedJavaVMs failed: result=%d, number of VMs=%d", result, numVMs);
    return NULL;
}

void init_iohook(JavaVM *vm);

/* ---------- 读第一行 ---------- */
static char *read_first_line(const char *filepath) {
    FILE *file = fopen(filepath, "r");
    if (!file) return NULL;

    char *line = NULL;
    size_t len = 0;
    ssize_t read = getline(&line, &len, file);
    fclose(file);

    if (read <= 0) {
        free(line);
        return NULL;
    }
    if (line[read - 1] == '\n') line[read - 1] = '\0';
    return line;
}

/* ---------- 数据目录 ---------- */
static char *get_current_app_data_dir(JavaVM *vm) {
    JNIEnv *env = NULL;
    const jint result = (*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6);
    tef_jni_clear_pending(env, "GetEnv");

    if (result == JNI_EDETACHED) {
        (*vm)->AttachCurrentThread(vm, &env, NULL);
    }
    if (!env) {
        TEKLOG_ERROR("Failed to get JNIEnv");
        return NULL;
    }

    jclass activity_thread_class = (*env)->FindClass(env, "android/app/ActivityThread");
    if (!activity_thread_class) {
        tef_jni_clear_pending(env, "FindClass ActivityThread");
        return NULL;
    }

    jmethodID current_activity_thread_method = (*env)->GetStaticMethodID(
        env, activity_thread_class, "currentActivityThread",
        "()Landroid/app/ActivityThread;");
    if (!current_activity_thread_method) {
        tef_jni_clear_pending(env, "GetStaticMethodID currentActivityThread");
        (*env)->DeleteLocalRef(env, activity_thread_class);
        return NULL;
    }

    jobject current_activity_thread = (*env)->CallStaticObjectMethod(
        env, activity_thread_class, current_activity_thread_method);
    tef_jni_clear_pending(env, "currentActivityThread");
    if (!current_activity_thread) {
        (*env)->DeleteLocalRef(env, activity_thread_class);
        return NULL;
    }

    jmethodID get_application_method = (*env)->GetMethodID(
        env, activity_thread_class, "getApplication", "()Landroid/app/Application;");
    if (!get_application_method) {
        tef_jni_clear_pending(env, "GetMethodID getApplication");
        (*env)->DeleteLocalRef(env, current_activity_thread);
        (*env)->DeleteLocalRef(env, activity_thread_class);
        return NULL;
    }

    jobject application = (*env)->CallObjectMethod(env, current_activity_thread, get_application_method);
    tef_jni_clear_pending(env, "getApplication");
    if (!application) {
        (*env)->DeleteLocalRef(env, current_activity_thread);
        (*env)->DeleteLocalRef(env, activity_thread_class);
        return NULL;
    }

    jclass context_class = (*env)->FindClass(env, "android/content/Context");
    if (!context_class) {
        tef_jni_clear_pending(env, "FindClass Context");
        (*env)->DeleteLocalRef(env, application);
        (*env)->DeleteLocalRef(env, current_activity_thread);
        (*env)->DeleteLocalRef(env, activity_thread_class);
        return NULL;
    }

    jmethodID get_files_dir_method = (*env)->GetMethodID(
        env, context_class, "getFilesDir", "()Ljava/io/File;");
    if (!get_files_dir_method) {
        tef_jni_clear_pending(env, "GetMethodID getFilesDir");
        (*env)->DeleteLocalRef(env, context_class);
        (*env)->DeleteLocalRef(env, application);
        (*env)->DeleteLocalRef(env, current_activity_thread);
        (*env)->DeleteLocalRef(env, activity_thread_class);
        return NULL;
    }

    jobject files_dir_obj = (*env)->CallObjectMethod(env, application, get_files_dir_method);
    tef_jni_clear_pending(env, "getFilesDir");
    if (!files_dir_obj) {
        (*env)->DeleteLocalRef(env, context_class);
        (*env)->DeleteLocalRef(env, application);
        (*env)->DeleteLocalRef(env, current_activity_thread);
        (*env)->DeleteLocalRef(env, activity_thread_class);
        return NULL;
    }

    jclass file_class = (*env)->FindClass(env, "java/io/File");
    if (!file_class) {
        tef_jni_clear_pending(env, "FindClass File");
        (*env)->DeleteLocalRef(env, files_dir_obj);
        (*env)->DeleteLocalRef(env, context_class);
        (*env)->DeleteLocalRef(env, application);
        (*env)->DeleteLocalRef(env, current_activity_thread);
        (*env)->DeleteLocalRef(env, activity_thread_class);
        return NULL;
    }

    jmethodID get_absolute_path_method = (*env)->GetMethodID(
        env, file_class, "getAbsolutePath", "()Ljava/lang/String;");
    if (!get_absolute_path_method) {
        tef_jni_clear_pending(env, "GetMethodID getAbsolutePath");
        (*env)->DeleteLocalRef(env, file_class);
        (*env)->DeleteLocalRef(env, files_dir_obj);
        (*env)->DeleteLocalRef(env, context_class);
        (*env)->DeleteLocalRef(env, application);
        (*env)->DeleteLocalRef(env, current_activity_thread);
        (*env)->DeleteLocalRef(env, activity_thread_class);
        return NULL;
    }

    const jstring path_str = (*env)->CallObjectMethod(env, files_dir_obj, get_absolute_path_method);
    tef_jni_clear_pending(env, "getAbsolutePath");
    const char *path = path_str ? (*env)->GetStringUTFChars(env, path_str, NULL) : NULL;
    char *data_dir = path ? strdup(path) : NULL;

    if (path) (*env)->ReleaseStringUTFChars(env, path_str, path);
    if (path_str) (*env)->DeleteLocalRef(env, path_str);
    (*env)->DeleteLocalRef(env, file_class);
    (*env)->DeleteLocalRef(env, files_dir_obj);
    (*env)->DeleteLocalRef(env, context_class);
    (*env)->DeleteLocalRef(env, application);
    (*env)->DeleteLocalRef(env, current_activity_thread);
    (*env)->DeleteLocalRef(env, activity_thread_class);

    if (result == JNI_EDETACHED) {
        (*vm)->DetachCurrentThread(vm);
    }
    return data_dir;
}

/* ---------- IL2CPP watcher ---------- */
static void *wait_for_il2cpp_lib(const int timeout_seconds) {
    TEKLOG_INFO("[xDL] Starting IL2CPP watcher, timeout: %d seconds", timeout_seconds);

    int total_wait_time = 0;
    const int max_wait_time = timeout_seconds * 1000;

    while (total_wait_time < max_wait_time && atomic_load(&il2cpp_watcher_running)) {
        const int check_interval_ms = 100;

        void *handle = xdl_open("libil2cpp.so", XDL_DEFAULT);
        if (handle) {
            void *test_symbol = xdl_sym(handle, "il2cpp_init", NULL);
            if (test_symbol) {
                TEKLOG_INFO("[xDL] Found valid IL2CPP library: %p (il2cpp_init at %p)",
                            handle, test_symbol);
                return handle;
            }
            xdl_close(handle);
        }

        usleep(check_interval_ms * 1000);
        total_wait_time += check_interval_ms;

        if (total_wait_time % 5000 == 0) {
            TEKLOG_DEBUG("[xDL] Still waiting for IL2CPP... (%dms/%dms)",
                         total_wait_time, max_wait_time);
        }
    }

    TEKLOG_WARN("[xDL] Timeout waiting for IL2CPP library");
    return NULL;
}

static void *il2cpp_watcher_thread_func(void *_nullable) {
    TEKLOG_INFO("[IL2CPP-Watcher] Starting IL2CPP watcher thread");

    void *handle = wait_for_il2cpp_lib(30);
    if (!handle) {
        TEKLOG_ERROR("[IL2CPP-Watcher] Failed to find IL2CPP library");
        atomic_store(&il2cpp_watcher_running, false);
        return NULL;
    }

    atomic_store(&il2cpp_handle, (uintptr_t)handle);
    atomic_store(&il2cpp_loaded, true);

    TEKLOG_INFO("[IL2CPP-Watcher] IL2CPP library found, handle: %p", handle);

    il2cpp_api_init(handle);

    TEKLOG_DEBUG("[IL2CPP-Watcher] Installing il2cpp_init hook");
    if (!shadowhook_hook_func_addr((void *)il2cpp_init,
                                   (void *)hook_il2cpp_init,
                                   (void **)&orig_il2cpp_init)) {
        TEKLOG_INFO("[IL2CPP-Watcher] il2cpp_init hook installed successfully");
    } else {
        TEKLOG_ERROR("[IL2CPP-Watcher] Failed to install il2cpp_init hook");
    }

    atomic_store(&il2cpp_watcher_running, false);
    TEKLOG_INFO("[IL2CPP-Watcher] IL2CPP watcher thread completed");
    return NULL;
}

static void start_iL2cpp_watcher(void) {
    if (atomic_load(&il2cpp_watcher_running) || atomic_load(&il2cpp_loaded)) {
        TEKLOG_DEBUG("[Main] IL2CPP watcher already running or IL2CPP already loaded");
        return;
    }

    TEKLOG_INFO("[Main] Starting IL2CPP watcher in background thread");
    atomic_store(&il2cpp_watcher_running, true);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&il2cpp_watcher_thread, &attr, il2cpp_watcher_thread_func, NULL) != 0) {
        TEKLOG_ERROR("[Main] Failed to create IL2CPP watcher thread");
        atomic_store(&il2cpp_watcher_running, false);
    }
    pthread_attr_destroy(&attr);
}

/* ---------- 构造函数 ---------- */
__attribute__((constructor))
static int init_ary(void) {
    TEKLOG_INFO("TEFKernel initializing");

    JavaVM *vm = get_java_vm_via_jni_standard();
    if (!vm) {
        TEKLOG_ERROR("Failed to get JavaVM");
        return -1;
    }

    // ShadowHook 初始化
    const int rc = shadowhook_init(SHADOWHOOK_MODE_MULTI, false);
    if (rc != 0)
        TEKLOG_ERROR("shadowhook_init failed: %s", shadowhook_to_errmsg(rc));


    char *data_dir = get_current_app_data_dir(vm);
    if (data_dir) {
        char config_path[512];
        snprintf(config_path, sizeof(config_path), "%s/tefkernel_working_dir", data_dir);

        if (access(config_path, F_OK) == 0) {
            char *line = read_first_line(config_path);
            if (line) {
                TEKLOG_INFO("Found config file, working dir: %s", line);
                free(tefkernel_working_dir);
                tefkernel_working_dir = strdup(line);
                free(line);
            } else {
                TEKLOG_WARN("Config file exists but cannot read");
            }
        } else {
            TEKLOG_DEBUG("Config file does not exist, will perform iohook");
            init_iohook(vm);
        }
        free(data_dir);
    } else {
        TEKLOG_WARN("Failed to get data directory, will continue with iohook");
        init_iohook(vm);
    }

    char log_dir[512];
    char log_config_filename[512] = {0};
    snprintf(log_dir, sizeof(log_dir), "%s/logs/tefkernel", tefkernel_working_dir);

    struct stat st;
    if (stat(log_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
        snprintf(log_config_filename, sizeof(log_config_filename), "%s/runtime", log_dir);
        fprintf(stderr, "[LOG] Log directory found: %s\n", log_dir);
    }

    if (log_config_filename[0] != '\0') {
        tefkernel_log_init(log_config_filename);
    } else {
        tefkernel_log_init(NULL);
    }

    TEKLOG_DEBUG("Setting up crash signal handlers");
    tefkernel_crash_handler_init();

    start_iL2cpp_watcher();

    TEKLOG_INFO("TEFKernel initialization completed successfully");
    return 0;
}