/*******************************************************************************
 * tefkernel - entity
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
 * Created: 2026/11/5
 *******************************************************************************/

#include "internal/terraria/entity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t terraria_entity_key_size(const terraria_entity_handle_t *e, const char *prefix) {
    if (!e || !prefix) return 0;
    size_t n = strlen(prefix) + 4;  // '_' '_' '.' '\0'
    n += e->parent_modloader_id ? strlen(e->parent_modloader_id) : 1;
    n += e->parent_id ? strlen(e->parent_id) : 1;
    n += e->internal_name ? strlen(e->internal_name) : 1;
    return n;
}

char * terraria_entity_make_key(const terraria_entity_handle_t *e, const char *prefix) {
    if (!e || !prefix) return NULL;

    const size_t size = terraria_entity_key_size(e, prefix);
    if (size == 0) return NULL;

    char *key = malloc(size);
    if (!key) return NULL;

    snprintf(key, size, "%s_%s_%s.%s", prefix,
             e->parent_modloader_id ? e->parent_modloader_id : "?",
             e->parent_id ? e->parent_id : "?",
             e->internal_name ? e->internal_name : "?");
    return key;
}

