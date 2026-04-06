/*******************************************************************************
 * tefkernel - cpcall
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
 * Created: 2026/4/4
 *******************************************************************************/

#include "cpcall.h"
#include <android/log.h>
#include <string>
#include <mutex>
#include <vector>
#include <cstring>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#define LOG_TAG "CPCall"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// 全局变量
static JavaVM* g_vm = nullptr;
static std::mutex g_vm_mutex;

// ContentProvider信息
static auto AUTHORITY = "eternal.future.tefkernel.fileprovider";

// 初始化
void cp_call_init(JavaVM* vm) {
    std::lock_guard lock(g_vm_mutex);
    g_vm = vm;
    ALOGD("CPCall initialized");
}

// 获取JNIEnv
static JNIEnv* getJNIEnv() {
    if (!g_vm) {
        ALOGE("JavaVM not initialized");
        return nullptr;
    }

    JNIEnv* env = nullptr;
    jint status = g_vm->GetEnv((void**)&env, JNI_VERSION_1_6);

    if (status == JNI_EDETACHED) {
        std::lock_guard<std::mutex> lock(g_vm_mutex);
        JavaVMAttachArgs args = {JNI_VERSION_1_6, "CPCall-Thread", nullptr};
        if (g_vm->AttachCurrentThread(&env, &args) != JNI_OK) {
            ALOGE("Failed to attach thread");
            return nullptr;
        }
    } else if (status != JNI_OK) {
        ALOGE("Failed to get JNIEnv: %d", status);
        return nullptr;
    }

    return env;
}

// 工具函数
static jobject getContentResolver(JNIEnv* env) {
    jclass activityThread = env->FindClass("android/app/ActivityThread");
    jmethodID currentActivityThread = env->GetStaticMethodID(
            activityThread, "currentActivityThread", "()Landroid/app/ActivityThread;");
    jobject at = env->CallStaticObjectMethod(activityThread, currentActivityThread);

    jmethodID getApplication = env->GetMethodID(
            activityThread, "getApplication", "()Landroid/app/Application;");
    jobject context = env->CallObjectMethod(at, getApplication);

    jclass contextClass = env->GetObjectClass(context);
    jobject resolver = env->CallObjectMethod(context,
            env->GetMethodID(contextClass, "getContentResolver", "()Landroid/content/ContentResolver;"));

    env->DeleteLocalRef(activityThread);
    env->DeleteLocalRef(at);
    env->DeleteLocalRef(context);
    env->DeleteLocalRef(contextClass);

    return resolver;
}

static jobject createBundle(JNIEnv* env) {
    jclass bundleClass = env->FindClass("android/os/Bundle");
    jmethodID constructor = env->GetMethodID(bundleClass, "<init>", "()V");
    return env->NewObject(bundleClass, constructor);
}

static void putString(JNIEnv* env, jobject bundle, const char* key, const char* value) {
    jclass bundleClass = env->FindClass("android/os/Bundle");
    jmethodID putString = env->GetMethodID(bundleClass, "putString",
            "(Ljava/lang/String;Ljava/lang/String;)V");
    jstring jKey = env->NewStringUTF(key);
    jstring jValue = env->NewStringUTF(value);
    env->CallVoidMethod(bundle, putString, jKey, jValue);
    env->DeleteLocalRef(jKey);
    env->DeleteLocalRef(jValue);
}

static void putInt(JNIEnv* env, jobject bundle, const char* key, jint value) {
    jclass bundleClass = env->FindClass("android/os/Bundle");
    jmethodID putInt = env->GetMethodID(bundleClass, "putInt", "(Ljava/lang/String;I)V");
    jstring jKey = env->NewStringUTF(key);
    env->CallVoidMethod(bundle, putInt, jKey, value);
    env->DeleteLocalRef(jKey);
}

static void putLong(JNIEnv* env, jobject bundle, const char* key, jlong value) {
    jclass bundleClass = env->FindClass("android/os/Bundle");
    jmethodID putLong = env->GetMethodID(bundleClass, "putLong", "(Ljava/lang/String;J)V");
    jstring jKey = env->NewStringUTF(key);
    env->CallVoidMethod(bundle, putLong, jKey, value);
    env->DeleteLocalRef(jKey);
}

static bool getBoolean(JNIEnv* env, jobject bundle, const char* key) {
    jclass bundleClass = env->FindClass("android/os/Bundle");
    jmethodID getBoolean = env->GetMethodID(bundleClass, "getBoolean", "(Ljava/lang/String;)Z");
    jstring jKey = env->NewStringUTF(key);
    jboolean result = env->CallBooleanMethod(bundle, getBoolean, jKey);
    env->DeleteLocalRef(jKey);
    return result;
}

static int getInt(JNIEnv* env, jobject bundle, const char* key) {
    jclass bundleClass = env->FindClass("android/os/Bundle");
    jmethodID getInt = env->GetMethodID(bundleClass, "getInt", "(Ljava/lang/String;)I");
    jstring jKey = env->NewStringUTF(key);
    jint result = env->CallIntMethod(bundle, getInt, jKey);
    env->DeleteLocalRef(jKey);
    return result;
}

static int64_t getLong(JNIEnv* env, jobject bundle, const char* key) {
    jclass bundleClass = env->FindClass("android/os/Bundle");
    jmethodID getLong = env->GetMethodID(bundleClass, "getLong", "(Ljava/lang/String;)J");
    jstring jKey = env->NewStringUTF(key);
    jlong result = env->CallLongMethod(bundle, getLong, jKey);
    env->DeleteLocalRef(jKey);
    return (int64_t)result;
}

static std::string getString(JNIEnv* env, jobject bundle, const char* key) {
    jclass bundleClass = env->FindClass("android/os/Bundle");
    jmethodID getString = env->GetMethodID(bundleClass, "getString", "(Ljava/lang/String;)Ljava/lang/String;");
    jstring jKey = env->NewStringUTF(key);
    auto jResult = (jstring)env->CallObjectMethod(bundle, getString, jKey);
    env->DeleteLocalRef(jKey);

    if (!jResult) return "";

    const char* cstr = env->GetStringUTFChars(jResult, nullptr);
    std::string result(cstr);
    env->ReleaseStringUTFChars(jResult, cstr);
    env->DeleteLocalRef(jResult);

    return result;
}

// 新增：获取字符串并分配内存
static char* getStringAndAllocate(JNIEnv* env, jobject bundle, const char* key) {
    jclass bundleClass = env->FindClass("android/os/Bundle");
    jmethodID getString = env->GetMethodID(bundleClass, "getString", "(Ljava/lang/String;)Ljava/lang/String;");
    jstring jKey = env->NewStringUTF(key);
    auto jResult = (jstring)env->CallObjectMethod(bundle, getString, jKey);
    env->DeleteLocalRef(jKey);

    if (!jResult) {
        ALOGE("No string value for key: %s", key);
        return nullptr;
    }

    const char* cstr = env->GetStringUTFChars(jResult, nullptr);
    if (!cstr) {
        env->DeleteLocalRef(jResult);
        return nullptr;
    }

    // 分配内存并复制字符串
    size_t len = strlen(cstr);
    char* result = (char*)malloc(len + 1);
    if (result) {
        strcpy(result, cstr);
    }

    env->ReleaseStringUTFChars(jResult, cstr);
    env->DeleteLocalRef(jResult);

    return result;
}

// 通用call函数（无extras版本）
static jobject callJavaMethodNoExtras(const char* method) {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        ALOGE("Cannot get JNIEnv for method: %s", method);
        return nullptr;
    }

    // 获取ContentResolver
    jobject resolver = getContentResolver(env);
    if (!resolver) {
        ALOGE("Cannot get ContentResolver");
        return nullptr;
    }

    // 构建URI
    jclass uriClass = env->FindClass("android/net/Uri");
    std::string uriStr = "content://";
    uriStr += AUTHORITY;
    jstring jUriStr = env->NewStringUTF(uriStr.c_str());
    jobject uri = env->CallStaticObjectMethod(uriClass,
            env->GetStaticMethodID(uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;"), jUriStr);

    // 调用call方法
    jclass resolverClass = env->FindClass("android/content/ContentResolver");
    jmethodID callMethod = env->GetMethodID(resolverClass, "call",
            "(Landroid/net/Uri;Ljava/lang/String;Ljava/lang/String;Landroid/os/Bundle;)Landroid/os/Bundle;");

    jstring jMethod = env->NewStringUTF(method);
    jobject result = env->CallObjectMethod(resolver, callMethod, uri, jMethod, nullptr, nullptr);

    // 清理
    env->DeleteLocalRef(jMethod);
    env->DeleteLocalRef(jUriStr);
    env->DeleteLocalRef(uri);
    env->DeleteLocalRef(uriClass);
    env->DeleteLocalRef(resolverClass);
    env->DeleteLocalRef(resolver);

    return result;
}

// 新增的路径获取函数实现
char* cp_call_get_internal_dir() {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        ALOGE("Cannot get JNIEnv for getInternalDir");
        return nullptr;
    }

    jobject result = callJavaMethodNoExtras("getInternalDir");
    if (!result) {
        ALOGE("Failed to call getInternalDir");
        return nullptr;
    }

    bool success = getBoolean(env, result, "success");
    if (!success) {
        ALOGE("getInternalDir failed");
        env->DeleteLocalRef(result);
        return nullptr;
    }

    char* path = getStringAndAllocate(env, result, "dir_path");
    env->DeleteLocalRef(result);

    if (path) {
        ALOGD("getInternalDir returned: %s", path);
    } else {
        ALOGE("Failed to get path from getInternalDir result");
    }

    return path;
}

char* cp_call_get_external_dir() {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        ALOGE("Cannot get JNIEnv for getExternalDir");
        return nullptr;
    }

    jobject result = callJavaMethodNoExtras("getExternalDir");
    if (!result) {
        ALOGE("Failed to call getExternalDir");
        return nullptr;
    }

    bool success = getBoolean(env, result, "success");
    if (!success) {
        ALOGE("getExternalDir failed");
        env->DeleteLocalRef(result);
        return nullptr;
    }

    char* path = getStringAndAllocate(env, result, "dir_path");
    env->DeleteLocalRef(result);

    if (path) {
        ALOGD("getExternalDir returned: %s", path);
    } else {
        ALOGE("Failed to get path from getExternalDir result");
    }

    return path;
}

char* cp_call_get_cache_dir() {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        ALOGE("Cannot get JNIEnv for getCacheDir");
        return nullptr;
    }

    jobject result = callJavaMethodNoExtras("getCacheDir");
    if (!result) {
        ALOGE("Failed to call getCacheDir");
        return nullptr;
    }

    bool success = getBoolean(env, result, "success");
    if (!success) {
        ALOGE("getCacheDir failed");
        env->DeleteLocalRef(result);
        return nullptr;
    }

    char* path = getStringAndAllocate(env, result, "dir_path");
    env->DeleteLocalRef(result);

    if (path) {
        ALOGD("getCacheDir returned: %s", path);
    } else {
        ALOGE("Failed to get path from getCacheDir result");
    }

    return path;
}

char* cp_call_get_external_cache_dir() {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        ALOGE("Cannot get JNIEnv for getExternalCacheDir");
        return nullptr;
    }

    jobject result = callJavaMethodNoExtras("getExternalCacheDir");
    if (!result) {
        ALOGE("Failed to call getExternalCacheDir");
        return nullptr;
    }

    bool success = getBoolean(env, result, "success");
    if (!success) {
        ALOGE("getExternalCacheDir failed");
        env->DeleteLocalRef(result);
        return nullptr;
    }

    char* path = getStringAndAllocate(env, result, "dir_path");
    env->DeleteLocalRef(result);

    if (path) {
        ALOGD("getExternalCacheDir returned: %s", path);
    } else {
        ALOGE("Failed to get path from getExternalCacheDir result");
    }

    return path;
}

// 通用call函数
static jobject callJavaMethod(const char* method, jobject extras) {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        ALOGE("Cannot get JNIEnv for method: %s", method);
        return nullptr;
    }

    // 获取ContentResolver
    jobject resolver = getContentResolver(env);
    if (!resolver) {
        ALOGE("Cannot get ContentResolver");
        return nullptr;
    }

    // 构建URI
    jclass uriClass = env->FindClass("android/net/Uri");
    std::string uriStr = "content://";
    uriStr += AUTHORITY;
    jstring jUriStr = env->NewStringUTF(uriStr.c_str());
    jobject uri = env->CallStaticObjectMethod(uriClass,
            env->GetStaticMethodID(uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;"), jUriStr);

    // 调用call方法
    jclass resolverClass = env->FindClass("android/content/ContentResolver");
    jmethodID callMethod = env->GetMethodID(resolverClass, "call",
            "(Landroid/net/Uri;Ljava/lang/String;Ljava/lang/String;Landroid/os/Bundle;)Landroid/os/Bundle;");

    jstring jMethod = env->NewStringUTF(method);
    jobject result = env->CallObjectMethod(resolver, callMethod, uri, jMethod, nullptr, extras);

    // 清理
    env->DeleteLocalRef(jMethod);
    env->DeleteLocalRef(jUriStr);
    env->DeleteLocalRef(uri);
    env->DeleteLocalRef(uriClass);
    env->DeleteLocalRef(resolverClass);
    env->DeleteLocalRef(resolver);

    return result;
}

int cp_call_open(const char* path, int flags, mode_t mode) {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        errno = EIO;
        return -1;
    }

    jobject extras = createBundle(env);
    putString(env, extras, "path", path);
    putInt(env, extras, "flags", flags);

    // 转换flags到mode字符串
    const char* modeStr = "r";
    if ((flags & O_ACCMODE) == O_RDWR) {
        modeStr = "rw";
    } else if (flags & O_WRONLY) {
        if (flags & O_APPEND) {
            modeStr = "wa";
        } else if (flags & O_TRUNC) {
            modeStr = "wt";
        } else {
            modeStr = "w";
        }
    }
    putString(env, extras, "mode", modeStr);

    jobject result = callJavaMethod("open", extras);
    if (!result) {
        env->DeleteLocalRef(extras);
        errno = EIO;
        return -1;
    }

    bool success = getBoolean(env, result, "success");
    int fd = -1;

    if (success) {
        // 从Bundle中获取ParcelFileDescriptor对象
        jclass bundleClass = env->FindClass("android/os/Bundle");
        jmethodID getParcelable = env->GetMethodID(bundleClass,
                "getParcelable", "(Ljava/lang/String;)Landroid/os/Parcelable;");
        jstring fdKey = env->NewStringUTF("fd");

        jobject pfdObj = env->CallObjectMethod(result, getParcelable, fdKey);

        if (pfdObj) {
            // 获取ParcelFileDescriptor的fd
            jclass pfdClass = env->FindClass("android/os/ParcelFileDescriptor");
            jmethodID getFd = env->GetMethodID(pfdClass, "getFd", "()I");

            // 获取Java层的文件描述符
            jint javaFd = env->CallIntMethod(pfdObj, getFd);

            // 复制文件描述符（dup是必要的）
            if (javaFd >= 0) {
                fd = dup(javaFd);

                if (fd < 0) {
                    // dup失败
                    ALOGE("dup failed: %s", strerror(errno));
                }
            } else {
                ALOGE("getFd returned invalid fd: %d", javaFd);
                errno = EBADF;
            }

            // 关闭Java层的ParcelFileDescriptor
            jmethodID closeMethod = env->GetMethodID(pfdClass, "close", "()V");
            env->CallVoidMethod(pfdObj, closeMethod);

            env->DeleteLocalRef(pfdClass);
            env->DeleteLocalRef(pfdObj);
        } else {
            ALOGE("Failed to get ParcelFileDescriptor from bundle");
            errno = EIO;
        }

        env->DeleteLocalRef(fdKey);
        env->DeleteLocalRef(bundleClass);
    } else {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
    }

    env->DeleteLocalRef(extras);
    env->DeleteLocalRef(result);

    return fd;
}

int cp_call_unlink(const char* path) {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        errno = EIO;
        return -1;
    }

    jobject extras = createBundle(env);
    putString(env, extras, "path", path);

    jobject result = callJavaMethod("unlink", extras);
    if (!result) {
        env->DeleteLocalRef(extras);
        errno = EIO;
        return -1;
    }

    bool success = getBoolean(env, result, "success");
    int ret = success ? 0 : -1;
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
    }

    env->DeleteLocalRef(extras);
    env->DeleteLocalRef(result);

    return ret;
}

int cp_call_rmdir(const char* path) {
    return cp_call_unlink(path); // 实现相同
}

int cp_call_mkdir(const char* path, mode_t mode) {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        errno = EIO;
        return -1;
    }

    jobject extras = createBundle(env);
    putString(env, extras, "path", path);
    putInt(env, extras, "mode", mode);

    jobject result = callJavaMethod("mkdir", extras);
    if (!result) {
        env->DeleteLocalRef(extras);
        errno = EIO;
        return -1;
    }

    bool success = getBoolean(env, result, "success");
    int ret = success ? 0 : -1;
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
    }

    env->DeleteLocalRef(extras);
    env->DeleteLocalRef(result);

    return ret;
}

int cp_call_rename(const char* oldpath, const char* newpath) {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        errno = EIO;
        return -1;
    }

    jobject extras = createBundle(env);
    putString(env, extras, "path", oldpath);
    putString(env, extras, "path2", newpath);

    jobject result = callJavaMethod("rename", extras);
    if (!result) {
        env->DeleteLocalRef(extras);
        errno = EIO;
        return -1;
    }

    bool success = getBoolean(env, result, "success");
    int ret = success ? 0 : -1;
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
    }

    env->DeleteLocalRef(extras);
    env->DeleteLocalRef(result);

    return ret;
}

int cp_call_stat(const char* path, struct stat* buf) {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        errno = EIO;
        return -1;
    }

    jobject extras = createBundle(env);
    putString(env, extras, "path", path);

    jobject result = callJavaMethod("stat", extras);
    if (!result) {
        env->DeleteLocalRef(extras);
        errno = EIO;
        return -1;
    }

    bool success = getBoolean(env, result, "success");
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
        env->DeleteLocalRef(extras);
        env->DeleteLocalRef(result);
        return -1;
    }

    // 填充stat结构
    memset(buf, 0, sizeof(struct stat));
    buf->st_size = getLong(env, result, "file_size");
    buf->st_mtime = getLong(env, result, "last_modified");
    buf->st_mode = getBoolean(env, result, "is_dir") ? S_IFDIR : S_IFREG;
    buf->st_mode |= 0644; // 默认权限

    env->DeleteLocalRef(extras);
    env->DeleteLocalRef(result);

    return 0;
}

int cp_call_access(const char* path, int mode) {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        errno = EIO;
        return -1;
    }

    jobject extras = createBundle(env);
    putString(env, extras, "path", path);
    putInt(env, extras, "mode", mode);

    jobject result = callJavaMethod("access", extras);
    if (!result) {
        env->DeleteLocalRef(extras);
        errno = EIO;
        return -1;
    }

    bool success = getBoolean(env, result, "success");
    int ret = success ? 0 : -1;
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
    }

    env->DeleteLocalRef(extras);
    env->DeleteLocalRef(result);

    return ret;
}

DIR* cp_call_opendir(const char* name) {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        errno = EIO;
        return nullptr;
    }

    jobject extras = createBundle(env);
    putString(env, extras, "path", name);

    jobject result = callJavaMethod("opendir", extras);
    if (!result) {
        env->DeleteLocalRef(extras);
        errno = EIO;
        return nullptr;
    }

    bool success = getBoolean(env, result, "success");
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
        env->DeleteLocalRef(extras);
        env->DeleteLocalRef(result);
        return nullptr;
    }

    // 通过打开目录然后fdopendir实现
    int fd = cp_call_open(name, O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        env->DeleteLocalRef(extras);
        env->DeleteLocalRef(result);
        return nullptr;
    }

    DIR* dir = fdopendir(fd);
    if (!dir) {
        close(fd);
    }

    env->DeleteLocalRef(extras);
    env->DeleteLocalRef(result);

    return dir;
}

char* cp_call_realpath(const char* path, char* resolved_path) {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        errno = EIO;
        return nullptr;
    }

    jobject extras = createBundle(env);
    putString(env, extras, "path", path);

    jobject result = callJavaMethod("realpath", extras);
    if (!result) {
        env->DeleteLocalRef(extras);
        errno = EIO;
        return nullptr;
    }

    bool success = getBoolean(env, result, "success");
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
        env->DeleteLocalRef(extras);
        env->DeleteLocalRef(result);
        return nullptr;
    }

    std::string realpath = getString(env, result, "result");

    if (resolved_path == nullptr) {
        // 调用者需要分配内存
        resolved_path = (char*)malloc(realpath.length() + 1);
    }

    if (resolved_path) {
        strcpy(resolved_path, realpath.c_str());
    }

    env->DeleteLocalRef(extras);
    env->DeleteLocalRef(result);

    return resolved_path;
}

int cp_call_truncate(const char* path, off_t length) {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        errno = EIO;
        return -1;
    }

    jobject extras = createBundle(env);
    putString(env, extras, "path", path);
    putLong(env, extras, "size", length);

    jobject result = callJavaMethod("truncate", extras);
    if (!result) {
        env->DeleteLocalRef(extras);
        errno = EIO;
        return -1;
    }

    bool success = getBoolean(env, result, "success");
    int ret = success ? 0 : -1;
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
    }

    env->DeleteLocalRef(extras);
    env->DeleteLocalRef(result);

    return ret;
}