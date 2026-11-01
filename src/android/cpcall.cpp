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
static const char* AUTHORITY = "eternal.future.tefkernel.fileprovider";

// Cached JNI metadata structure
struct JniCache {
    std::mutex mtx;
    bool initialized = false;

    // Global references
    jclass bundleClass = nullptr;
    jclass resolverClass = nullptr;
    jclass uriClass = nullptr;
    jclass pfdClass = nullptr;

    jobject uri = nullptr;
    jobject resolver = nullptr;

    // Method IDs
    jmethodID resolverCall = nullptr;

    jmethodID bundleInit = nullptr;
    jmethodID bundlePutString = nullptr;
    jmethodID bundlePutInt = nullptr;
    jmethodID bundlePutLong = nullptr;
    jmethodID bundleGetBoolean = nullptr;
    jmethodID bundleGetInt = nullptr;
    jmethodID bundleGetLong = nullptr;
    jmethodID bundleGetString = nullptr;
    jmethodID bundleGetParcelable = nullptr;

    jmethodID pfdGetFd = nullptr;
    jmethodID pfdClose = nullptr;
};

static JniCache g_cache;

// RAII helper for JNI local reference frames
class JniLocalFrame {
public:
    explicit JniLocalFrame(JNIEnv* env, jint capacity = 16) : m_env(env) {
        if (m_env) {
            m_pushed = (m_env->PushLocalFrame(capacity) == 0);
        }
    }
    ~JniLocalFrame() {
        if (m_env && m_pushed) {
            m_env->PopLocalFrame(nullptr);
        }
    }
    JniLocalFrame(const JniLocalFrame&) = delete;
    JniLocalFrame& operator=(const JniLocalFrame&) = delete;

private:
    JNIEnv* m_env;
    bool m_pushed = false;
};

// 获取JNIEnv
/* JNI 铁律：Java 侧抛出异常后，必须先 ExceptionClear 才能继续调用 JNI，
 * 否则 ART 会在下一次 JNI 调用时 AssertNoPendingException → abort（SIGABRT）。
 * 这个桥接层原来 59 处 JNI 调用、0 处异常检查 —— 任何一次文件操作失败
 * （例如 FileNotFoundException: open failed: ENOENT）都会留下 pending 异常，
 * 表现为"玩一会儿随机闪退"。 */
static void clear_pending(JNIEnv* env, const char* where) {
    if (!env)
        return;
    if (env->ExceptionCheck()) {
        ALOGE("cpcall: 检测到未清理的 JNI 异常(%s)，已 ExceptionClear", where);
        env->ExceptionClear();
    }
}

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

    clear_pending(env, "getJNIEnv");
    return env;
}

// 缓存 JNI 元数据
static bool ensure_jni_cache(JNIEnv* env) {
    if (g_cache.initialized) {
        return true;
    }
    std::lock_guard<std::mutex> lock(g_cache.mtx);
    if (g_cache.initialized) {
        return true;
    }

    // 1. android/os/Bundle
    jclass localBundle = env->FindClass("android/os/Bundle");
    if (!localBundle) {
        clear_pending(env, "FindClass Bundle");
        return false;
    }
    g_cache.bundleClass = (jclass)env->NewGlobalRef(localBundle);
    env->DeleteLocalRef(localBundle);

    g_cache.bundleInit = env->GetMethodID(g_cache.bundleClass, "<init>", "()V");
    g_cache.bundlePutString = env->GetMethodID(g_cache.bundleClass, "putString", "(Ljava/lang/String;Ljava/lang/String;)V");
    g_cache.bundlePutInt = env->GetMethodID(g_cache.bundleClass, "putInt", "(Ljava/lang/String;I)V");
    g_cache.bundlePutLong = env->GetMethodID(g_cache.bundleClass, "putLong", "(Ljava/lang/String;J)V");
    g_cache.bundleGetBoolean = env->GetMethodID(g_cache.bundleClass, "getBoolean", "(Ljava/lang/String;)Z");
    g_cache.bundleGetInt = env->GetMethodID(g_cache.bundleClass, "getInt", "(Ljava/lang/String;)I");
    g_cache.bundleGetLong = env->GetMethodID(g_cache.bundleClass, "getLong", "(Ljava/lang/String;)J");
    g_cache.bundleGetString = env->GetMethodID(g_cache.bundleClass, "getString", "(Ljava/lang/String;)Ljava/lang/String;");
    g_cache.bundleGetParcelable = env->GetMethodID(g_cache.bundleClass, "getParcelable", "(Ljava/lang/String;)Landroid/os/Parcelable;");

    // 2. android/net/Uri
    jclass localUriClass = env->FindClass("android/net/Uri");
    if (!localUriClass) {
        clear_pending(env, "FindClass Uri");
        return false;
    }
    g_cache.uriClass = (jclass)env->NewGlobalRef(localUriClass);
    env->DeleteLocalRef(localUriClass);

    jmethodID uriParse = env->GetStaticMethodID(g_cache.uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    std::string uriStr = "content://";
    uriStr += AUTHORITY;
    jstring jUriStr = env->NewStringUTF(uriStr.c_str());
    jobject localUri = env->CallStaticObjectMethod(g_cache.uriClass, uriParse, jUriStr);
    clear_pending(env, "Uri.parse");
    env->DeleteLocalRef(jUriStr);
    if (localUri) {
        g_cache.uri = env->NewGlobalRef(localUri);
        env->DeleteLocalRef(localUri);
    }

    // 3. android/content/ContentResolver
    jclass localResolverClass = env->FindClass("android/content/ContentResolver");
    if (!localResolverClass) {
        clear_pending(env, "FindClass ContentResolver");
        return false;
    }
    g_cache.resolverClass = (jclass)env->NewGlobalRef(localResolverClass);
    env->DeleteLocalRef(localResolverClass);

    g_cache.resolverCall = env->GetMethodID(g_cache.resolverClass, "call",
            "(Landroid/net/Uri;Ljava/lang/String;Ljava/lang/String;Landroid/os/Bundle;)Landroid/os/Bundle;");

    // 4. android/os/ParcelFileDescriptor
    jclass localPfdClass = env->FindClass("android/os/ParcelFileDescriptor");
    if (localPfdClass) {
        g_cache.pfdClass = (jclass)env->NewGlobalRef(localPfdClass);
        env->DeleteLocalRef(localPfdClass);

        g_cache.pfdGetFd = env->GetMethodID(g_cache.pfdClass, "getFd", "()I");
        g_cache.pfdClose = env->GetMethodID(g_cache.pfdClass, "close", "()V");
    } else {
        clear_pending(env, "FindClass ParcelFileDescriptor");
    }

    if (!g_cache.bundleInit || !g_cache.bundlePutString || !g_cache.bundlePutInt ||
        !g_cache.bundlePutLong || !g_cache.bundleGetBoolean || !g_cache.bundleGetInt ||
        !g_cache.bundleGetLong || !g_cache.bundleGetString || !g_cache.bundleGetParcelable ||
        !g_cache.uri || !g_cache.resolverCall) {
        clear_pending(env, "ensure_jni_cache verification");
        ALOGE("cpcall: Failed to resolve all required JNI metadata");
        return false;
    }

    clear_pending(env, "ensure_jni_cache");
    g_cache.initialized = true;
    ALOGD("cpcall: JNI cache initialized successfully");
    return true;
}

// 获取并缓存 ContentResolver 全局引用
static jobject getContentResolver(JNIEnv* env) {
    if (g_cache.resolver) {
        return g_cache.resolver;
    }
    std::lock_guard<std::mutex> lock(g_cache.mtx);
    if (g_cache.resolver) {
        return g_cache.resolver;
    }

    jclass activityThread = env->FindClass("android/app/ActivityThread");
    if (!activityThread) {
        clear_pending(env, "FindClass ActivityThread");
        return nullptr;
    }
    jmethodID currentActivityThread = env->GetStaticMethodID(
            activityThread, "currentActivityThread", "()Landroid/app/ActivityThread;");
    if (!currentActivityThread) {
        clear_pending(env, "GetStaticMethodID currentActivityThread");
        env->DeleteLocalRef(activityThread);
        return nullptr;
    }
    jobject at = env->CallStaticObjectMethod(activityThread, currentActivityThread);
    clear_pending(env, "CallStaticObjectMethod currentActivityThread");
    env->DeleteLocalRef(activityThread);
    if (!at) {
        return nullptr;
    }

    jclass atClass = env->GetObjectClass(at);
    jmethodID getApplication = env->GetMethodID(
            atClass, "getApplication", "()Landroid/app/Application;");
    if (!getApplication) {
        clear_pending(env, "GetMethodID getApplication");
        env->DeleteLocalRef(atClass);
        env->DeleteLocalRef(at);
        return nullptr;
    }
    jobject context = env->CallObjectMethod(at, getApplication);
    clear_pending(env, "CallObjectMethod getApplication");
    env->DeleteLocalRef(atClass);
    env->DeleteLocalRef(at);
    if (!context) {
        return nullptr;
    }

    jclass contextClass = env->GetObjectClass(context);
    jmethodID getResolverMethod = env->GetMethodID(
            contextClass, "getContentResolver", "()Landroid/content/ContentResolver;");
    if (!getResolverMethod) {
        clear_pending(env, "GetMethodID getContentResolver");
        env->DeleteLocalRef(contextClass);
        env->DeleteLocalRef(context);
        return nullptr;
    }
    jobject localResolver = env->CallObjectMethod(context, getResolverMethod);
    clear_pending(env, "CallObjectMethod getContentResolver");
    env->DeleteLocalRef(contextClass);
    env->DeleteLocalRef(context);

    if (localResolver) {
        g_cache.resolver = env->NewGlobalRef(localResolver);
        env->DeleteLocalRef(localResolver);
    }
    return g_cache.resolver;
}

// 初始化
void cp_call_init(JavaVM* vm) {
    std::lock_guard<std::mutex> lock(g_vm_mutex);
    g_vm = vm;
    JNIEnv* env = nullptr;
    if (g_vm && g_vm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_OK) {
        ensure_jni_cache(env);
        getContentResolver(env);
    }
    ALOGD("CPCall initialized");
}

// 工具函数
static jobject createBundle(JNIEnv* env) {
    if (!ensure_jni_cache(env) || !g_cache.bundleClass || !g_cache.bundleInit) {
        return nullptr;
    }
    return env->NewObject(g_cache.bundleClass, g_cache.bundleInit);
}

static void putString(JNIEnv* env, jobject bundle, const char* key, const char* value) {
    if (!env || !bundle || !key || !value || !ensure_jni_cache(env)) return;
    jstring jKey = env->NewStringUTF(key);
    jstring jValue = env->NewStringUTF(value);
    env->CallVoidMethod(bundle, g_cache.bundlePutString, jKey, jValue);
    clear_pending(env, "Bundle.putString");
    env->DeleteLocalRef(jKey);
    env->DeleteLocalRef(jValue);
}

static void putInt(JNIEnv* env, jobject bundle, const char* key, jint value) {
    if (!env || !bundle || !key || !ensure_jni_cache(env)) return;
    jstring jKey = env->NewStringUTF(key);
    env->CallVoidMethod(bundle, g_cache.bundlePutInt, jKey, value);
    clear_pending(env, "Bundle.putInt");
    env->DeleteLocalRef(jKey);
}

static void putLong(JNIEnv* env, jobject bundle, const char* key, jlong value) {
    if (!env || !bundle || !key || !ensure_jni_cache(env)) return;
    jstring jKey = env->NewStringUTF(key);
    env->CallVoidMethod(bundle, g_cache.bundlePutLong, jKey, value);
    clear_pending(env, "Bundle.putLong");
    env->DeleteLocalRef(jKey);
}

static bool getBoolean(JNIEnv* env, jobject bundle, const char* key) {
    if (!env || !bundle || !key || !ensure_jni_cache(env)) return false;
    jstring jKey = env->NewStringUTF(key);
    jboolean result = env->CallBooleanMethod(bundle, g_cache.bundleGetBoolean, jKey);
    clear_pending(env, "Bundle.getBoolean");
    env->DeleteLocalRef(jKey);
    return result;
}

static int getInt(JNIEnv* env, jobject bundle, const char* key) {
    if (!env || !bundle || !key || !ensure_jni_cache(env)) return 0;
    jstring jKey = env->NewStringUTF(key);
    jint result = env->CallIntMethod(bundle, g_cache.bundleGetInt, jKey);
    clear_pending(env, "Bundle.getInt");
    env->DeleteLocalRef(jKey);
    return result;
}

static int64_t getLong(JNIEnv* env, jobject bundle, const char* key) {
    if (!env || !bundle || !key || !ensure_jni_cache(env)) return 0;
    jstring jKey = env->NewStringUTF(key);
    jlong result = env->CallLongMethod(bundle, g_cache.bundleGetLong, jKey);
    clear_pending(env, "Bundle.getLong");
    env->DeleteLocalRef(jKey);
    return (int64_t)result;
}

static std::string getString(JNIEnv* env, jobject bundle, const char* key) {
    if (!env || !bundle || !key || !ensure_jni_cache(env)) return "";
    jstring jKey = env->NewStringUTF(key);
    auto jResult = (jstring)env->CallObjectMethod(bundle, g_cache.bundleGetString, jKey);
    clear_pending(env, "Bundle.getString");
    env->DeleteLocalRef(jKey);

    if (!jResult) return "";

    const char* cstr = env->GetStringUTFChars(jResult, nullptr);
    std::string result(cstr ? cstr : "");
    if (cstr) {
        env->ReleaseStringUTFChars(jResult, cstr);
    }
    env->DeleteLocalRef(jResult);

    return result;
}

// 获取字符串并分配内存
static char* getStringAndAllocate(JNIEnv* env, jobject bundle, const char* key) {
    if (!env || !bundle || !key || !ensure_jni_cache(env)) return nullptr;
    jstring jKey = env->NewStringUTF(key);
    auto jResult = (jstring)env->CallObjectMethod(bundle, g_cache.bundleGetString, jKey);
    clear_pending(env, "Bundle.getString");
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
        memcpy(result, cstr, len + 1);
    }

    env->ReleaseStringUTFChars(jResult, cstr);
    env->DeleteLocalRef(jResult);

    return result;
}

// 通用call函数
static jobject callJavaMethod(const char* method, jobject extras) {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        ALOGE("Cannot get JNIEnv for method: %s", method);
        return nullptr;
    }

    if (!ensure_jni_cache(env)) {
        ALOGE("Cannot initialize JNI cache for method: %s", method);
        return nullptr;
    }

    jobject resolver = getContentResolver(env);
    if (!resolver) {
        ALOGE("Cannot get ContentResolver for method: %s", method);
        return nullptr;
    }

    if (!g_cache.uri || !g_cache.resolverCall) {
        ALOGE("JNI metadata missing for uri or call method: %s", method);
        return nullptr;
    }

    jstring jMethod = env->NewStringUTF(method);
    jobject result = env->CallObjectMethod(resolver, g_cache.resolverCall, g_cache.uri, jMethod, nullptr, extras);
    if (env->ExceptionCheck()) {
        ALOGE("cpcall: contentResolver.call(%s) 抛出异常，已清理并返回失败", method);
        env->ExceptionClear();
        result = nullptr;
    }

    env->DeleteLocalRef(jMethod);
    return result;
}

// 通用call函数（无extras版本）
static jobject callJavaMethodNoExtras(const char* method) {
    return callJavaMethod(method, nullptr);
}

// 路径获取函数实现
char* cp_call_get_internal_dir() {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        ALOGE("Cannot get JNIEnv for getInternalDir");
        return nullptr;
    }
    JniLocalFrame frame(env, 16);

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
    JniLocalFrame frame(env, 16);

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
    JniLocalFrame frame(env, 16);

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
    JniLocalFrame frame(env, 16);

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

int cp_call_open(const char* path, int flags, mode_t mode) {
    JNIEnv* env = getJNIEnv();
    if (!env) {
        errno = EIO;
        return -1;
    }
    JniLocalFrame frame(env, 16);

    jobject extras = createBundle(env);
    if (!extras) {
        errno = EIO;
        return -1;
    }
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
        jstring fdKey = env->NewStringUTF("fd");
        jobject pfdObj = env->CallObjectMethod(result, g_cache.bundleGetParcelable, fdKey);
        clear_pending(env, "Bundle.getParcelable");
        env->DeleteLocalRef(fdKey);

        if (pfdObj) {
            // 获取Java层的文件描述符
            jint javaFd = env->CallIntMethod(pfdObj, g_cache.pfdGetFd);
            clear_pending(env, "ParcelFileDescriptor.getFd");

            // 复制文件描述符（dup是必要的）
            if (javaFd >= 0) {
                fd = dup(javaFd);
                if (fd < 0) {
                    ALOGE("dup failed: %s", strerror(errno));
                }
            } else {
                ALOGE("getFd returned invalid fd: %d", javaFd);
                errno = EBADF;
            }

            // 关闭Java层的ParcelFileDescriptor
            env->CallVoidMethod(pfdObj, g_cache.pfdClose);
            clear_pending(env, "ParcelFileDescriptor.close");

            env->DeleteLocalRef(pfdObj);
        } else {
            ALOGE("Failed to get ParcelFileDescriptor from bundle");
            errno = EIO;
        }
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
    JniLocalFrame frame(env, 16);

    jobject extras = createBundle(env);
    if (!extras) {
        errno = EIO;
        return -1;
    }
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
    JniLocalFrame frame(env, 16);

    jobject extras = createBundle(env);
    if (!extras) {
        errno = EIO;
        return -1;
    }
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
    JniLocalFrame frame(env, 16);

    jobject extras = createBundle(env);
    if (!extras) {
        errno = EIO;
        return -1;
    }
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
    JniLocalFrame frame(env, 16);

    jobject extras = createBundle(env);
    if (!extras) {
        errno = EIO;
        return -1;
    }
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
    const bool is_dir = getBoolean(env, result, "is_dir");
    buf->st_mode = is_dir ? (S_IFDIR | 0755) : (S_IFREG | 0644);

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
    JniLocalFrame frame(env, 16);

    jobject extras = createBundle(env);
    if (!extras) {
        errno = EIO;
        return -1;
    }
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
    JniLocalFrame frame(env, 16);

    jobject extras = createBundle(env);
    if (!extras) {
        errno = EIO;
        return nullptr;
    }
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
    JniLocalFrame frame(env, 16);

    jobject extras = createBundle(env);
    if (!extras) {
        errno = EIO;
        return nullptr;
    }
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
    JniLocalFrame frame(env, 16);

    jobject extras = createBundle(env);
    if (!extras) {
        errno = EIO;
        return -1;
    }
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