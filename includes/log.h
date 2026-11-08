/*******************************************************************************
 * tefkernel - log
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
 * Created: 2025/12/20
 *******************************************************************************/

#ifndef TEFKERNEL_LOG_H
#define TEFKERNEL_LOG_H

#include "tef_api.h"

#ifdef __cplusplus
extern "C" {

#endif

typedef enum tefkernel_log_level_t {
    tefkernel_log_level_trace, // 最详细的日志信息
    tefkernel_log_level_debug, // 调试信息
    tefkernel_log_level_info, // 一般信息
    tefkernel_log_level_warn, // 警告信息
    tefkernel_log_level_error, // 错误信息
    tefkernel_log_level_critical // 严重错误信息
} tefkernel_log_level_t;


API_EXPORT void API_CALL tefkernel_log_write_net(tefkernel_log_level_t level,
                                                 const char *file, int line, const char *func,
                                                 const char *msg);

#ifdef __cplusplus
}
#endif
#endif //TEFKERNEL_LOG_H
