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

#include "internal/runtime.h"

#include "tef_api.h"
#include "internal/log.h"
#include "internal/kernel_state.h"
#include "internal/crash_handler.h"

#include <string.h>

#include "internal/terraria/asset.h"
#include "internal/terraria/main.h"
#include "internal/terraria/netmanager.h"
#include "internal/terraria/texture2d.h"
#include "terraria/texture2d.h"

void Test(void);

char* tefkernel_working_dir = NULL;
API_EXPORT int init_tefkernel(const char* workdir, bool is_server) {
    if (is_server) TEKLOG_INFO("Running Server Client");
    tefkernel_working_dir = strdup(workdir);

    // Initialize logging system first

    tefkernel_log_init(NULL);
    tefkernel_crash_handler_init();

    terraria_main_init(is_server);
    terraria_netmanager_init();
    terraria_texture2d_init(is_server);
    terraria_asset_init();

    tefkernel_init();
    tefkernel_load();
    tefkernel_start();

    // Test();

    return 0;
}
