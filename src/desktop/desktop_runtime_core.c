/*******************************************************************************
 * tefkernel - desktop_runtime_core
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
 * Created: 2026/1/3
 *******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "internal/runtime.h"

#include "internal/log.h"
#include "internal/kernel_state.h"
#include "internal/crash_handler.h"

#include "internal/terraria/asset.h"
#include "internal/terraria/main.h"
#include "internal/terraria/netmanager.h"
#include "internal/terraria/texture2d.h"
#include "terraria/texture2d.h"


#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define MKDIR(path) _mkdir(path)
#else
#define MKDIR(path) mkdir(path, 0755)
#endif


void Test(void);


// ==================== 目录操作辅助函数 ====================

// 检查目录是否存在
static int dir_exists(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return 0;
}

char* tefkernel_working_dir = NULL;
static char tefkernel_log_path[512] = {0};

API_EXPORT int init_tefkernel(const char* workdir, bool is_server) {
    if (is_server) {
        printf("Running Server Client\n");
        TEKLOG_INFO("Running Server Client");
    }

    // 保存工作目录
    tefkernel_working_dir = strdup(workdir);
    if (!tefkernel_working_dir) {
        fprintf(stderr, "Failed to allocate memory for working directory\n");
        return -1;
    }

    // ============ 构建日志目录路径 ============
    char log_dir[512];
    snprintf(log_dir, sizeof(log_dir), "%slogs/tefkernel", workdir);

    // ============ 检查并创建日志目录 ============
    char log_config_filename[512] = {0};

    if (dir_exists(log_dir)) {
        // 目录存在，使用该目录
        snprintf(tefkernel_log_path, sizeof(tefkernel_log_path),
                "%s/runtime", log_dir);
        snprintf(log_config_filename, sizeof(log_config_filename),
                "%s/runtime", log_dir);

        printf("Log directory found: %s\n", log_dir);
        TEKLOG_INFO("Log directory found: %s", log_dir);
    }

    // ============ 初始化日志系统 ============
    if (log_config_filename[0] != '\0') {
        tefkernel_log_init(log_config_filename);
    } else {
        // 使用默认配置（仅控制台）
        printf("Log system initialized (console only)\n");
        tefkernel_log_init(NULL);
    }

    // ============ 继续初始化其他模块 ============
    tefkernel_crash_handler_init();
    terraria_main_init(is_server);
    terraria_texture2d_init(is_server);
    terraria_asset_init();
    tefkernel_init();
    tefkernel_load();
    tefkernel_start();
    terraria_netmanager_init();

    // 记录初始化完成
    TEKLOG_INFO("TEFKernel initialization completed successfully");
    TEKLOG_INFO("Working directory: %s", workdir);
    if (tefkernel_log_path[0] != '\0') {
        TEKLOG_INFO("Log path: %s", tefkernel_log_path);
    }

    return 0;
}