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

#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "internal/tefplugin/tef_core_imp.h"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir(path, mode) _mkdir(path)
#define PATH_MAX MAX_PATH
#define SEPARATOR '\\'
#define SEPARATOR_STR "\\"
#else
#define SEPARATOR '/'
#define SEPARATOR_STR "/"
#endif


#include "tef_api.h"
#include "internal/log.h"

#include "internal/modloader/modloader_core_imp.h"
#include "memdl/memdl.h"

API_EXPORT int init_tefkernel() {
    // Initialize logging system first
    tefkernel_log_init("tefkernel.log");

    return 0;
}
