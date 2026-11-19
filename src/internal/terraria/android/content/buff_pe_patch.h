/*******************************************************************************
 * tefkernel - PE buff instruction patches (internal)
 ******************************************************************************/
#ifndef TEFKERNEL_INTERNAL_BUFF_PE_PATCH_H
#define TEFKERNEL_INTERNAL_BUFF_PE_PATCH_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Detect the running libil2cpp.so build and cache its patch profile.
 *
 * @return true when the build is recognized and instruction patching is
 *         available.
 */
bool terraria_buff_pe_patch_init(void);

/**
 * Extend embedded BuffID.Count checks to include all registered buff IDs.
 *
 * The function is idempotent. It only applies patches when the requested
 * upper bound grows.
 */
void terraria_buff_pe_patch_apply(int vanilla_count, int total_buffs);

#ifdef __cplusplus
}
#endif

#endif
