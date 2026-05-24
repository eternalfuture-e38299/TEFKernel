/*******************************************************************************
 * tefkernel - string
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
 * Created: 2026/1/11
 *******************************************************************************/

#include "patchlib/struct/string.h"
#include "../../il2cpp_api.h"

#include <string.h>

#include "internal/log.h"

char* patchlib_string_cstr(patch_handle_t str) {
    TEKLOG_DEBUG("patchlib_string_cstr called: str=%p", str);
    
    if (!patchlib_is_valid(str)) {
        TEKLOG_ERROR("Invalid string handle");
        return NULL;
    }
    
    return il2cpp_string_cstr(str);
}
