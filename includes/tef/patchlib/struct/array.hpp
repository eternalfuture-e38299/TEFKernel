/*******************************************************************************
 * tefkernel - array
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
#include "patchlib/struct/array.h"

#include <initializer_list>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace tef::patchlib::structs::detail {
    template<typename T>
    T read_slot(const void *slot) {
        if constexpr (std::is_same_v<T, handle>) {
            return handle(*static_cast<const patch_handle_t *>(slot), true);
        } else {
            return *static_cast<const T *>(slot);
        }
    }

    template<typename T>
    void write_slot(void *slot, const T &value) {
        if constexpr (std::is_same_v<T, handle>) {
            *static_cast<patch_handle_t *>(slot) = value.raw();
        } else {
            *static_cast<T *>(slot) = value;
        }
    }
} // namespace tef::patchlib::structs::detail

namespace tef::patchlib::structs {
    template<typename T>
    struct array : handle {
        array() = default;

        array(const handle &h) : handle(h) {
        }

        explicit array(const handle &h, const bool own) : handle(h.raw(), own) {
        }

        explicit array(const size_t size, handle type = {}) : handle() {
            if (type.raw() == nullptr) {
                type = type::get_basic_type(get_patch_type());
            }
            _value = patchlib_array_create(size, type.raw());
            _own = true;
            if (_value == nullptr) {
                throw std::runtime_error("Failed to create array");
            }
        }

        array(const T *data, size_t size, handle type = {}) : array(size, type) {
            if constexpr (std::is_same_v<T, handle>) {
                std::vector<patch_handle_t> raw(size, nullptr);
                for (size_t i = 0; i < size; ++i) raw[i] = data[i].raw();
                if (!patchlib_array_copy_from_c(_value, raw.data(), size)) {
                    throw std::runtime_error("Failed to copy from C array");
                }
            } else {
                if (!patchlib_array_copy_from_c(_value, data, size)) {
                    throw std::runtime_error("Failed to copy from C array");
                }
            }
        }

        explicit array(const std::vector<T> &vec, handle type = {})
            : array(vec.data(), vec.size(), type) {
        }

        ~array() = default;

        array(const array &other) : handle(static_cast<const handle &>(other)) {
        }

        array &operator=(const array &other) {
            if (this != &other) handle::operator=(static_cast<const handle &>(other));
            return *this;
        }

        array(array &&other) noexcept : handle(std::move(static_cast<handle &>(other))) {
        }

        array &operator=(array &&other) noexcept {
            if (this != &other) handle::operator=(std::move(static_cast<handle &>(other)));
            return *this;
        }

        [[nodiscard]] bool is_valid() const { return patchlib_is_valid(_value); }

        void reset(const handle &new_handle = {}, const bool take_ownership = false) {
            handle::reset(new_handle.raw(), take_ownership);
        }

        // ==================== 元素访问 ====================
        T at(const size_t index) const {
            if constexpr (std::is_same_v<T, handle>) {
                patch_handle_t raw = nullptr;
                if (!patchlib_array_at(_value, index, &raw)) {
                    throw std::out_of_range("array index out of bounds");
                }
                return handle(raw, true);
            } else {
                T value;
                if (!patchlib_array_at(_value, index, &value)) {
                    throw std::out_of_range("array index out of bounds");
                }
                return value;
            }
        }

        T operator[](const size_t index) const { return at(index); }

        void set(const size_t index, const T &value) {
            if constexpr (std::is_same_v<T, handle>) {
                patch_handle_t raw = value.raw();
                if (!patchlib_array_set(_value, index, &raw)) {
                    throw std::out_of_range("array index out of bounds");
                }
            } else {
                if (!patchlib_array_set(_value, index,
                                        const_cast<void *>(static_cast<const void *>(&value)))) {
                    throw std::out_of_range("array index out of bounds");
                                        }
            }
        }

        void set_unchecked(const size_t index, const T &value) {
            if constexpr (std::is_same_v<T, handle>) {
                patch_handle_t raw = value.raw();
                patchlib_array_set(_value, index, &raw);
            } else {
                patchlib_array_set(_value, index,
                                   const_cast<void *>(static_cast<const void *>(&value)));
            }
        }

        // ==================== 容量 ====================
        [[nodiscard]] size_t length() const { return patchlib_array_length(_value); }
        [[nodiscard]] bool is_empty() const { return patchlib_array_empty(_value); }
        [[nodiscard]] size_t size() const { return length(); }

        // ==================== 填充 ====================
        void fill(const T &value) {
            if constexpr (std::is_same_v<T, handle>) {
                patch_handle_t raw = value.raw();
                if (!patchlib_array_fill(_value, &raw)) {
                    throw std::runtime_error("Failed to fill array");
                }
            } else {
                if (!patchlib_array_fill(_value,
                                         const_cast<void *>(static_cast<const void *>(&value)))) {
                    throw std::runtime_error("Failed to fill array");
                }
            }
        }

        // ==================== 复制 ====================
        void copy_to_c(T *dest, const size_t count) const {
            if constexpr (std::is_same_v<T, handle>) {
                std::vector<patch_handle_t> raw(count, nullptr);
                if (!patchlib_array_copy_to_c(raw.data(), _value, count)) {
                    throw std::runtime_error("Failed to copy to C array");
                }
                for (size_t i = 0; i < count; ++i)
                    dest[i] = handle(raw[i], true);
            } else {
                if (!patchlib_array_copy_to_c(dest, _value, count)) {
                    throw std::runtime_error("Failed to copy to C array");
                }
            }
        }

        void copy_from_c(const T *src, const size_t count) {
            if constexpr (std::is_same_v<T, handle>) {
                std::vector<patch_handle_t> raw(count, nullptr);
                for (size_t i = 0; i < count; ++i) raw[i] = src[i].raw();
                if (!patchlib_array_copy_from_c(_value, raw.data(), count)) {
                    throw std::runtime_error("Failed to copy from C array");
                }
            } else {
                if (!patchlib_array_copy_from_c(_value, src, count)) {
                    throw std::runtime_error("Failed to copy from C array");
                }
            }
        }

        void copy_from(const array &src, const size_t count) {
            if (!patchlib_array_copy(_value, src.raw(), count)) {
                throw std::runtime_error("Failed to copy from array");
            }
        }

        array resized(const size_t new_size, const T &default_value) const {
            patch_handle_t new_handle;
            if constexpr (std::is_same_v<T, handle>) {
                patch_handle_t raw = default_value.raw();
                new_handle = patchlib_array_resize(_value, new_size, &raw);
            } else {
                new_handle = patchlib_array_resize(
                    _value, new_size,
                    const_cast<void *>(static_cast<const void *>(&default_value)));
            }
            if (new_handle == nullptr) {
                throw std::runtime_error("Failed to resize array");
            }
            return array(handle(new_handle, true));
        }

        std::vector<T> to_std_vector() const {
            std::vector<T> result(length());
            copy_to_c(result.data(), result.size());
            return result;
        }

        // ==================== 迭代器 ====================
        class iterator {
        public:
            iterator(array *a, size_t i) : _array(a), _index(i) {
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

            T operator*() const { return (*_array)[_index]; }
            bool operator==(const iterator &o) const { return _array == o._array && _index == o._index; }
            bool operator!=(const iterator &o) const { return !(*this == o); }

        private:
            array *_array;
            size_t _index;
        };

        class const_iterator {
        public:
            const_iterator(const array *a, size_t i) : _array(a), _index(i) {
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

            T operator*() const { return (*_array)[_index]; }
            bool operator==(const const_iterator &o) const { return _array == o._array && _index == o._index; }
            bool operator!=(const const_iterator &o) const { return !(*this == o); }

        private:
            const array *_array;
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

        bool operator==(const array &other) const {
            if (length() != other.length()) return false;
            for (size_t i = 0; i < length(); ++i) {
                if ((*this)[i] != other[i]) return false;
            }
            return true;
        }

        bool operator!=(const array &other) const { return !(*this == other); }

        static array create_empty() { return array(0); }

        static array from_initializer_list(const std::initializer_list<T> &list) {
            return array(std::vector<T>(list));
        }

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
            else if constexpr (std::is_same_v<T, void *>) return PATCH_POINTER;
            else return PATCH_OBJECT;
        }
    };

    template<typename T>
    array<T> make_array(size_t size) { return array<T>(size); }

    template<typename T>
    array<T> make_array_from_vector(const std::vector<T> &vec) { return array<T>(vec); }

    template<typename T>
    array<T> make_array_from_list(const std::initializer_list<T> &list) {
        return array<T>::from_initializer_list(list);
    }
}
