/*******************************************************************************
 * File: tef_api
 * Project: tefkernel
 * Created: 2025/11/23
 * Author: eternalfuture-e38299
 * Github: https://github.com/eternalfuture-e38299
 * 
 * MIT License
 * 
 * Copyright (c) 2025 eternalfuture-e38299
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *******************************************************************************/

#ifndef TEFKERNEL_TEF_API_H
#define TEFKERNEL_TEF_API_H

#if defined(_WIN32) || defined(_WIN64)
#ifdef BUILDING_DLL
#define API_EXPORT __declspec(dllexport)
#else
#define API_EXPORT __declspec(dllimport)
#endif
#else
#define API_EXPORT __attribute__((visibility("default")))
#endif

#if defined(_WIN32) || defined(_WIN64)
#define API_CALL __cdecl
#else
#define API_CALL
#endif

#if IS_TEFKERNEL_BUILD

#define DEFINE_FUNCTION(ret, name, ...) \
ret name(__VA_ARGS__);

#else

#define DEFINE_FUNCTION(ret, name, ...) \
API_EXPORT ret (API_CALL *name)(__VA_ARGS__) = NULL;

#endif

#define DEFINE_API_FUNCTION(ret, name, ...) \
API_EXPORT ret (API_CALL *name)(__VA_ARGS__) = NULL;

#endif //TEFKERNEL_TEF_API_H