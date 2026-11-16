/*******************************************************************************
 * tefkernel - scope
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

#include "tefstd/scope.h"

#include <stdlib.h>

void tefstd_scope_push(tefstd_scope_list_t *list, tefstd_scope_node_t *node,
                       void *ptr, const tefstd_scope_release_fn fn) {
    (void)node;
    if (!list || !fn) return;

    tefstd_scope_node_t *owned = malloc(sizeof(*owned));
    if (!owned) return;

    owned->ptr = ptr;
    owned->fn = fn;
    owned->next = list->head;
    list->head = owned;
}

void tefstd_scope_run(tefstd_scope_list_t *list) {
    while (list->head) {
        tefstd_scope_node_t *n = list->head;
        list->head = n->next;
        n->fn(n->ptr);   /* 后登记的先释放 */
        free(n);
    }
}

bool tefstd_scope_detach(tefstd_scope_list_t *list, const void *ptr) {
    tefstd_scope_node_t **pp = &list->head;
    while (*pp) {
        if ((*pp)->ptr == ptr) {
            tefstd_scope_node_t *owned = *pp;
            *pp = (*pp)->next;
            free(owned);
            return true;
        }
        pp = &(*pp)->next;
    }
    return false;
}
