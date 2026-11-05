#ifndef TEFKERNEL_BUTTON_PROFILE_CACHE_H
#define TEFKERNEL_BUTTON_PROFILE_CACHE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUTTON_PROFILE_CACHE_SIZE 1024

typedef void (*button_profile_provider_t)(char *output, size_t size,
                                         void *user);

typedef struct button_profile_cache_t {
    char value[BUTTON_PROFILE_CACHE_SIZE];
    unsigned long long tick;
    bool valid;
} button_profile_cache_t;

void button_profile_cache_init(button_profile_cache_t *cache);
void button_profile_cache_invalidate(button_profile_cache_t *cache);
void button_profile_cache_get(button_profile_cache_t *cache,
                              unsigned long long tick,
                              button_profile_provider_t provider, void *user,
                              char *output, size_t size);

#ifdef __cplusplus
}
#endif

#endif
