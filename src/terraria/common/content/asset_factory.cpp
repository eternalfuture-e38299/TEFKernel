/*******************************************************************************
 * tefkernel - asset_factory
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

#include "terraria/content/asset_factory.hpp"

#include "tef/core/log.hpp"
#include "tef/patchlib/field.hpp"
#include "tef/patchlib/struct/string.hpp"

namespace terraria::content {
    using namespace tef;

    asset_factory &asset_factory::instance() {
        static asset_factory inst;
        return inst;
    }

    void asset_factory::early_init() {
        if (resolved_) return;
        resolved_ = true;
        asset_class_ = patchlib::type("ReLogic.Content", "Asset`1");
        if (!asset_class_)
            log::warn("asset_factory: ReLogic.Content.Asset`1 not found");
    }

    void asset_factory::shutdown() {
        asset_class_.reset();
        resolved_ = false;
    }

    patchlib::type asset_factory::get_generic_class(const patchlib::handle &element_type) const {
        if (!asset_class_) {
            log::error("asset_factory: Asset`1 class not initialized");
            return {};
        }
        if (!element_type) {
            log::error("asset_factory: invalid element type");
            return {};
        }

        std::vector<patchlib::handle> args;

#if defined(__ANDROID__)
        // Android：mono 侧用 OBJECT 作为泛型参数（与原 C 实现一致）
        auto obj_type = patchlib::type::get_basic_type(PATCH_OBJECT);
        if (!obj_type) {
            log::error("asset_factory: PATCH_OBJECT basic type unavailable");
            return {};
        }
        auto mono = obj_type.get_mono_type();
        if (!mono) {
            log::error("asset_factory: mono type for OBJECT failed");
            return {};
        }
        args.push_back(mono);
#else
        args.push_back(element_type);
#endif

        auto generic_class = asset_class_.make_generic_type(args);
        if (!generic_class) {
            log::error("asset_factory: Asset`1 generic instantiation failed");
            return {};
        }
        log::debug("asset_factory: Asset`1 instantiated");
        return generic_class;
    }

    patchlib::handle asset_factory::create(const patchlib::handle &type,
                                           const patchlib::handle &value) const {
        const auto generic_class = get_generic_class(type);
        if (!generic_class) return {};

        const auto ctor = generic_class.get_method(".ctor", 1);
        auto value_field = generic_class.get_field("Value");
        if (!value_field)
            value_field = generic_class.get_field("<Value>k__BackingField");
        const auto set_state = generic_class.get_method("set_State", 1);
        if (!ctor || !value_field || !set_state) {
            log::error("asset_factory: Asset`1 members missing");
            return {};
        }

        // 构造 Asset`1（桌面临时空字符串；Android 无参构造 + 手动写字段）
        const auto null_str = patchlib::structs::string("");
        patchlib::handle asset;

#if !defined(__ANDROID__)
        asset = ctor.invoke_ctor(null_str);
#else
        asset = generic_class.new_instance();
#endif
        if (!asset) {
            log::error("asset_factory: Asset`1 construct failed");
            return {};
        }

        // 写 <Value>k__BackingField
        if (value)
            value_field.set_value(value, asset);

        // State = Loaded(2)
        set_state[asset](2);

        return asset;
    }

    patchlib::handle asset_factory::get_value(const patchlib::handle &asset) {
        if (!asset) return {};

        const auto generic_class = asset.get_type();
        if (!generic_class) return {};

        auto value_field = generic_class.get_field("Value");
        if (!value_field)
            value_field = generic_class.get_field("<Value>k__BackingField");
        if (!value_field) return {};

        return value_field.get_value<patchlib::handle>(asset);
    }
}
