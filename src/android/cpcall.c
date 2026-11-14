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
 * Created: 2026/11/14
 *******************************************************************************/

#include "cpcall.h"

#include <android/log.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#define LOG_TAG "CPCall"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

static JavaVM *g_vm = NULL;
static pthread_mutex_t g_vm_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *AUTHORITY = "eternal.future.tefkernel.fileprovider";

typedef struct {
    pthread_mutex_t mtx;
    bool initialized;

    jclass bundleClass;
    jclass resolverClass;
    jclass uriClass;
    jclass pfdClass;

    jobject uri;
    jobject resolver;

    jmethodID resolverCall;

    jmethodID bundleInit;
    jmethodID bundlePutString;
    jmethodID bundlePutInt;
    jmethodID bundlePutLong;
    jmethodID bundleGetBoolean;
    jmethodID bundleGetInt;
    jmethodID bundleGetLong;
    jmethodID bundleGetString;
    jmethodID bundleGetParcelable;

    jmethodID pfdGetFd;
    jmethodID pfdClose;
} JniCache;

static JniCache g_cache = {
    .mtx = PTHREAD_MUTEX_INITIALIZER,
    .initialized = false
};

/* ---------- JNI 异常清理 ---------- */
static void clear_pending(JNIEnv *env, const char *where) {
    if (!env) return;
    if ((*env)->ExceptionCheck(env)) {
        ALOGE("cpcall: 检测到未清理的 JNI 异常(%s)，已 ExceptionClear", where);
        (*env)->ExceptionClear(env);
    }
}

/* ---------- 获取 JNIEnv ---------- */
static JNIEnv *getJNIEnv(void) {
    if (!g_vm) {
        ALOGE("JavaVM not initialized");
        return NULL;
    }

    JNIEnv *env = NULL;
    const jint status = (*g_vm)->GetEnv(g_vm, (void **)&env, JNI_VERSION_1_6);

    if (status == JNI_EDETACHED) {
        pthread_mutex_lock(&g_vm_mutex);
        JavaVMAttachArgs args = {.version = JNI_VERSION_1_6, .name = "CPCall-Thread", .group = NULL};
        if ((*g_vm)->AttachCurrentThread(g_vm, &env, &args) != JNI_OK) {
            pthread_mutex_unlock(&g_vm_mutex);
            ALOGE("Failed to attach thread");
            return NULL;
        }
        pthread_mutex_unlock(&g_vm_mutex);
    } else if (status != JNI_OK) {
        ALOGE("Failed to get JNIEnv: %d", status);
        return NULL;
    }

    clear_pending(env, "getJNIEnv");
    return env;
}

/* ---------- JNI 缓存 ---------- */
static bool ensure_jni_cache(JNIEnv *env) {
    if (g_cache.initialized) return true;

    pthread_mutex_lock(&g_cache.mtx);
    if (g_cache.initialized) {
        pthread_mutex_unlock(&g_cache.mtx);
        return true;
    }

    const jclass localBundle = (*env)->FindClass(env, "android/os/Bundle");
    if (!localBundle) { clear_pending(env, "FindClass Bundle"); pthread_mutex_unlock(&g_cache.mtx); return false; }
    g_cache.bundleClass = (*env)->NewGlobalRef(env, localBundle);
    (*env)->DeleteLocalRef(env, localBundle);

    g_cache.bundleInit = (*env)->GetMethodID(env, g_cache.bundleClass, "<init>", "()V");
    g_cache.bundlePutString = (*env)->GetMethodID(env, g_cache.bundleClass, "putString", "(Ljava/lang/String;Ljava/lang/String;)V");
    g_cache.bundlePutInt = (*env)->GetMethodID(env, g_cache.bundleClass, "putInt", "(Ljava/lang/String;I)V");
    g_cache.bundlePutLong = (*env)->GetMethodID(env, g_cache.bundleClass, "putLong", "(Ljava/lang/String;J)V");
    g_cache.bundleGetBoolean = (*env)->GetMethodID(env, g_cache.bundleClass, "getBoolean", "(Ljava/lang/String;)Z");
    g_cache.bundleGetInt = (*env)->GetMethodID(env, g_cache.bundleClass, "getInt", "(Ljava/lang/String;)I");
    g_cache.bundleGetLong = (*env)->GetMethodID(env, g_cache.bundleClass, "getLong", "(Ljava/lang/String;)J");
    g_cache.bundleGetString = (*env)->GetMethodID(env, g_cache.bundleClass, "getString", "(Ljava/lang/String;)Ljava/lang/String;");
    g_cache.bundleGetParcelable = (*env)->GetMethodID(env, g_cache.bundleClass, "getParcelable", "(Ljava/lang/String;)Landroid/os/Parcelable;");

    const jclass localUriClass = (*env)->FindClass(env, "android/net/Uri");
    if (!localUriClass) { clear_pending(env, "FindClass Uri"); pthread_mutex_unlock(&g_cache.mtx); return false; }
    g_cache.uriClass = (*env)->NewGlobalRef(env, localUriClass);
    (*env)->DeleteLocalRef(env, localUriClass);

    jmethodID uriParse = (*env)->GetStaticMethodID(env, g_cache.uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    char uriStr[256];
    snprintf(uriStr, sizeof(uriStr), "content://%s", AUTHORITY);
    const jstring jUriStr = (*env)->NewStringUTF(env, uriStr);
    jobject localUri = (*env)->CallStaticObjectMethod(env, g_cache.uriClass, uriParse, jUriStr);
    clear_pending(env, "Uri.parse");
    (*env)->DeleteLocalRef(env, jUriStr);
    if (localUri) {
        g_cache.uri = (*env)->NewGlobalRef(env, localUri);
        (*env)->DeleteLocalRef(env, localUri);
    }

    const jclass localResolverClass = (*env)->FindClass(env, "android/content/ContentResolver");
    if (!localResolverClass) { clear_pending(env, "FindClass ContentResolver"); pthread_mutex_unlock(&g_cache.mtx); return false; }
    g_cache.resolverClass = (jclass)(*env)->NewGlobalRef(env, localResolverClass);
    (*env)->DeleteLocalRef(env, localResolverClass);

    g_cache.resolverCall = (*env)->GetMethodID(env, g_cache.resolverClass, "call",
        "(Landroid/net/Uri;Ljava/lang/String;Ljava/lang/String;Landroid/os/Bundle;)Landroid/os/Bundle;");

    jclass localPfdClass = (*env)->FindClass(env, "android/os/ParcelFileDescriptor");
    if (localPfdClass) {
        g_cache.pfdClass = (jclass)(*env)->NewGlobalRef(env, localPfdClass);
        (*env)->DeleteLocalRef(env, localPfdClass);
        g_cache.pfdGetFd = (*env)->GetMethodID(env, g_cache.pfdClass, "getFd", "()I");
        g_cache.pfdClose = (*env)->GetMethodID(env, g_cache.pfdClass, "close", "()V");
    } else {
        clear_pending(env, "FindClass ParcelFileDescriptor");
    }

    if (!g_cache.bundleInit || !g_cache.bundlePutString || !g_cache.bundlePutInt ||
        !g_cache.bundlePutLong || !g_cache.bundleGetBoolean || !g_cache.bundleGetInt ||
        !g_cache.bundleGetLong || !g_cache.bundleGetString || !g_cache.bundleGetParcelable ||
        !g_cache.uri || !g_cache.resolverCall) {
        clear_pending(env, "ensure_jni_cache verification");
        ALOGE("cpcall: Failed to resolve all required JNI metadata");
        pthread_mutex_unlock(&g_cache.mtx);
        return false;
    }

    clear_pending(env, "ensure_jni_cache");
    g_cache.initialized = true;
    pthread_mutex_unlock(&g_cache.mtx);
    ALOGD("cpcall: JNI cache initialized successfully");
    return true;
}

/* ---------- ContentResolver ---------- */
static jobject getContentResolver(JNIEnv *env) {
    if (g_cache.resolver) return g_cache.resolver;

    pthread_mutex_lock(&g_cache.mtx);
    if (g_cache.resolver) {
        pthread_mutex_unlock(&g_cache.mtx);
        return g_cache.resolver;
    }

    const jclass activityThread = (*env)->FindClass(env, "android/app/ActivityThread");
    if (!activityThread) { clear_pending(env, "FindClass ActivityThread"); pthread_mutex_unlock(&g_cache.mtx); return NULL; }

    jmethodID currentActivityThread = (*env)->GetStaticMethodID(env, activityThread, "currentActivityThread", "()Landroid/app/ActivityThread;");
    if (!currentActivityThread) {
        clear_pending(env, "GetStaticMethodID currentActivityThread");
        (*env)->DeleteLocalRef(env, activityThread);
        pthread_mutex_unlock(&g_cache.mtx);
        return NULL;
    }

    jobject at = (*env)->CallStaticObjectMethod(env, activityThread, currentActivityThread);
    clear_pending(env, "CallStaticObjectMethod currentActivityThread");
    (*env)->DeleteLocalRef(env, activityThread);
    if (!at) { pthread_mutex_unlock(&g_cache.mtx); return NULL; }

    const jclass atClass = (*env)->GetObjectClass(env, at);
    jmethodID getApplication = (*env)->GetMethodID(env, atClass, "getApplication", "()Landroid/app/Application;");
    if (!getApplication) {
        clear_pending(env, "GetMethodID getApplication");
        (*env)->DeleteLocalRef(env, atClass);
        (*env)->DeleteLocalRef(env, at);
        pthread_mutex_unlock(&g_cache.mtx);
        return NULL;
    }

    jobject context = (*env)->CallObjectMethod(env, at, getApplication);
    clear_pending(env, "CallObjectMethod getApplication");
    (*env)->DeleteLocalRef(env, atClass);
    (*env)->DeleteLocalRef(env, at);
    if (!context) { pthread_mutex_unlock(&g_cache.mtx); return NULL; }

    jclass contextClass = (*env)->GetObjectClass(env, context);
    jmethodID getResolverMethod = (*env)->GetMethodID(env, contextClass, "getContentResolver", "()Landroid/content/ContentResolver;");
    if (!getResolverMethod) {
        clear_pending(env, "GetMethodID getContentResolver");
        (*env)->DeleteLocalRef(env, contextClass);
        (*env)->DeleteLocalRef(env, context);
        pthread_mutex_unlock(&g_cache.mtx);
        return NULL;
    }

    jobject localResolver = (*env)->CallObjectMethod(env, context, getResolverMethod);
    clear_pending(env, "CallObjectMethod getContentResolver");
    (*env)->DeleteLocalRef(env, contextClass);
    (*env)->DeleteLocalRef(env, context);

    if (localResolver) {
        g_cache.resolver = (*env)->NewGlobalRef(env, localResolver);
        (*env)->DeleteLocalRef(env, localResolver);
    }
    pthread_mutex_unlock(&g_cache.mtx);
    return g_cache.resolver;
}

/* ---------- 初始化 ---------- */
void cp_call_init(JavaVM *vm) {
    pthread_mutex_lock(&g_vm_mutex);
    g_vm = vm;
    JNIEnv *env = NULL;
    if (g_vm && (*g_vm)->GetEnv(g_vm, (void **)&env, JNI_VERSION_1_6) == JNI_OK) {
        ensure_jni_cache(env);
        getContentResolver(env);
    }
    pthread_mutex_unlock(&g_vm_mutex);
    ALOGD("CPCall initialized");
}

/* ---------- Bundle 工具 ---------- */
static jobject createBundle(JNIEnv *env) {
    if (!ensure_jni_cache(env) || !g_cache.bundleClass || !g_cache.bundleInit) return NULL;
    return (*env)->NewObject(env, g_cache.bundleClass, g_cache.bundleInit);
}

static void putString(JNIEnv *env, jobject bundle, const char *key, const char *value) {
    if (!env || !bundle || !key || !value || !ensure_jni_cache(env)) return;
    const jstring jKey = (*env)->NewStringUTF(env, key);
    const jstring jValue = (*env)->NewStringUTF(env, value);
    (*env)->CallVoidMethod(env, bundle, g_cache.bundlePutString, jKey, jValue);
    clear_pending(env, "Bundle.putString");
    (*env)->DeleteLocalRef(env, jKey);
    (*env)->DeleteLocalRef(env, jValue);
}

static void putInt(JNIEnv *env, jobject bundle, const char *key, jint value) {
    if (!env || !bundle || !key || !ensure_jni_cache(env)) return;
    const jstring jKey = (*env)->NewStringUTF(env, key);
    (*env)->CallVoidMethod(env, bundle, g_cache.bundlePutInt, jKey, value);
    clear_pending(env, "Bundle.putInt");
    (*env)->DeleteLocalRef(env, jKey);
}

static void putLong(JNIEnv *env, jobject bundle, const char *key, jlong value) {
    if (!env || !bundle || !key || !ensure_jni_cache(env)) return;
    jstring jKey = (*env)->NewStringUTF(env, key);
    (*env)->CallVoidMethod(env, bundle, g_cache.bundlePutLong, jKey, value);
    clear_pending(env, "Bundle.putLong");
    (*env)->DeleteLocalRef(env, jKey);
}

static bool getBoolean(JNIEnv *env, jobject bundle, const char *key) {
    if (!env || !bundle || !key || !ensure_jni_cache(env)) return false;
    jstring jKey = (*env)->NewStringUTF(env, key);
    jboolean result = (*env)->CallBooleanMethod(env, bundle, g_cache.bundleGetBoolean, jKey);
    clear_pending(env, "Bundle.getBoolean");
    (*env)->DeleteLocalRef(env, jKey);
    return result;
}

static int getInt(JNIEnv *env, jobject bundle, const char *key) {
    if (!env || !bundle || !key || !ensure_jni_cache(env)) return 0;
    const jstring jKey = (*env)->NewStringUTF(env, key);
    const jint result = (*env)->CallIntMethod(env, bundle, g_cache.bundleGetInt, jKey);
    clear_pending(env, "Bundle.getInt");
    (*env)->DeleteLocalRef(env, jKey);
    return result;
}

static int64_t getLong(JNIEnv *env, jobject bundle, const char *key) {
    if (!env || !bundle || !key || !ensure_jni_cache(env)) return 0;
    const jstring jKey = (*env)->NewStringUTF(env, key);
    const jlong result = (*env)->CallLongMethod(env, bundle, g_cache.bundleGetLong, jKey);
    clear_pending(env, "Bundle.getLong");
    (*env)->DeleteLocalRef(env, jKey);
    return result;
}

static char *getStringAlloc(JNIEnv *env, jobject bundle, const char *key) {
    if (!env || !bundle || !key || !ensure_jni_cache(env)) return NULL;
    const jstring jKey = (*env)->NewStringUTF(env, key);
    const jstring jResult = (*env)->CallObjectMethod(env, bundle, g_cache.bundleGetString, jKey);
    clear_pending(env, "Bundle.getString");
    (*env)->DeleteLocalRef(env, jKey);

    if (!jResult) return NULL;

    const char *cstr = (*env)->GetStringUTFChars(env, jResult, NULL);
    if (!cstr) {
        (*env)->DeleteLocalRef(env, jResult);
        return NULL;
    }

    const size_t len = strlen(cstr);
    char *result = malloc(len + 1);
    if (result) memcpy(result, cstr, len + 1);

    (*env)->ReleaseStringUTFChars(env, jResult, cstr);
    (*env)->DeleteLocalRef(env, jResult);
    return result;
}

/* ---------- 通用 call ---------- */
static jobject callJavaMethod(const char *method, jobject extras) {
    JNIEnv *env = getJNIEnv();
    if (!env) { ALOGE("Cannot get JNIEnv for method: %s", method); return NULL; }
    if (!ensure_jni_cache(env)) { ALOGE("Cannot initialize JNI cache for method: %s", method); return NULL; }

    jobject resolver = getContentResolver(env);
    if (!resolver) { ALOGE("Cannot get ContentResolver for method: %s", method); return NULL; }
    if (!g_cache.uri || !g_cache.resolverCall) { ALOGE("JNI metadata missing for method: %s", method); return NULL; }

    const jstring jMethod = (*env)->NewStringUTF(env, method);
    jobject result = (*env)->CallObjectMethod(env, resolver, g_cache.resolverCall, g_cache.uri, jMethod, NULL, extras);
    if ((*env)->ExceptionCheck(env)) {
        ALOGE("cpcall: contentResolver.call(%s) Throw an exception, cleaned up and returned failure", method);
        (*env)->ExceptionClear(env);
        result = NULL;
    }
    (*env)->DeleteLocalRef(env, jMethod);
    return result;
}

static jobject callJavaMethodNoExtras(const char *method) {
    return callJavaMethod(method, NULL);
}

/* ---------- 目录查询 ---------- */
static char *call_get_dir(const char *method) {
    JNIEnv *env = getJNIEnv();
    if (!env) { ALOGE("Cannot get JNIEnv for %s", method); return NULL; }

    (*env)->PushLocalFrame(env, 16);
    jobject result = callJavaMethodNoExtras(method);
    if (!result) {
        ALOGE("Failed to call %s", method);
        (*env)->PopLocalFrame(env, NULL);
        return NULL;
    }

    const bool success = getBoolean(env, result, "success");
    if (!success) {
        ALOGE("%s failed", method);
        (*env)->PopLocalFrame(env, NULL);
        return NULL;
    }

    char *path = getStringAlloc(env, result, "dir_path");
    (*env)->PopLocalFrame(env, NULL);
    if (path) ALOGD("%s returned: %s", method, path);
    else ALOGE("Failed to get path from %s result", method);
    return path;
}

char *cp_call_get_internal_dir(void)        { return call_get_dir("getInternalDir"); }
char *cp_call_get_external_dir(void)        { return call_get_dir("getExternalDir"); }
char *cp_call_get_cache_dir(void)           { return call_get_dir("getCacheDir"); }
char *cp_call_get_external_cache_dir(void)  { return call_get_dir("getExternalCacheDir"); }

/* ---------- 文件操作 ---------- */
int cp_call_open(const char *path, int flags, mode_t mode) {
    JNIEnv *env = getJNIEnv();
    if (!env) { errno = EIO; return -1; }

    (*env)->PushLocalFrame(env, 16);
    jobject extras = createBundle(env);
    if (!extras) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return -1; }

    putString(env, extras, "path", path);
    putInt(env, extras, "flags", flags);

    const char *modeStr = "r";
    if ((flags & O_ACCMODE) == O_RDWR) modeStr = "rw";
    else if (flags & O_WRONLY) {
        if (flags & O_APPEND) modeStr = "wa";
        else if (flags & O_TRUNC) modeStr = "wt";
        else modeStr = "w";
    }
    putString(env, extras, "mode", modeStr);

    jobject result = callJavaMethod("open", extras);
    if (!result) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return -1; }

    const bool success = getBoolean(env, result, "success");
    int fd = -1;

    if (success) {
        const jstring fdKey = (*env)->NewStringUTF(env, "fd");
        jobject pfdObj = (*env)->CallObjectMethod(env, result, g_cache.bundleGetParcelable, fdKey);
        clear_pending(env, "Bundle.getParcelable");
        (*env)->DeleteLocalRef(env, fdKey);

        if (pfdObj) {
            const jint javaFd = (*env)->CallIntMethod(env, pfdObj, g_cache.pfdGetFd);
            clear_pending(env, "ParcelFileDescriptor.getFd");

            if (javaFd >= 0) {
                fd = dup(javaFd);
                if (fd < 0) ALOGE("dup failed: %s", strerror(errno));
            } else {
                ALOGE("getFd returned invalid fd: %d", javaFd);
                errno = EBADF;
            }

            (*env)->CallVoidMethod(env, pfdObj, g_cache.pfdClose);
            clear_pending(env, "ParcelFileDescriptor.close");
            (*env)->DeleteLocalRef(env, pfdObj);
        } else {
            ALOGE("Failed to get ParcelFileDescriptor from bundle");
            errno = EIO;
        }
    } else {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
    }

    (*env)->PopLocalFrame(env, NULL);
    return fd;
}

int cp_call_unlink(const char *path) {
    JNIEnv *env = getJNIEnv();
    if (!env) { errno = EIO; return -1; }

    (*env)->PushLocalFrame(env, 16);
    jobject extras = createBundle(env);
    if (!extras) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return -1; }
    putString(env, extras, "path", path);

    jobject result = callJavaMethod("unlink", extras);
    if (!result) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return -1; }

    const bool success = getBoolean(env, result, "success");
    const int ret = success ? 0 : -1;
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
    }
    (*env)->PopLocalFrame(env, NULL);
    return ret;
}

int cp_call_rmdir(const char *path) { return cp_call_unlink(path); }

int cp_call_mkdir(const char *path, mode_t mode) {
    JNIEnv *env = getJNIEnv();
    if (!env) { errno = EIO; return -1; }

    (*env)->PushLocalFrame(env, 16);
    jobject extras = createBundle(env);
    if (!extras) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return -1; }
    putString(env, extras, "path", path);
    putInt(env, extras, "mode", mode);

    jobject result = callJavaMethod("mkdir", extras);
    if (!result) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return -1; }

    const bool success = getBoolean(env, result, "success");
    int ret = success ? 0 : -1;
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
    }
    (*env)->PopLocalFrame(env, NULL);
    return ret;
}

int cp_call_rename(const char *oldpath, const char *newpath) {
    JNIEnv *env = getJNIEnv();
    if (!env) { errno = EIO; return -1; }

    (*env)->PushLocalFrame(env, 16);
    jobject extras = createBundle(env);
    if (!extras) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return -1; }
    putString(env, extras, "path", oldpath);
    putString(env, extras, "path2", newpath);

    jobject result = callJavaMethod("rename", extras);
    if (!result) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return -1; }

    const bool success = getBoolean(env, result, "success");
    const int ret = success ? 0 : -1;
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
    }
    (*env)->PopLocalFrame(env, NULL);
    return ret;
}

int cp_call_stat(const char *path, struct stat *buf) {
    JNIEnv *env = getJNIEnv();
    if (!env) { errno = EIO; return -1; }

    (*env)->PushLocalFrame(env, 16);
    jobject extras = createBundle(env);
    if (!extras) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return -1; }
    putString(env, extras, "path", path);

    jobject result = callJavaMethod("stat", extras);
    if (!result) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return -1; }

    const bool success = getBoolean(env, result, "success");
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
        (*env)->PopLocalFrame(env, NULL);
        return -1;
    }

    memset(buf, 0, sizeof(struct stat));
    buf->st_size = getLong(env, result, "file_size");
    buf->st_mtime = (int)getLong(env, result, "last_modified");
    const bool is_dir = getBoolean(env, result, "is_dir");
    buf->st_mode = is_dir ? (S_IFDIR | 0755) : (S_IFREG | 0644);

    (*env)->PopLocalFrame(env, NULL);
    return 0;
}

int cp_call_access(const char *path, int mode) {
    JNIEnv *env = getJNIEnv();
    if (!env) { errno = EIO; return -1; }

    (*env)->PushLocalFrame(env, 16);
    jobject extras = createBundle(env);
    if (!extras) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return -1; }
    putString(env, extras, "path", path);
    putInt(env, extras, "mode", mode);

    jobject result = callJavaMethod("access", extras);
    if (!result) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return -1; }

    const bool success = getBoolean(env, result, "success");
    const int ret = success ? 0 : -1;
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
    }
    (*env)->PopLocalFrame(env, NULL);
    return ret;
}

DIR *cp_call_opendir(const char *name) {
    JNIEnv *env = getJNIEnv();
    if (!env) { errno = EIO; return NULL; }

    (*env)->PushLocalFrame(env, 16);
    jobject extras = createBundle(env);
    if (!extras) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return NULL; }
    putString(env, extras, "path", name);

    jobject result = callJavaMethod("opendir", extras);
    if (!result) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return NULL; }

    const bool success = getBoolean(env, result, "success");
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
        (*env)->PopLocalFrame(env, NULL);
        return NULL;
    }

    int fd = cp_call_open(name, O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) { (*env)->PopLocalFrame(env, NULL); return NULL; }

    DIR *dir = fdopendir(fd);
    if (!dir) close(fd);

    (*env)->PopLocalFrame(env, NULL);
    return dir;
}

char *cp_call_realpath(const char *path, char *resolved_path) {
    JNIEnv *env = getJNIEnv();
    if (!env) { errno = EIO; return NULL; }

    (*env)->PushLocalFrame(env, 16);
    jobject extras = createBundle(env);
    if (!extras) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return NULL; }
    putString(env, extras, "path", path);

    jobject result = callJavaMethod("realpath", extras);
    if (!result) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return NULL; }

    const bool success = getBoolean(env, result, "success");
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
        (*env)->PopLocalFrame(env, NULL);
        return NULL;
    }

    char *realpath_str = getStringAlloc(env, result, "result");
    if (!realpath_str) { (*env)->PopLocalFrame(env, NULL); return NULL; }

    if (!resolved_path) {
        resolved_path = malloc(strlen(realpath_str) + 1);
    }
    if (resolved_path) strcpy(resolved_path, realpath_str);

    free(realpath_str);
    (*env)->PopLocalFrame(env, NULL);
    return resolved_path;
}

int cp_call_truncate(const char *path, off_t length) {
    JNIEnv *env = getJNIEnv();
    if (!env) { errno = EIO; return -1; }

    (*env)->PushLocalFrame(env, 16);
    jobject extras = createBundle(env);
    if (!extras) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return -1; }
    putString(env, extras, "path", path);
    putLong(env, extras, "size", length);

    jobject result = callJavaMethod("truncate", extras);
    if (!result) { (*env)->PopLocalFrame(env, NULL); errno = EIO; return -1; }

    const bool success = getBoolean(env, result, "success");
    const int ret = success ? 0 : -1;
    if (!success) {
        int err = getInt(env, result, "errno");
        if (err < 0) err = -err;
        errno = err;
    }
    (*env)->PopLocalFrame(env, NULL);
    return ret;
}