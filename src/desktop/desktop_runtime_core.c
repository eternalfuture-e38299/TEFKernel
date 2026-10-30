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

#include "internal/crash_handler.h"
#include "internal/kernel_state.h"
#include "internal/log.h"

#include "internal/terraria/asset.h"
#include "internal/terraria/content/item_manager.h"
#include "internal/terraria/content/projectile_manager.h"
#include "internal/terraria/content/npc_manager.h"
#include "internal/terraria/content/buff_manager.h"
#include "internal/terraria/content/prefix_manager.h"
#include "internal/terraria/gui.h"
#include "internal/terraria/save/tef_save_game_handles.h"
#include "internal/terraria/save/tef_player_save.h"
#include "internal/terraria/save/tef_world_save.h"
#include "internal/terraria/language_manager.h"
#include "internal/terraria/main.h"
#include "internal/terraria/netmanager.h"
#include "internal/terraria/recipe_manager.h"
#include "internal/terraria/texture2d.h"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define MKDIR(path) _mkdir(path)
#else
#define MKDIR(path) mkdir(path, 0755)
#endif

void Test(void);

// ==================== 目录操作辅助函数 ====================

// 检查目录是否存在
static int dir_exists(const char *path) {
  struct stat st;
  if (stat(path, &st) == 0) {
    return S_ISDIR(st.st_mode);
  }
  return 0;
}

char *tefkernel_working_dir = NULL;
static char tefkernel_log_path[512] = {0};

API_EXPORT int init_tefkernel(const char *workdir, bool is_server) {
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
  /* workdir 可能自带或不带结尾斜杠，统一补一个再拼 */
  char log_dir[512];
  const size_t wlen = strlen(workdir);
  if (wlen > 0 && workdir[wlen - 1] == '/')
    snprintf(log_dir, sizeof(log_dir), "%slogs/tefkernel", workdir);
  else if (wlen > 0 && workdir[wlen - 1] == '\\')
    snprintf(log_dir, sizeof(log_dir), "%slogs/tefkernel", workdir);
  else
    snprintf(log_dir, sizeof(log_dir), "%s/logs/tefkernel", workdir);

  // ============ 检查并创建日志目录 ============
  char log_config_filename[512] = {0};

  if (dir_exists(log_dir)) {
    // 目录存在，使用该目录
    snprintf(tefkernel_log_path, sizeof(tefkernel_log_path), "%s/runtime",
             log_dir);
    snprintf(log_config_filename, sizeof(log_config_filename), "%s/runtime",
             log_dir);

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
#if defined(_WIN32)
  // 必须在任何代码访问 LocalizedText.Empty 之前完成 LanguageManager 的静态初始化。
  terraria_language_manager_ensure_initialized();
#endif
  tefkernel_crash_handler_init();
  terraria_main_init(is_server);
  terraria_texture2d_init(is_server);
  terraria_asset_init();
  terraria_item_manager_init();
  terraria_projectile_manager_init();
  terraria_npc_manager_init();
  terraria_buff_manager_init();
  terraria_prefix_manager_init();
  terraria_gui_init();
  tef_save_game_handles_init();
  tef_player_save_init();
  tef_world_save_init();
  terraria_recipe_manager_init();
  terraria_language_manager_init();

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

  Test();

  return 0;
}
