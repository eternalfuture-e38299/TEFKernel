/*******************************************************************************
 * tefkernel - gui
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

#include "terraria/gui.h"
#include "terraria/content/gui_overlay.hpp"
#include "terraria/abi/guard.hpp"

namespace {
    using terraria::abi::guard_void;
    using terraria::content::gui_overlay;
} // namespace

extern "C" {
void terraria_gui_add_draw(const tef_gui_draw_cb cb, void *user) {
    guard_void([&] { gui_overlay::instance().add_draw(cb, user); });
}

void terraria_gui_remove_draw(const tef_gui_draw_cb cb) {
    guard_void([&] { gui_overlay::instance().remove_draw(cb); });
}

void terraria_gui_rect(const float x, const float y, const float w, const float h, const unsigned rgba) {
    guard_void([&] { gui_overlay::instance().rect(x, y, w, h, rgba); });
}

void terraria_gui_text(const char *text, const float x, const float y, const float scale, const unsigned rgba) {
    guard_void([&] { gui_overlay::instance().text(text, x, y, scale, rgba); });
}

void terraria_gui_string_text(patch_handle_t text, const float x, const float y, const float scale,
                              const unsigned rgba) {
    guard_void([&] { gui_overlay::instance().string_text(text, x, y, scale, rgba); });
}

void terraria_gui_image(patch_handle_t texture, const float x, const float y, const float w, const float h,
                        const unsigned rgba) {
    guard_void([&] { gui_overlay::instance().image(texture, x, y, w, h, rgba); });
}

void terraria_gui_draw_string(patch_handle_t font, const char *text, const float x, const float y,
                              const float scale, const unsigned rgba) {
    guard_void([&] { gui_overlay::instance().draw_string(font, text, x, y, scale, rgba); });
}

void terraria_gui_image_raw(patch_handle_t texture, const float x, const float y, const float w, const float h,
                            const unsigned raw_color) {
    guard_void([&] { gui_overlay::instance().image_raw(texture, x, y, w, h, raw_color); });
}

void terraria_gui_begin_external(void *sprite_batch) {
    guard_void([&] { gui_overlay::instance().begin_external(sprite_batch); });
}

void terraria_gui_end_external(void) {
    guard_void([&] { gui_overlay::instance().end_external(); });
}
} // extern "C"
