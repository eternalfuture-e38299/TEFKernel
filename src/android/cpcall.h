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
 
#ifndef TEFKERNEL_CPCALL_H
#define TEFKERNEL_CPCALL_H

#include <jni.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef __cplusplus
extern "C" {
#endif

// 初始化
void cp_call_init(JavaVM* vm);

// 支持的函数
char* cp_call_get_internal_dir(void);
char* cp_call_get_external_dir(void);
char* cp_call_get_cache_dir(void);
char* cp_call_get_external_cache_dir();
int cp_call_open(const char* path, int flags, mode_t mode);
int cp_call_unlink(const char* path);
int cp_call_rmdir(const char* path);
int cp_call_mkdir(const char* path, mode_t mode);
int cp_call_rename(const char* oldpath, const char* newpath);
int cp_call_stat(const char* path, struct stat* buf);
int cp_call_access(const char* path, int mode);
DIR* cp_call_opendir(const char* name);
char* cp_call_realpath(const char* path, char* resolved_path);
int cp_call_truncate(const char* path, off_t length);

// 不支持的函数返回错误
#define CP_UNSUPPORTED -1

#ifdef __cplusplus
}
#endif


#endif //TEFKERNEL_CPCALL_H
