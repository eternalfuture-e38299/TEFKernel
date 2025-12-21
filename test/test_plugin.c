/*******************************************************************************
 * tefkernel - test_plugin
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
 * Created: 2025/12/13
 *******************************************************************************/

#include <stdio.h>

#include "../include/tefplugin/tpf_core.h"

void print_hello() {
    printf("Hello From Plugin\n");
}


bool initialize(plugin_handle_t* this_handle) {
    TPF_SYMBOL(print_hello);
    return true;
}

void cleanup(plugin_handle_t* this_handle) {

}

TPF_PLUGIN("eternal.future.testplugin", "test", "eternalfuture-e38299", "1.0.0", initialize, cleanup);