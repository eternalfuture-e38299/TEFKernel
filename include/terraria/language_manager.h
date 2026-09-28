/*******************************************************************************
 * File: item_manager
 * Project: tefkernel
 * Created: 2026/9/16
 * Author: eternalfuture-e38299
 * Github: https://github.com/eternalfuture-e38299
 *
 * MIT License
 *
 * Copyright (c) 2026 eternalfuture-e38299
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *******************************************************************************/

#ifndef LANGUAGE_MANAGER_H
#define LANGUAGE_MANAGER_H

#include "../patchlib/type.h"
#include "../tef_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  LANGUAGE_ENGLISH = 1,
  LANGUAGE_GERMAN = 2,
  LANGUAGE_ITALIAN = 3,
  LANGUAGE_FRENCH = 4,
  LANGUAGE_SPANISH = 5,
  LANGUAGE_RUSSIAN = 6,
  LANGUAGE_CHINESE_SIMPLIFIED = 7,
  LANGUAGE_PORTUGUESE = 8,
  LANGUAGE_POLISH = 9,
  LANGUAGE_JAPANESE = 10,
  LANGUAGE_KOREAN = 11,
  LANGUAGE_CHINESE_TRADITIONAL = 12
} language_culture_t;

/**
 * @brief 创建一个本地化文本对象
 * @param key   本地化键名，例如 "Mods.MyMod.Items.TestSword.DisplayName"
 * @param value 本地化文本值，例如 "测试剑"
 * @return 创建的 LocalizedText 句柄，失败返回 PATCH_NULL
 *
 * @note 创建后可通过 terraria_language_manager_get_localized_text 获取
 *       切换语言时此对象的 Value 不会自动变化，需自行管理
 */
DEFINE_FUNCTION(patch_handle_t, terraria_language_manager_create_localized_text,
                const char *key, const char *value)

/**
 * @brief 获取指定键名的本地化文本
 * @param key 本地化键名
 * @return 本地化文本的句柄，未找到时返回 PATCH_NULL
 *
 * @note 如果 key 不存在，返回的 LocalizedText.Value 会是 key 本身
 */
DEFINE_FUNCTION(patch_handle_t, terraria_language_manager_get_localized_text,
                const char *key)

/**
 * @brief 自定义本地化查询函数类型
 * @param culture 目标语言，见 language_culture_t 枚举
 * @return 返回 JSON 格式的本地化数据字符串
 *
 * @note 返回的 JSON 格式必须为嵌套结构，例如：
 * @code
 * {
 *   "ItemName.<modloader id>.<mod id>": {
 *     "TestSword": "测试剑",
 *     "TestBow": "测试弓"
 *   },
 *   "Tooltip.<modloader id>.<mod id>": {
 *     "TestSword": "村里最好的剑",
 *     "TestBow": "射得又快又准"
 *   },
 *   "Prefix.<modloader id>.<mod id>": {
 *     "Legendary": "传说"
 *   }
 * }
 * @endcode
 *
 * 返回的字符串由调用者管理，建议使用静态缓冲区或持久化内存
 * 框架会解析此 JSON 并合并到游戏的本地化系统中
 */
typedef const char *(*terraria_languager_manager_get_localized_text_t)(
    language_culture_t culture);

/**
 * @brief 注册本地化回调
 * @param callback 自定义本地化查询函数
 *
 * @note 注册后，当游戏需要获取本地化文本时会调用此回调
 *       回调返回的 JSON 会被解析并合并到当前语言的本地化表中
 *       支持多次注册，后注册的回调优先级更高
 *
 * @warning 回调中不要调用其他本地化 API，避免死循环
 *          回调需要线程安全，可能在任意线程被调用
 *          返回的 JSON 必须包含完整的嵌套结构，即使某些分类为空
 */
DEFINE_FUNCTION(void, terraria_language_manager_register_callback,
                terraria_languager_manager_get_localized_text_t callback)

#ifdef __cplusplus
}
#endif
#endif // LANGUAGE_MANAGER_H
