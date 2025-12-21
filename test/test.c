/*******************************************************************************
 * tefkernel - test
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

#include <dlfcn.h>
#include <stdio.h>

#include "internal/tefplugin/tef_core_imp.h"
#include "internal/log.h"
#include "memdl/memdl.h"
#include "tefplugin/tpf_core.h"


#include <stdlib.h>

#include "internal/modloader/modloader_core_imp.h"

/*
// 原函数
void original_function(const char* message) {
    printf("Original: %s\n", message);
}

// Hook 管理器
typedef struct {
    void* original;
    void* before_hook;
    void* after_hook;
} hook_manager_t;

void hook_t(ffi_cif* cif, void* ret, void** args, void* user_data) {
    hook_manager_t* mgr = user_data;

    // 前置钩子
    if (mgr->before_hook) {
        ((void(*)(void**))mgr->before_hook)(args);
    }

    // 调用原函数
    ffi_call(cif, mgr->original, ret, args);

    // 后置钩子
    if (mgr->after_hook) {
        ((void(*)(void**))mgr->after_hook)(args);
    }
}

void* create_hook(ffi_type* arg_types[], ffi_type* ret_type, void* target_func, void* before, void* after) {
    void* code_ptr;
    ffi_closure* closure = ffi_closure_alloc(sizeof(ffi_closure), &code_ptr);

    hook_manager_t* manager = malloc(sizeof(hook_manager_t));
    manager->original = target_func;
    manager->before_hook = before;
    manager->after_hook = after;

    ffi_cif cif;

    ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, ret_type, arg_types);

    ffi_prep_closure_loc(closure, &cif,
        hook_t,
        manager, code_ptr
    );

    return code_ptr;
}

// 使用示例
void before_hook(void** args) {
    printf("Before hook: message = %s\n", *(const char**)args[0]);
}

void after_hook(void** args) {
    printf("After hook completed\n");
}

void before_hook2(void** args) {
    printf("前置hook2 = %s\n", *(const char**)args[0]);
}*/

// 测试代码示例
int main() {
    tefkernel_log_init(NULL);

    plugin_handle_t* p;
    tpf_load_plugin(dlopen("libtest_plugin.so", MEMDL_LAZY), &p);

    ml_handle_t* ml;
    tefkernel_load_ml(dlopen("libtest_modloader.so", MEMDL_LAZY), NULL, NULL, &ml);
    ml->ml_entry->ops->initialize(ml->ml_entry);
    ml->ml_entry->ops->load_mods(ml->ml_entry);
    ml->ml_entry->ops->initialize_mods(ml->ml_entry);
    ml->ml_entry->ops->unload_mods(ml->ml_entry);
    ml->ml_entry->ops->shutdown(ml->ml_entry);


    // ml->ml_entry->ops.;


    tefkernel_log_cleanup();
    return 0;
}

/*
int main() {

    tefkernel_log_init("Hello.log");
    TEKLOG_INFO("Hello, World! %s", "aa");

    /*
    // 创建钩子函数
    void* hooked_func = create_hook((ffi_type*[]){ &ffi_type_pointer }, &ffi_type_void, original_function, before_hook, after_hook);
    void* hooked_func2 = create_hook((ffi_type*[]){ &ffi_type_pointer }, &ffi_type_void, original_function, before_hook2, after_hook);

    // 调用钩子函数
    ((void(*)(const char*))hooked_func)("Hello World");
    ((void(*)(const char*))hooked_func2)("Hello World");

    log_trace("Hello, %d", 1145);
    #1#

    return 0;
}*/