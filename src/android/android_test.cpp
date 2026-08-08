/*******************************************************************************
 * tefkernel - android_test
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
 * Created: 2025/12/28
 *******************************************************************************/

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <sstream>

#include "internal/kernel_state.h"
#include "internal/log.h"
#include "patchlib/method.h"
#include "patchlib/type.h"

void start_test() {
    patch_handle_t fmod = patchlib_type_get_type("System", "FMOD");
    if (!fmod) {
        TEKLOG_ERROR("Failed to get FMOD::System type");
        return;
    }

    tefstd_vector_t vec;
    patchlib_type_get_methods(fmod, false, &vec);

    const size_t count = tefstd_vector_size(&vec);
    TEKLOG_INFO("Searching %zu methods for CreateSound...", count);

    for (size_t i = 0; i < count; ++i) {
        auto e = static_cast<patch_handle_t*>(tefstd_vector_at(&vec, i));
        if (!e) continue;

        const char* name = patchlib_method_get_name(*e);
        TEKLOG_INFO("Found: %s", name);
    }

    
}