/*******************************************************************************
 * tefkernel - log_bridge
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
 * Created: 2026/11/6
 *******************************************************************************/

#include "../../../includes/tef/core/log.hpp"
#include "../../../includes/log.h"

extern "C" {
API_EXPORT void API_CALL
tefkernel_log_write_net(tefkernel_log_level_t level,
                        const char *file, const int line, const char *func,
                        const char *msg) {
    if (!msg) return;

#if !defined(NDEBUG)
    tef::log::write(static_cast<tef::log::level>(level), file, line, func, msg);
#else
    if (level >= tefkernel_log_level_info)
        tef::log::write(static_cast<tef::log::level>(level), file, line, func, msg);
#endif
}
}
