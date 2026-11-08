/*******************************************************************************
 * tefkernel - method
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

#include "tef/patchlib/method.hpp"

#include <cstring>

namespace tef::patchlib {
    method_signature::method_signature() : patch_method_signature_t{} {
        method = nullptr;
        is_instance = false;
        return_type = PATCH_VOID;
        arg_types = {};
        arg_names = {};
        token = 0;
    }

    method_signature::~method_signature() { patchlib_method_signature_free(this); }

    method_signature::method_signature(method_signature &&other) noexcept
        : patch_method_signature_t(other) {
        std::memset(static_cast<patch_method_signature_t *>(&other), 0, sizeof(patch_method_signature_t));
    }

    method_signature &method_signature::operator=(method_signature &&other) noexcept {
        if (this != &other) {
            patchlib_method_signature_free(this);
            static_cast<patch_method_signature_t &>(*this) = other;
            std::memset(static_cast<patch_method_signature_t *>(&other), 0, sizeof(patch_method_signature_t));
        }
        return *this;
    }

    std::vector<patch_type_t> method_signature::get_arg_types() const {
        std::vector<patch_type_t> out;
        auto *d = static_cast<const patch_type_t *>(arg_types.data);
        for (size_t i = 0; i < tefstd_vector_size(&arg_types); ++i) out.push_back(d[i]);
        return out;
    }

    std::vector<std::string> method_signature::get_arg_names() const {
        std::vector<std::string> out;
        auto *d = static_cast<const char * const*>(arg_names.data);
        for (size_t i = 0; i < tefstd_vector_size(&arg_names); ++i)
            if (d[i]) out.emplace_back(d[i]);
        return out;
    }

    std::string method::get_name() const {
        const char *n = patchlib_method_get_name(_value);
        return n ? std::string(n) : std::string();
    }

    int method::get_param_count() const { return patchlib_method_get_param_count(_value); }
    bool method::is_instance() const { return patchlib_method_is_instance(_value); }
    bool method::is_static() const { return patchlib_method_is_static(_value); }
    int method::get_token() const { return patchlib_method_get_token(_value); }
    patch_type_t method::get_return_type() const { return patchlib_method_get_return_type(_value); }

    method method::make_generic_instance(const std::vector<handle> &template_types) const {
        tefstd_vector_t raw{};
        tefstd_vector_init(&raw, sizeof(patch_handle_t));
        tefstd_vector_reserve(&raw, template_types.size());
        for (auto &h: template_types) {
            patch_handle_t raw_h = h.raw();
            tefstd_vector_push_back(&raw, &raw_h);
        }

        handle result(patchlib_method_make_generic_instance(_value, &raw), true);
        tefstd_vector_destroy(&raw); // 调用完后释放 raw 内部缓冲区
        return {result};
    }

#if defined(__ANDROID__)
    void *method::get_pointer() const { return patchlib_method_get_pointer(_value); }
#endif

    std::unique_ptr<method_signature> method::get_signature() const {
        if (auto sig = std::make_unique<method_signature>(); patchlib_method_get_signature(
            _value, const_cast<patch_method_signature_t *>(sig->raw()))) {
            return sig;
        }
        return nullptr;
    }

    bool method::uninstall_hook(const hook_id &id) { return patchlib_uninstall_hook(id.get()); }
}
