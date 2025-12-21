/*******************************************************************************
 * tefkernel - test_mod_api
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

#ifndef TEFKERNEL_TEST_MOD_API_H
#define TEFKERNEL_TEST_MOD_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

    // Mod API 版本
#define MOD_API_VERSION 1

    // Mod 信息结构
    typedef struct {
        const char* mod_id;          // Mod唯一标识符
        const char* name;           // Mod显示名称
        const char* author;         // 作者
        const char* version;        // 版本号
        const char* description;    // 描述
        int api_version;           // 要求的API版本
    } mod_info_t;

    // Mod 操作函数表
    typedef struct {
        // 初始化Mod
        int (*initialize)(void);

        // 卸载Mod
        void (*shutdown)(void);

        // 获取Mod信息
        const mod_info_t* (*get_info)(void);

        // 每帧更新（可选）
        void (*update)(float delta_time);
    } mod_ops_t;

    // Mod 主函数类型
    typedef const mod_ops_t* (*mod_main_func_t)(void);

    // Mod 必须导出的函数名
#define MOD_MAIN_FUNC_NAME "mod_main"

#ifdef __cplusplus
}
#endif

#endif //TEFKERNEL_TEST_MOD_API_H