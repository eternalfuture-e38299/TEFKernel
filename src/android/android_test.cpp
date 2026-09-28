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
#include "patchlib/field.h"
#include "patchlib/method.h"
#include "patchlib/type.h"
#include "tefstd/vector.h"
#include "terraria/asset.h"
#include "terraria/recipe_manager.h"
#include "terraria/texture2d.h"

void add_r(void) {
    for (int item_id = 1; item_id < 6196; item_id++) {
        terraria_recipe_set_result(item_id,
                                   9999);
        terraria_recipe_set_material(
            (int[2])
            {9, 1},
            1);
        terraria_recipe_add();
    }
}

void start_test(void) { /*terraria_recipe_manager_register_callback(add_r);*/ }
