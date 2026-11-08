/*******************************************************************************
 * tefkernel - dictionary
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

#pragma once

#include <cstdint>

#include "../type.hpp"
#include "patchlib/struct/dictionary.h"

#include <stdexcept>
#include <type_traits>
#include <unordered_map>

namespace tef::patchlib::structs {
    template<typename Key, typename Value>
    struct dictionary : handle {
        dictionary() = default;

        dictionary(const handle &h) : handle(h) {
        }

        explicit dictionary(const handle &h, const bool own) : handle(h.raw(), own) {
        }

        explicit dictionary(const size_t capacity, handle key_type = {}, handle value_type = {})
            : handle() {
            if (key_type.raw() == nullptr) key_type = type::get_basic_type(get_key_patch_type());
            if (value_type.raw() == nullptr) value_type = type::get_basic_type(get_value_patch_type());

            _value = patchlib_dictionary_create(key_type.raw(), value_type.raw(), capacity);
            _own = true;
            if (_value == nullptr) {
                throw std::runtime_error("Failed to create dictionary");
            }
        }

        explicit dictionary(const std::unordered_map<Key, Value> &map, size_t capacity = 0,
                            handle key_type = {}, handle value_type = {})
            : dictionary(capacity > 0 ? capacity : map.size(), key_type, value_type) {
            for (const auto &[k, v]: map) add(k, v);
        }

        ~dictionary() = default;

        dictionary(const dictionary &other) : handle(static_cast<const handle &>(other)) {
        }

        dictionary &operator=(const dictionary &other) {
            if (this != &other) handle::operator=(static_cast<const handle &>(other));
            return *this;
        }

        dictionary(dictionary &&other) noexcept : handle(std::move(static_cast<handle &>(other))) {
        }

        dictionary &operator=(dictionary &&other) noexcept {
            if (this != &other) handle::operator=(std::move(static_cast<handle &>(other)));
            return *this;
        }

        [[nodiscard]] bool is_valid() const { return patchlib_is_valid(_value); }

        void reset(const handle &new_handle = {}, const bool take_ownership = false) {
            handle::reset(new_handle.raw(), take_ownership);
        }

        handle release() {
            handle h(_value, _own);
            _value = nullptr;
            _own = false;
            return h;
        }

        // ==================== 元素操作 ====================
        void add(const Key &key, const Value &value) {
            if constexpr (std::is_same_v<Key, handle> || std::is_same_v<Value, handle>) {
                patch_handle_t k_raw, v_raw;
                const void *k_ptr = &key;
                const void *v_ptr = &value;
                if constexpr (std::is_same_v<Key, handle>) {
                    k_raw = key.raw();
                    k_ptr = &k_raw;
                }
                if constexpr (std::is_same_v<Value, handle>) {
                    v_raw = value.raw();
                    v_ptr = &v_raw;
                }
                if (!patchlib_dictionary_add(_value,
                                             const_cast<void *>(k_ptr),
                                             const_cast<void *>(v_ptr))) {
                    throw std::runtime_error("Failed to add key-value pair to dictionary");
                }
            } else {
                if (!patchlib_dictionary_add(
                    _value,
                    const_cast<void *>(static_cast<const void *>(&key)),
                    const_cast<void *>(static_cast<const void *>(&value)))) {
                    throw std::runtime_error("Failed to add key-value pair to dictionary");
                }
            }
        }

        Value get(const Key &key) const {
            if constexpr (std::is_same_v<Key, handle> || std::is_same_v<Value, handle>) {
                patch_handle_t k_raw;
                const void *k_ptr = &key;
                if constexpr (std::is_same_v<Key, handle>) {
                    k_raw = key.raw();
                    k_ptr = &k_raw;
                }

                if constexpr (std::is_same_v<Value, handle>) {
                    patch_handle_t v_raw = nullptr;
                    if (!patchlib_dictionary_get_value(_value, const_cast<void *>(k_ptr), &v_raw)) {
                        throw std::out_of_range("Key not found in dictionary");
                    }
                    return handle(v_raw, true);
                } else {
                    Value value;
                    if (!patchlib_dictionary_get_value(_value, const_cast<void *>(k_ptr), &value)) {
                        throw std::out_of_range("Key not found in dictionary");
                    }
                    return value;
                }
            } else {
                Value value;
                if (!patchlib_dictionary_get_value(
                    _value, const_cast<void *>(static_cast<const void *>(&key)), &value)) {
                    throw std::out_of_range("Key not found in dictionary");
                }
                return value;
            }
        }

        bool try_get(const Key &key, Value &out_value) const {
            if constexpr (std::is_same_v<Key, handle> || std::is_same_v<Value, handle>) {
                patch_handle_t k_raw;
                const void *k_ptr = &key;
                if constexpr (std::is_same_v<Key, handle>) {
                    k_raw = key.raw();
                    k_ptr = &k_raw;
                }

                if constexpr (std::is_same_v<Value, handle>) {
                    patch_handle_t v_raw = nullptr;
                    if (!patchlib_dictionary_get_value(_value, const_cast<void *>(k_ptr), &v_raw)) {
                        return false;
                    }
                    out_value = handle(v_raw, true);
                    return true;
                } else {
                    return patchlib_dictionary_get_value(_value, const_cast<void *>(k_ptr), &out_value);
                }
            } else {
                return patchlib_dictionary_get_value(
                    _value, const_cast<void *>(static_cast<const void *>(&key)), &out_value);
            }
        }

        void set(const Key &key, const Value &value) {
            patch_handle_t k_raw;
            patch_handle_t v_raw;
            const void *k_ptr = &key;
            const void *v_ptr = &value;

            if constexpr (std::is_same_v<Key, handle>) {
                k_raw = key.raw();
                k_ptr = &k_raw;
            }
            if constexpr (std::is_same_v<Value, handle>) {
                v_raw = value.raw();
                v_ptr = &v_raw;
            }

            if (!patchlib_dictionary_set_value(_value,
                                               const_cast<void *>(k_ptr),
                                               const_cast<void *>(v_ptr))) {
                throw std::runtime_error("Failed to set value in dictionary");
            }
        }

        bool remove(const Key &key) {
            patch_handle_t k_raw;
            const void *k_ptr = &key;
            if constexpr (std::is_same_v<Key, handle>) {
                k_raw = key.raw();
                k_ptr = &k_raw;
            }
            return patchlib_dictionary_remove(_value, const_cast<void *>(k_ptr));
        }

        void clear() const {
            if (!patchlib_dictionary_clear(_value)) {
                throw std::runtime_error("Failed to clear dictionary");
            }
        }

        Value operator[](const Key &key) const { return get(key); }

        template<typename V = Value>
        std::enable_if_t<!std::is_same_v<V, handle>, V &> operator[](const Key &key) {
            static V temp;
            try { temp = get(key); } catch (const std::out_of_range &) {
                V default_val{};
                add(key, default_val);
                temp = default_val;
            }
            return temp;
        }

        // ==================== 容量 ====================
        [[nodiscard]] size_t length() const { return patchlib_dictionary_length(_value); }
        [[nodiscard]] bool empty() const { return length() == 0; }
        [[nodiscard]] size_t size() const { return length(); }

        // ==================== 运算符 ====================
        explicit operator bool() const { return is_valid(); }

        bool operator==(const dictionary &other) const {
            if (length() != other.length()) return false;
            return _value == other._value;
        }

        bool operator!=(const dictionary &other) const { return !(*this == other); }

    private:
        static patch_type_t get_key_patch_type() {
            if constexpr (std::is_same_v<Key, void>) return PATCH_VOID;
            else if constexpr (std::is_same_v<Key, int8_t>) return PATCH_INT8;
            else if constexpr (std::is_same_v<Key, int16_t>) return PATCH_INT16;
            else if constexpr (std::is_same_v<Key, int32_t>) return PATCH_INT32;
            else if constexpr (std::is_same_v<Key, int64_t>) return PATCH_INT64;
            else if constexpr (std::is_same_v<Key, uint8_t>) return PATCH_UINT8;
            else if constexpr (std::is_same_v<Key, uint16_t>) return PATCH_UINT16;
            else if constexpr (std::is_same_v<Key, uint32_t>) return PATCH_UINT32;
            else if constexpr (std::is_same_v<Key, uint64_t>) return PATCH_UINT64;
            else if constexpr (std::is_same_v<Key, bool>) return PATCH_BOOL;
            else if constexpr (std::is_same_v<Key, float>) return PATCH_FLOAT;
            else if constexpr (std::is_same_v<Key, double>) return PATCH_DOUBLE;
            else if constexpr (std::is_same_v<Key, char>) return PATCH_CHAR;
            else return PATCH_OBJECT;
        }

        static patch_type_t get_value_patch_type() {
            if constexpr (std::is_same_v<Value, void>) return PATCH_VOID;
            else if constexpr (std::is_same_v<Value, int8_t>) return PATCH_INT8;
            else if constexpr (std::is_same_v<Value, int16_t>) return PATCH_INT16;
            else if constexpr (std::is_same_v<Value, int32_t>) return PATCH_INT32;
            else if constexpr (std::is_same_v<Value, int64_t>) return PATCH_INT64;
            else if constexpr (std::is_same_v<Value, uint8_t>) return PATCH_UINT8;
            else if constexpr (std::is_same_v<Value, uint16_t>) return PATCH_UINT16;
            else if constexpr (std::is_same_v<Value, uint32_t>) return PATCH_UINT32;
            else if constexpr (std::is_same_v<Value, uint64_t>) return PATCH_UINT64;
            else if constexpr (std::is_same_v<Value, bool>) return PATCH_BOOL;
            else if constexpr (std::is_same_v<Value, float>) return PATCH_FLOAT;
            else if constexpr (std::is_same_v<Value, double>) return PATCH_DOUBLE;
            else if constexpr (std::is_same_v<Value, char>) return PATCH_CHAR;
            else return PATCH_OBJECT;
        }
    };
}
