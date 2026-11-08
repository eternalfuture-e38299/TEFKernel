/*******************************************************************************
 * tefkernel - entity_registry
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
 * Created: 2026/11/8
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <cstring>
#include <string_view>
#include <vector>

#include "tefstd/vector.h"   // tefstd_vector_t

namespace terraria::content {

template<typename T>
class entity_registry {
public:
    // ------------------------------------------------------------------
    // 注册 / 注销
    // ------------------------------------------------------------------
    bool register_entity(T *h) {
        if (!valid(h)) return false;

        // 热重载：同 (loader, mod, name) 替换句柄，保留 runtime_id
        auto it = std::ranges::find_if(items_,
            [&](T *e) { return same_identity(e, h); });
        if (it != items_.end()) {
            h->runtime_id = (*it)->runtime_id;
            *it = h;
            mirror_dirty_ = true;
            return true;
        }

        items_.push_back(h);
        mirror_dirty_ = true;
        return true;
    }

    bool unregister(T *h) {
        auto it = std::ranges::find(items_, h);
        if (it == items_.end()) return false;
        items_.erase(it);
        mirror_dirty_ = true;
        if (ids_assigned_) assign_ids(base_);
        return true;
    }

    bool unregister_by_id(int runtime_id) {
        auto it = std::ranges::find_if(items_,
            [&](T *e) { return e->runtime_id == runtime_id; });
        if (it == items_.end()) return false;
        items_.erase(it);
        mirror_dirty_ = true;
        if (ids_assigned_) assign_ids(base_);
        return true;
    }

    // ------------------------------------------------------------------
    // ID 分配（按 loader/mod/name 排序后从 base 起紧凑分配）
    // ------------------------------------------------------------------
    bool assign_ids(const int base) {
        base_ = base;
        std::ranges::sort(items_, &entity_registry::compare);
        for (size_t i = 0; i < items_.size(); ++i)
            items_[i]->runtime_id = base + static_cast<int>(i);
        ids_assigned_ = true;
        mirror_dirty_ = true;
        return true;
    }

    // ------------------------------------------------------------------
    // 查询
    // ------------------------------------------------------------------
    [[nodiscard]] T *get(const int runtime_id) const {
        const int nid = runtime_id - base_;
        if (nid < 0 || nid >= static_cast<int>(items_.size())) return nullptr;
        return items_[nid];
    }

    [[nodiscard]] T *find(std::string_view loader, std::string_view mod,
                          std::string_view name) const {
        for (auto *h : items_) {
            if (loader == h->parent_modloader_id &&
                mod    == h->parent_id &&
                name   == h->internal_name)
                return h;
        }
        return nullptr;
    }

    // ------------------------------------------------------------------
    // 遍历
    // ------------------------------------------------------------------
    [[nodiscard]] std::vector<T *> &all() { return items_; }
    [[nodiscard]] const std::vector<T *> &all() const { return items_; }
    [[nodiscard]] size_t size() const { return items_.size(); }
    [[nodiscard]] bool empty() const { return items_.empty(); }

    [[nodiscard]] bool ids_assigned() const { return ids_assigned_; }
    [[nodiscard]] int base() const { return base_; }

    // ------------------------------------------------------------------
    // ABI：terraria_xxx_manager_get_items() 返回 tefstd_vector_t*
    // ------------------------------------------------------------------
    tefstd_vector_t *raw_tefstd_vector() {
        if (!mirror_ready_) {
            if (!tefstd_vector_init(&mirror_, sizeof(T *))) return nullptr;
            mirror_ready_ = true;
            mirror_dirty_ = true;
        }
        if (mirror_dirty_) {
            tefstd_vector_clear(&mirror_);
            for (auto *h : items_) tefstd_vector_push_back(&mirror_, &h);
            mirror_dirty_ = false;
        }
        return &mirror_;
    }

    ~entity_registry() {
        if (mirror_ready_) tefstd_vector_destroy(&mirror_);
    }

    entity_registry() = default;
    entity_registry(const entity_registry &) = delete;
    entity_registry &operator=(const entity_registry &) = delete;

private:
    static bool valid(T *h) {
        return h && h->parent_modloader_id && h->parent_id && h->internal_name;
    }
    static bool same_identity(T *a, T *b) {
        return std::strcmp(a->parent_modloader_id, b->parent_modloader_id) == 0 &&
               std::strcmp(a->parent_id, b->parent_id) == 0 &&
               std::strcmp(a->internal_name, b->internal_name) == 0;
    }
    static int compare(T *a, T *b) {
        if (const int c = std::strcmp(a->parent_modloader_id, b->parent_modloader_id); c) return c;
        if (const int c = std::strcmp(a->parent_id, b->parent_id); c) return c;
        return std::strcmp(a->internal_name, b->internal_name);
    }

    std::vector<T *> items_;
    tefstd_vector_t mirror_{};
    bool mirror_ready_ = false;
    bool mirror_dirty_ = false;
    bool ids_assigned_ = false;
    int base_ = 0;
};

} // namespace terraria::content