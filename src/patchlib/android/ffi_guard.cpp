/*******************************************************************************
 * tefkernel - ffi_guard
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
 * Created: 2026/01/02
 *******************************************************************************/

#include <ffi.h>

/* 用 C++ 的 catch-all 包住 ffi_call。
 *
 * 托管方法抛出的异常会以 C++ 异常（Il2CppExceptionWrapper）的形式展开。
 * 如果调用方（Lua 的 invoke 链）没有 handler，就会一路展开到顶层
 * std::terminate → SIGABRT。这里就地捕获，保证调用失败而不是崩游戏。
 *
 * 返回 0 表示正常返回；1 表示托管异常已被捕获。 */
extern "C" int tef_ffi_call_guarded(ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue) {
    try {
        ffi_call(cif, FFI_FN(fn), rvalue, avalue);
        return 0;
    } catch (...) {
        return 1;
    }
}