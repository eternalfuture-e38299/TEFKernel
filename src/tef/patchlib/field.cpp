/*******************************************************************************
 * tefkernel - field
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

#include "tef/patchlib/field.hpp"

namespace tef::patchlib {
    std::string field::get_name() const {
        const char *n = patchlib_field_get_name(_value);
        return n ? std::string(n) : std::string();
    }

    bool field::is_static() const { return patchlib_field_is_static(_value); }
    bool field::is_instance() const { return patchlib_field_is_instance(_value); }
    bool field::is_const() const { return patchlib_field_is_const(_value); }
    bool field::is_thread_static() const { return patchlib_field_is_thread_static(_value); }

    size_t field::get_size() const {
        return patchlib_field_get_size(_value);
    }

    patch_type_t field::get_type() const { return patchlib_field_get_type(_value); }

#if defined(__ANDROID__)
    void *field::get_pointer(void *inst) const { return patchlib_field_get_pointer(_value, inst); }
#endif
}
