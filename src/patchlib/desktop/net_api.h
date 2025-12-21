/*******************************************************************************
 * tefkernel - net_api
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
 * Created: 2025/11/23
 *******************************************************************************/

#ifndef TEFKERNEL_NET_API_H
#define TEFKERNEL_NET_API_H

#include <stddef.h>

#include "patchlib/type.h"

#ifdef _WIN32
#define SYM_EXPORT __declspec(dllexport)
#else
#define SYM_EXPORT __attribute__((visibility("default")))
#endif

#ifdef _WIN32
#define NETAPI_CALL __stdcall  // 必须与C#的StdCall一致
#else
#define NETAPI_CALL
#endif

#define DEFINE_NETAPI_FUNCTION(ret, name, ...) \
    SYM_EXPORT ret (NETAPI_CALL *name)(__VA_ARGS__) = NULL;

#ifdef __cplusplus
extern "C" {
#endif



#ifdef __cplusplus
}
#endif

#endif //TEFKERNEL_NET_API_H
