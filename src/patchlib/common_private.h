/*******************************************************************************
 * tefkernel - common_private
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

#ifndef TEFKERNEL_PRIVATE_H
#define TEFKERNEL_PRIVATE_H
#include "patchlib/type.h"

#ifdef __cplusplus
extern "C" {

#endif

patch_handle_t patchlib_type_find_method(
    patch_handle_t type,
    const char *name,
    int args_count,
    const patch_handle_t *args_types, // nullable
    const char **args_names // nullable
);

#ifdef __cplusplus
}
#endif

#endif //TEFKERNEL_PRIVATE_H
