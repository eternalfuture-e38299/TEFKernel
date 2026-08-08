/*******************************************************************************
 * tefkernel - thread
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
 * Created: 2026/7/27
 *******************************************************************************/

#include "patchlib/thread.h"
#include "../il2cpp_api.h"

patch_handle_t patchlib_thread_current() {
    return il2cpp_thread_current();
}

patch_handle_t patchlib_thread_attach() {
    return il2cpp_thread_attach(il2cpp_domain_get());
}

void patchlib_thread_detach(patch_handle_t thread) {
    il2cpp_thread_detach(thread);
}