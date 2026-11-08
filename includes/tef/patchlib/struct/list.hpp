/*******************************************************************************
 * tefkernel - list
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

#include "array.hpp"
#include "patchlib/struct/list.h"

#include <stdexcept>
#include <type_traits>
#include <vector>

namespace tef::patchlib::structs {
    template<typename T>
    struct list : handle {
        list() = default;

        list(const handle &h) : handle(h) {
        }

        explicit list(const handle &h, const bool own) : handle(h.raw(), own) {
        }

        explicit list(const size_t capacity, handle type = {}) : handle() {
            if (type.raw() == nullptr) {
                type = type::get_basic_type(get_patch_type());
            }
            _value = patchlib_list_create(capacity, type.raw());
            _own = true;
            if (_value == nullptr) {
                throw std::runtime_error("Failed to create list");
            }
        }

        explicit list(const std::vector<T> &vec, handle type = {})
            : list(vec.size(), type) {
            for (const auto &elem: vec) add(elem);
        }

        explicit list(const array<T> &arr, handle type = {})
            : list(arr.length(), type) {
            if (!patchlib_list_copy_from(_value, arr.raw())) {
                throw std::runtime_error("Failed to copy from array");
            }
        }

        ~list() = default;

        list(const list &other) : handle(static_cast<const handle &>(other)) {
        }

        list &operator=(const list &other) {
            if (this != &other) handle::operator=(static_cast<const handle &>(other));
            return *this;
        }

        list(list &&other) noexcept : handle(std::move(static_cast<handle &>(other))) {
        }

        list &operator=(list &&other) noexcept {
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
        void add(const T &value) {
            if constexpr (std::is_same_v<T, handle>) {
                patch_handle_t raw = value.raw();
                if (!patchlib_list_add(_value, &raw)) {
                    throw std::runtime_error("Failed to add element to list");
                }
            } else {
                if (!patchlib_list_add(_value,
                                       const_cast<void *>(static_cast<const void *>(&value)))) {
                    throw std::runtime_error("Failed to add element to list");
                }
            }
        }

        bool remove(const T &value) {
            if constexpr (std::is_same_v<T, handle>) {
                patch_handle_t raw = value.raw();
                return patchlib_list_remove(_value, &raw);
            } else {
                return patchlib_list_remove(
                    _value, const_cast<void *>(static_cast<const void *>(&value)));
            }
        }

        void remove_at(const size_t index) const {
            if (!patchlib_list_remove_at(_value, index)) {
                throw std::out_of_range("list index out of bounds");
            }
        }

        void clear() const {
            if (!patchlib_list_clear(_value)) {
                throw std::runtime_error("Failed to clear list");
            }
        }

        // ==================== 容量 ====================
        [[nodiscard]] size_t length() const { return get_array().length(); }
        [[nodiscard]] bool is_empty() const { return length() == 0; }
        [[nodiscard]] size_t size() const { return length(); }

        // ==================== 转换 ====================
        array<T> get_array() const {
            return array<T>(handle(patchlib_list_get_array(_value), true));
        }

        std::vector<T> to_std_vector() const { return get_array().to_std_vector(); }

        // ==================== 迭代器 ====================
        class iterator {
        public:
            iterator(list *l, const size_t i) : _list(l), _index(i) {
            }

            iterator &operator++() {
                ++_index;
                return *this;
            }

            iterator operator++(int) {
                iterator t = *this;
                ++(*this);
                return t;
            }

            iterator &operator--() {
                --_index;
                return *this;
            }

            iterator operator--(int) {
                iterator t = *this;
                --(*this);
                return t;
            }

            T operator*() const { return _list->get_array()[_index]; }
            bool operator==(const iterator &o) const { return _list == o._list && _index == o._index; }
            bool operator!=(const iterator &o) const { return !(*this == o); }

        private:
            list *_list;
            size_t _index;
        };

        class const_iterator {
        public:
            const_iterator(const list *l, const size_t i) : _list(l), _index(i) {
            }

            const_iterator &operator++() {
                ++_index;
                return *this;
            }

            const_iterator operator++(int) {
                const_iterator t = *this;
                ++(*this);
                return t;
            }

            const_iterator &operator--() {
                --_index;
                return *this;
            }

            const_iterator operator--(int) {
                const_iterator t = *this;
                --(*this);
                return t;
            }

            T operator*() const { return _list->get_array()[_index]; }
            bool operator==(const const_iterator &o) const { return _list == o._list && _index == o._index; }
            bool operator!=(const const_iterator &o) const { return !(*this == o); }

        private:
            const list *_list;
            size_t _index;
        };

        iterator begin() { return iterator(this, 0); }
        iterator end() { return iterator(this, length()); }
        const_iterator begin() const { return const_iterator(this, 0); }
        const_iterator end() const { return const_iterator(this, length()); }
        const_iterator cbegin() const { return const_iterator(this, 0); }
        const_iterator cend() const { return const_iterator(this, length()); }

        // ==================== 运算符 ====================
        explicit operator bool() const { return is_valid(); }

        bool operator==(const list &other) const {
            if (length() != other.length()) return false;
            auto a1 = get_array();
            auto a2 = other.get_array();
            for (size_t i = 0; i < length(); ++i) {
                if (a1[i] != a2[i]) return false;
            }
            return true;
        }

        bool operator!=(const list &other) const { return !(*this == other); }

    private:
        static patch_type_t get_patch_type() {
            if constexpr (std::is_same_v<T, void>) return PATCH_VOID;
            else if constexpr (std::is_same_v<T, int8_t>) return PATCH_INT8;
            else if constexpr (std::is_same_v<T, int16_t>) return PATCH_INT16;
            else if constexpr (std::is_same_v<T, int32_t>) return PATCH_INT32;
            else if constexpr (std::is_same_v<T, int64_t>) return PATCH_INT64;
            else if constexpr (std::is_same_v<T, uint8_t>) return PATCH_UINT8;
            else if constexpr (std::is_same_v<T, uint16_t>) return PATCH_UINT16;
            else if constexpr (std::is_same_v<T, uint32_t>) return PATCH_UINT32;
            else if constexpr (std::is_same_v<T, uint64_t>) return PATCH_UINT64;
            else if constexpr (std::is_same_v<T, bool>) return PATCH_BOOL;
            else if constexpr (std::is_same_v<T, float>) return PATCH_FLOAT;
            else if constexpr (std::is_same_v<T, double>) return PATCH_DOUBLE;
            else if constexpr (std::is_same_v<T, char>) return PATCH_CHAR;
            else return PATCH_OBJECT;
        }
    };
}
