/*******************************************************************************
 * tefkernel - asset
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

#include "terraria/asset.h"                         // include_abi，不许改
#include "terraria/content/asset_factory.hpp"
#include "tef/patchlib/handle.hpp"

namespace {
    using terraria::content::asset_factory;

    template<typename F, typename Default>
    constexpr auto guard(F &&f, Default def) noexcept -> decltype(f()) {
        try { return f(); } catch (...) { return def; }
    }
} // namespace

extern "C" {
patch_handle_t terraria_asset_create(patch_handle_t type, patch_handle_t value) {
    return guard([&] {
        const tef::patchlib::handle t_view(type, false);
        const tef::patchlib::handle v_view(value, false);
        auto out = asset_factory::instance().create(t_view, v_view);
        return out.release(); // ABI 返回裸 handle，调用方接管
    }, static_cast<patch_handle_t>(nullptr));
}

patch_handle_t terraria_asset_get_generic_class(patch_handle_t element_type) {
    return guard([&] {
        const tef::patchlib::handle t_view(element_type, false);
        auto out = asset_factory::instance().get_generic_class(t_view);
        return out.release();
    }, static_cast<patch_handle_t>(nullptr));
}

patch_handle_t terraria_asset_get_value(patch_handle_t asset) {
    return guard([&] {
        const tef::patchlib::handle a_view(asset, false);
        auto out = asset_factory::get_value(a_view);
        return out.release();
    }, static_cast<patch_handle_t>(nullptr));
}
} // extern "C"
