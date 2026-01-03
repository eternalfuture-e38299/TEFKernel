/*******************************************************************************
 * tefkernel - desktop_test
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


#include "patchlib/method.h"
#include "internal/log.h"

void Hello(patch_handle_t instance, void **args, void *result,
                                   const patch_method_signature_t *sig_info) {
    TEKLOG_INFO("Fuck!!!");
    TEKLOG_INFO("arg a=%d", *(int*)args[0]);
    *(int*)result = 114514;
}

void Test() {
    patch_handle_t hello_method = patchlib_type_get_method_by_param_count(patchlib_type_get_type("tefloader", "Test"), "Hello", 1);


    int r;
    patchlib_install_prepost_hook(hello_method, NULL, Hello);
    patchlib_method_invoke(hello_method, PATCH_NULL, &r, 114514);


    TEKLOG_INFO("r=%d", r);
}