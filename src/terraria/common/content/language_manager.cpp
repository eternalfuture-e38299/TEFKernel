/*******************************************************************************
 * tefkernel - language_manager
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
 * Created: 2026/11/7
 *******************************************************************************/

#include "terraria/content/language_manager.hpp"

#include "tef/core/log.hpp"
#include "tef/patchlib/struct/string.hpp"

namespace terraria::content {
    using namespace tef;

    language_manager &language_manager::instance() {
        static language_manager inst;
        return inst;
    }

    void language_manager::early_init() {
        localized_text_type_ = patchlib::type("Terraria.Localization", "LocalizedText");
        if (localized_text_type_)
            localized_text_ctor_ = localized_text_type_.get_method(".ctor", 2);

        if (const auto language_type = patchlib::type("Terraria.Localization", "Language"))
            language_get_text_ = language_type.get_method("GetText", 1);

        const auto language_mgr_type = patchlib::type("Terraria.Localization", "LanguageManager");
        if (!language_mgr_type) {
            log::error("language_manager: LanguageManager not found");
            return;
        }

#if defined(__ANDROID__)
        load_text_json_ = language_mgr_type.get_method("LoadLanguageFromFileText", 1);
#else
        load_text_json_ = language_mgr_type.get_method("LoadLanguageFromFileTextJson", 2);
#endif
        process_copy_commands_ = language_mgr_type.get_method("ProcessCopyCommandsInTexts", 0);

        if (const auto load_language = language_mgr_type.get_method("LoadLanguage", 0))
            h_load_language_ = patchlib_install_prepost_hook(
                load_language.raw(), nullptr, load_language_postfix);

        if (const auto game_culture = patchlib::type("Terraria.Localization", "GameCulture"))
            legacy_id_ = game_culture.get_field("LegacyId");
    }

    void language_manager::shutdown() {
        callbacks_.clear();
        // hook 不卸载
    }

    // -----------------------------------------------------------------------
    // 回调执行
    // -----------------------------------------------------------------------

    void language_manager::run_callbacks(patch_handle_t instance, int legacy_id) const {
        // 遍历快照：回调内注册/注销不影响本轮
        for (const auto snapshot = callbacks_; const auto cb: snapshot) {
            if (!cb) continue;
            const char *text = cb(static_cast<language_culture_t>(legacy_id));
            if (!text) continue;

            const patchlib::structs::string content(text);

#if defined(__ANDROID__)
            load_text_json_[instance](content);
#else
            load_text_json_[instance](content, true);
#endif
        }

#if defined(__ANDROID__)
        process_copy_commands_[instance]();
#else
        process_copy_commands_.invoke<void>(instance);
#endif
    }

    void language_manager::load_language_postfix(patch_handle_t this_, void **args,
                                                 void *,
                                                 const patch_method_signature_t *) {
        const auto &self = instance();
        if (!this_ || !args || !args[0]) return;

        // args[0] 是 GameCulture 实例（值内存）
        patch_handle_t culture_raw = *static_cast<patch_handle_t *>(args[0]);
        const patchlib::handle culture(culture_raw, false);
        if (!culture) return;

        const int legacy_id = self.legacy_id_ ? self.legacy_id_.get_value<int>(culture) : 0;
        self.run_callbacks(this_, legacy_id);
    }

    // -----------------------------------------------------------------------
    // 便捷 API
    // -----------------------------------------------------------------------

    patchlib::handle language_manager::create_localized_text(const char *key,
                                                             const char *value) const {
        if (!localized_text_type_ || !localized_text_ctor_) return {};
        if (!key) key = "";
        if (!value) value = "";

        const patchlib::structs::string skey(key);
        const patchlib::structs::string svalue(value);

#if defined(__ANDROID__)
        auto localized_text = localized_text_type_.new_instance();
        if (!localized_text) return {};
        localized_text_ctor_[localized_text](skey, svalue);
#else
        auto localized_text = localized_text_ctor_.invoke_ctor(skey, svalue);
#endif
        if (!localized_text) {
            log::error("language_manager: create_localized_text('{}') failed", key);
        }
        return localized_text;
    }

    patchlib::handle language_manager::get_localized_text(const char *key) const {
        log::debug("get_localized_text: enter key={}",
                   key ? key : "(null)");

        if (!language_get_text_) {
            log::warn("get_localized_text: language_get_text_ method handle is null");
            return {};
        }
        if (!key) key = "";

        const patchlib::structs::string skey(key);
        log::debug("get_localized_text: skey created, raw={}",
                   fmt::ptr(skey.raw()));
        if (!skey) {
            log::error("get_localized_text: failed to create il2cpp string for key={}", key);
            return {};
        }

        log::debug("get_localized_text: invoking GetText(key={})", key);
        auto localized_text = language_get_text_.invoke<patchlib::handle>(skey.raw());
        log::debug("get_localized_text: GetText returned raw={}",
                   fmt::ptr(localized_text.raw()));

        if (!localized_text) {
            log::warn("get_localized_text: GetText returned null for key={}", key);
        }

        return localized_text;
    }

    void language_manager::register_callback(const terraria_languager_manager_get_localized_text_t cb) {
        if (cb) callbacks_.push_back(cb);
    }

    bool language_manager::unregister_callback(const terraria_languager_manager_get_localized_text_t cb) {
        for (auto it = callbacks_.begin(); it != callbacks_.end(); ++it) {
            if (*it == cb) {
                callbacks_.erase(it);
                return true;
            }
        }
        return false;
    }

    void language_manager::ensure_initialized() {
        const auto language_mgr_type = patchlib::type("Terraria.Localization", "LanguageManager");
        if (!language_mgr_type) return;

        if (const auto instance_field = language_mgr_type.get_field("Instance")) {
            auto instance = instance_field.get_value<patchlib::handle>();
            // 仅为了让静态构造函数先跑起来；句柄是借用，不接管
        }
    }
}
