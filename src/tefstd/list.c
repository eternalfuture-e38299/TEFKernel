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
 * Created: 2026/11/14
 *******************************************************************************/

#include "tefstd/list.h"
#include <stdlib.h>
#include <string.h>

static void tefstd_list_link(tefstd_list_node_t *prev,
                             tefstd_list_node_t *node,
                             tefstd_list_node_t *next) {
    prev->next = node;
    node->prev = prev;
    node->next = next;
    next->prev = node;
}

static void tefstd_list_unlink(const tefstd_list_node_t *node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

bool tefstd_list_init(tefstd_list_t *list, const size_t elem_size) {
    if (!list || elem_size == 0) return false;
    list->header.prev = &list->header;
    list->header.next = &list->header;
    list->header.data = NULL;
    list->size = 0;
    list->elem_size = elem_size;
    return true;
}

void tefstd_list_free(tefstd_list_t *list) {
    if (!list) return;
    tefstd_list_clear(list);
    list->header.prev = NULL;
    list->header.next = NULL;
    list->size = 0;
    list->elem_size = 0;
}

void tefstd_list_clear(tefstd_list_t *list) {
    if (!list || !list->header.next) return;
    tefstd_list_node_t *cur = list->header.next;
    while (cur != &list->header) {
        tefstd_list_node_t *next = cur->next;
        free(cur->data);
        free(cur);
        cur = next;
    }
    list->header.next = &list->header;
    list->header.prev = &list->header;
    list->size = 0;
}

static tefstd_list_node_t *tefstd_list_new_node(const tefstd_list_t *list,
                                                const void *elem) {
    tefstd_list_node_t *node = malloc(sizeof(*node));
    if (!node) return NULL;
    node->data = malloc(list->elem_size);
    if (!node->data) { free(node); return NULL; }
    memcpy(node->data, elem, list->elem_size);
    node->prev = node->next = NULL;
    return node;
}

bool tefstd_list_push_front(tefstd_list_t *list, const void *elem) {
    if (!list || !elem) return false;
    tefstd_list_node_t *node = tefstd_list_new_node(list, elem);
    if (!node) return false;
    tefstd_list_link(&list->header, node, list->header.next);
    list->size++;
    return true;
}

bool tefstd_list_push_back(tefstd_list_t *list, const void *elem) {
    if (!list || !elem) return false;
    tefstd_list_node_t *node = tefstd_list_new_node(list, elem);
    if (!node) return false;
    tefstd_list_link(list->header.prev, node, &list->header);
    list->size++;
    return true;
}

static bool tefstd_list_pop_node(tefstd_list_t *list,
                                 tefstd_list_node_t *node,
                                 void *out_elem) {
    if (out_elem) memcpy(out_elem, node->data, list->elem_size);
    tefstd_list_unlink(node);
    free(node->data);
    free(node);
    list->size--;
    return true;
}

bool tefstd_list_pop_front(tefstd_list_t *list, void *out_elem) {
    if (!list || list->size == 0) return false;
    return tefstd_list_pop_node(list, list->header.next, out_elem);
}

bool tefstd_list_pop_back(tefstd_list_t *list, void *out_elem) {
    if (!list || list->size == 0) return false;
    return tefstd_list_pop_node(list, list->header.prev, out_elem);
}

void *tefstd_list_front(const tefstd_list_t *list) {
    if (!list || list->size == 0) return NULL;
    return list->header.next->data;
}

void *tefstd_list_back(const tefstd_list_t *list) {
    if (!list || list->size == 0) return NULL;
    return list->header.prev->data;
}

size_t tefstd_list_size(const tefstd_list_t *list) {
    return list ? list->size : 0;
}

bool tefstd_list_empty(const tefstd_list_t *list) {
    return !list || list->size == 0;
}

tefstd_list_iter_t tefstd_list_iter(const tefstd_list_t *list) {
    tefstd_list_iter_t it;
    it.list = list;
    it.node = list ? list->header.next : NULL;
    return it;
}

bool tefstd_list_next(tefstd_list_iter_t *iter, void *out_elem) {
    if (!iter || !iter->list || !iter->node) return false;
    if (iter->node == &iter->list->header) return false;
    if (out_elem) memcpy(out_elem, iter->node->data, iter->list->elem_size);
    iter->node = iter->node->next;
    return true;
}

bool tefstd_list_erase(tefstd_list_iter_t *iter, void *out_elem) {
    if (!iter || !iter->list || !iter->node) return false;
    if (iter->node == &iter->list->header) return false;
    tefstd_list_node_t *node = iter->node;
    iter->node = node->next;
    return tefstd_list_pop_node((tefstd_list_t *)iter->list, node, out_elem);
}

tefstd_list_t *tefstd_list_new(size_t elem_size) {
    tefstd_list_t *l = malloc(sizeof(*l));
    if (!l) return NULL;
    if (!tefstd_list_init(l, elem_size)) { free(l); return NULL; }
    return l;
}