#ifndef TEFKERNEL_KEYBIND_MANAGER_H
#define TEFKERNEL_KEYBIND_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

#include "../../tef_api.h"

#if !defined(__ANDROID__)
#ifdef __cplusplus
extern "C" {
#endif

/* 键位注册的作用域：内核用 (modloader_id, mod_id) 区分不同 Mod。
 * 字符串会在注册时复制，调用方无需保持存活。 */
typedef struct terraria_keybind_scope_t {
    const char *modloader_id;
    const char *mod_id;
} terraria_keybind_scope_t;

/* 按键回调（仅在游戏主线程、键位状态变化时调用）。
 * scope/id 指向内核内部的稳定字符串副本，回调期间有效。 */
typedef void (*terraria_keybind_callback_t)(const terraria_keybind_scope_t *scope,
                                            const char *id, void *user);

/* 键位描述。
 *
 * name 同时作为：
 *   - 游戏「设置 -> 控制」菜单里显示的名称；
 *   - PlayerInput.KnownTriggers / KeyStatus 的 trigger 键。
 * 因此全局必须唯一（建议带 Mod 名前缀）。为空时退回 id。
 *
 * default_key 使用 XNA/MonoGame 的 Keys 名称，例如 "F6" / "K" / "LeftShift"；
 * 为空表示默认不绑定（菜单显示“未绑定”，玩家可自行设置）。
 *
 * on_press / on_release 可为 NULL。enabled 为 false 时仍会出现在菜单与状态查询中，
 * 但不会触发回调。 */
typedef struct terraria_keybind_desc_t {
    terraria_keybind_scope_t scope;
    const char *id;
    const char *name;
    const char *default_key;
    terraria_keybind_callback_t on_press;
    terraria_keybind_callback_t on_release;
    void *user;
    bool enabled;
} terraria_keybind_desc_t;

DEFINE_FUNCTION(bool, terraria_keybind_manager_available, void)
DEFINE_FUNCTION(const char *, terraria_keybind_manager_error, void)
DEFINE_FUNCTION(void, terraria_keybind_manager_install, void)
DEFINE_FUNCTION(void, terraria_keybind_manager_shutdown, void)
DEFINE_FUNCTION(bool, terraria_keybind_manager_register, const terraria_keybind_desc_t *desc)
DEFINE_FUNCTION(bool, terraria_keybind_manager_unregister, const terraria_keybind_scope_t *scope,
                const char *id)
DEFINE_FUNCTION(void, terraria_keybind_manager_unregister_owner,
                const terraria_keybind_scope_t *scope)
DEFINE_FUNCTION(bool, terraria_keybind_manager_down, const terraria_keybind_scope_t *scope,
                const char *id)
DEFINE_FUNCTION(bool, terraria_keybind_manager_pressed, const terraria_keybind_scope_t *scope,
                const char *id)
DEFINE_FUNCTION(bool, terraria_keybind_manager_released, const terraria_keybind_scope_t *scope,
                const char *id)
DEFINE_FUNCTION(bool, terraria_keybind_manager_set_enabled,
                const terraria_keybind_scope_t *scope, const char *id, bool value)

#ifdef __cplusplus
}
#endif
#endif /* !__ANDROID__ */

#endif /* TEFKERNEL_KEYBIND_MANAGER_H */
