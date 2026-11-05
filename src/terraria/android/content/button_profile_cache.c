#include "button_profile_cache.h"

#include <stdio.h>
#include <string.h>

void button_profile_cache_init(button_profile_cache_t *cache) {
    if (!cache) return;
    memset(cache, 0, sizeof(*cache));
}

void button_profile_cache_invalidate(button_profile_cache_t *cache) {
    if (cache) cache->valid = false;
}

void button_profile_cache_get(button_profile_cache_t *cache,
                              unsigned long long tick,
                              button_profile_provider_t provider, void *user,
                              char *output, size_t size) {
    if (!cache || !provider || !output || size == 0) return;
    if (!cache->valid || cache->tick != tick) {
        provider(cache->value, sizeof(cache->value), user);
        cache->tick = tick;
        cache->valid = true;
    }
    snprintf(output, size, "%s", cache->value);
}
