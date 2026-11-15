/*******************************************************************************
 * tefkernel - PE tile instruction patches
 *
 * TileID.Count is a compile-time constant on IL2CPP. The original value is
 * embedded in compare instructions, so the field cannot be resized at runtime.
 * This module identifies known libil2cpp builds, locates Count checks inside
 * named game methods, and edits only verified compare/branch sequences.
 ******************************************************************************/

#include "internal/terraria/android/content/tile_pe_patch.h"

#include "internal/log.h"
#include "patchlib/il2cpp_api.h"
#include "patchlib/method.h"
#include "patchlib/type.h"
#include "xdl.h"

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define TILE_PE_SCAN_BYTES (64u * 1024u)

typedef struct {
    unsigned long long size;
    long text_offset;
    unsigned char text_prefix[16];
    const char *name;
} tile_pe_profile_t;

typedef struct {
    const char *namespace_name;
    const char *type_name;
    const char *method_name;
    int parameter_count;
} tile_pe_method_t;

static const tile_pe_profile_t g_profiles[] = {
    {
        54823152ULL,
        0x5a0810,
        {0x88, 0x63, 0x01, 0xf0, 0x81, 0x63, 0x01, 0xf0,
         0x60, 0x08, 0x00, 0xf0, 0xe2, 0x4d, 0x01, 0x90},
        "arm64-1e896547"
    },
    {
        54781968ULL,
        0x5a0810,
        {0x88, 0x63, 0x01, 0xf0, 0x81, 0x63, 0x01, 0xf0,
         0x60, 0x08, 0x00, 0xf0, 0xe2, 0x4d, 0x01, 0x90},
        "arm64-f33b48c0"
    },
    {
        56820880ULL,
        0x636c00,
        {0x48, 0x80, 0x01, 0xb0, 0x41, 0x80, 0x01, 0xb0,
         0x20, 0x09, 0x00, 0xf0, 0x02, 0x68, 0x01, 0xd0},
        "arm64-e4832743"
    },
    {
        56149256ULL,
        0x204960,
        {0x10, 0x40, 0x2d, 0xe9, 0x5c, 0x10, 0x9f, 0xe5,
         0x00, 0x40, 0xa0, 0xe3, 0x58, 0x00, 0x9f, 0xe5},
        "armv7-304c64bf"
    },
    {
        59810168ULL,
        0x2369b0,
        {0x10, 0x40, 0x2d, 0xe9, 0x5c, 0x10, 0x9f, 0xe5,
         0x00, 0x40, 0xa0, 0xe3, 0x58, 0x00, 0x9f, 0xe5},
        "armv7-0e4ef356"
    }
};

static const tile_pe_method_t g_methods[] = {
    {"Terraria", "WorldGen", "PlaceTile", 7},
    {"Terraria", "WorldGen", "KillTile", 5},
    {"Terraria", "WorldGen", "KillTile_DropItems", 4},
    {"Terraria", "WorldGen", "KillTile_DropItems", 5},
    {"Terraria", "WorldGen", "TileFrame", 3},
    {"Terraria", "WorldGen", "CanKillTile", 2},
    {"Terraria", "Player", "PlaceThing", 0},
    {"Terraria", "Player", "PickTile", 3},
    {"Terraria", "TileData", "SetType", 2},
    {"Terraria", "TileData", "GetTileDefinition", 8},
    {"Terraria", "TileData", "ReregisterTile", 1}
};

static const char *g_profile_name;
static int g_applied_total;
static uintptr_t g_image_base;
static char g_image_path[512];

static uintptr_t executable_address(void *address) {
    uintptr_t result = (uintptr_t)address;
#if defined(__arm__)
    result &= ~(uintptr_t)1;
#endif
    return result;
}

static const char *identify_profile(void *method) {
    Dl_info image = {0};
    void *address = (void *)executable_address(method);

    if (!address || !dladdr(address, &image) || !image.dli_fname ||
        !strstr(image.dli_fname, "libil2cpp.so")) {
        return NULL;
    }

    struct stat info;
    if (stat(image.dli_fname, &info) != 0) {
        return NULL;
    }

    for (size_t i = 0; i < sizeof(g_profiles) / sizeof(g_profiles[0]); ++i) {
        const tile_pe_profile_t *profile = &g_profiles[i];
        if ((unsigned long long)info.st_size != profile->size) {
            continue;
        }

        unsigned char prefix[sizeof(profile->text_prefix)] = {0};
        FILE *file = fopen(image.dli_fname, "rb");
        if (!file) {
            return NULL;
        }

        const bool matches =
                fseek(file, profile->text_offset, SEEK_SET) == 0 &&
                fread(prefix, 1, sizeof(prefix), file) == sizeof(prefix) &&
                memcmp(prefix, profile->text_prefix, sizeof(prefix)) == 0;
        fclose(file);

        if (matches) {
            return profile->name;
        }
    }
    return NULL;
}

static bool is_arm64_profile(void) {
    return g_profile_name &&
           strncmp(g_profile_name, "arm64-", 6) == 0;
}

static bool write_executable(uintptr_t address, const void *value,
                             size_t size) {
    const size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    if (!page_size || !address || !value || !size) {
        return false;
    }

    const uintptr_t page_start = address & ~(uintptr_t)(page_size - 1);
    const uintptr_t page_end =
            (address + size + page_size - 1) &
            ~(uintptr_t)(page_size - 1);
    const size_t protection_size = (size_t)(page_end - page_start);

    if (mprotect((void *)page_start, protection_size,
                 PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        return false;
    }

    memcpy((void *)address, value, size);
    __builtin___clear_cache((char *)address, (char *)(address + size));

    if (mprotect((void *)page_start, protection_size,
                 PROT_READ | PROT_EXEC) != 0) {
        return false;
    }
    return true;
}

static bool is_arm64_conditional_branch(uint32_t instruction) {
    return (instruction & 0xff000010u) == 0x54000000u;
}

static bool is_arm64_gt_branch(uint32_t instruction) {
    return is_arm64_conditional_branch(instruction) &&
           (instruction & 0xfu) == 0xcu;
}

static int patch_arm64_count_checks(uintptr_t address, size_t scan_size,
                                    int old_count, int new_count) {
    if (old_count <= 0 || new_count <= 0 || new_count > 0xfff) {
        return 0;
    }

    int patched = 0;
    for (size_t offset = 0; offset + sizeof(uint32_t) <= scan_size;
         offset += 4) {
        uint32_t instruction = 0;
        memcpy(&instruction, (const void *)(address + offset),
               sizeof(instruction));

        if ((instruction & 0x7f80001fu) != 0x7100001fu) {
            continue;
        }

        const int immediate = (int)((instruction >> 10) & 0xfff);
        int replacement_count = 0;
        if (immediate == old_count) {
            replacement_count = new_count;
        } else if (old_count > 0 && immediate == old_count - 1) {
            replacement_count = new_count - 1;
        } else {
            continue;
        }

        bool has_branch = false;
        for (size_t next = 4; next <= 24; next += 4) {
            uint32_t branch = 0;
            memcpy(&branch, (const void *)(address + offset + next),
                   sizeof(branch));
            if (is_arm64_conditional_branch(branch) &&
                (immediate != old_count - 1 ||
                 is_arm64_gt_branch(branch))) {
                has_branch = true;
                break;
            }
        }
        if (!has_branch) {
            continue;
        }

        const uint32_t replacement =
                (instruction & ~(0xfffu << 10)) |
                ((uint32_t)replacement_count << 10);
        if (write_executable(address + offset, &replacement,
                             sizeof(replacement))) {
            patched++;
            TEKLOG_INFO("tile PE compare patched at +0x%zx: %d -> %d",
                        offset, immediate, replacement_count);
        }
    }
    return patched;
}

static bool is_thumb_conditional_branch(uint16_t instruction) {
    const unsigned condition = (instruction >> 8) & 0xf;
    return (instruction & 0xf000) == 0xd000 && condition < 0xe;
}

static bool is_thumb_conditional_branch_wide(uint16_t first,
                                             uint16_t second) {
    return (first & 0xf800) == 0xf000 &&
           (second & 0xd000) == 0x8000;
}

static bool is_thumb_32bit(uint16_t instruction) {
    const unsigned top = instruction >> 11;
    return top == 0x1d || top == 0x1e || top == 0x1f;
}

static unsigned rotate_right(unsigned value, unsigned amount) {
    amount &= 31;
    if (!amount) {
        return value;
    }
    return (value >> amount) | (value << (32 - amount));
}

static int thumb_decode_imm12(unsigned code) {
    if (code < 4) {
        return code == 0 ? 0 : (0xff << (8 * (code - 1)));
    }

    const unsigned unrotated = (code & 0x7f) | 0x80;
    return (int)rotate_right(unrotated, code >> 7);
}

static int thumb_encode_imm12(unsigned value) {
    for (unsigned rotation = 0; rotation < 32; ++rotation) {
        const unsigned unrotated = rotate_right(value, 32 - rotation);
        if (unrotated <= 0xff && (unrotated & 0x80)) {
            const unsigned code =
                    (rotation << 7) | (unrotated & 0x7f);
            if (code >= 4) {
                return (int)code;
            }
        }
    }
    return -1;
}

static int patch_thumb_count_checks(uintptr_t address, size_t scan_size,
                                    int old_count, int new_count) {
    int patched = 0;
    const int encoded = thumb_encode_imm12((unsigned)new_count);

    for (size_t offset = 0; offset + sizeof(uint32_t) <= scan_size;
         offset += 2) {
        uint16_t first = 0;
        uint16_t second = 0;
        memcpy(&first, (const void *)(address + offset), sizeof(first));
        memcpy(&second, (const void *)(address + offset + 2),
               sizeof(second));

        if ((first & 0xfff0) != 0xf5b0 ||
            (second & 0x0f00) != 0x0f00) {
            continue;
        }
        if (thumb_decode_imm12(second & 0x0fff) != old_count) {
            continue;
        }

        size_t branch_offset = 0;
        size_t branch_size = 0;
        size_t next = 4;
        while (next <= 32) {
            uint16_t branch_first = 0;
            memcpy(&branch_first,
                   (const void *)(address + offset + next),
                   sizeof(branch_first));

            if (is_thumb_conditional_branch(branch_first)) {
                branch_offset = offset + next;
                branch_size = sizeof(branch_first);
                break;
            }

            if (is_thumb_32bit(branch_first)) {
                uint16_t branch_second = 0;
                memcpy(&branch_second,
                       (const void *)(address + offset + next + 2),
                       sizeof(branch_second));
                if (is_thumb_conditional_branch_wide(
                            branch_first, branch_second)) {
                    branch_offset = offset + next;
                    branch_size = 2 * sizeof(branch_first);
                    break;
                }
                next += 4;
            } else {
                next += 2;
            }
        }
        if (!branch_size) {
            continue;
        }

        bool changed = false;
        if (encoded >= 0) {
            const uint16_t compare =
                    (uint16_t)(0x7000 | encoded);
            changed = write_executable(address + offset + 2,
                                       &compare, sizeof(compare));
        } else if (branch_size == 2) {
            const uint16_t nop = 0xbf00;
            changed = write_executable(address + branch_offset,
                                       &nop, sizeof(nop));
        } else {
            const uint16_t nop_wide[2] = {0xf3af, 0x8000};
            changed = write_executable(address + branch_offset,
                                       nop_wide, sizeof(nop_wide));
        }

        if (changed) {
            patched++;
            TEKLOG_INFO("tile PE Thumb upper-bound branch patched at +0x%zx",
                        branch_offset);
        }
    }
    return patched;
}

static patch_handle_t find_named_method(patch_handle_t type,
                                        const char *name,
                                        int parameter_count) {
    patch_handle_t method =
            patchlib_type_get_method_by_param_count(
                    type, name, parameter_count);
    if (patchlib_is_valid(method)) {
        return method;
    }

    for (int count = 0; count <= 8; ++count) {
        if (count == parameter_count) {
            continue;
        }
        method = patchlib_type_get_method_by_param_count(type, name, count);
        if (patchlib_is_valid(method)) {
            TEKLOG_INFO("tile PE method %s resolved with %d parameters",
                        name, count);
            return method;
        }
    }
    return PATCH_NULL;
}

static int patch_method(patch_handle_t method, const char *name,
                        int old_count, int new_count) {
    void *entry = patchlib_method_get_pointer(method);
    if (!entry || identify_profile(entry) != g_profile_name) {
        TEKLOG_WARN("tile PE method %s belongs to an unexpected image", name);
        return 0;
    }

    const uintptr_t address = executable_address(entry);
    const uintptr_t offset = address - g_image_base;
    const int patched = is_arm64_profile()
            ? patch_arm64_count_checks(
                    address, TILE_PE_SCAN_BYTES,
                    old_count, new_count)
            : patch_thumb_count_checks(
                    address, TILE_PE_SCAN_BYTES,
                    old_count, new_count);

    const tefkernel_log_level_t level =
            patched > 0 ? TEFKERNEL_LOG_LEVEL_INFO
                        : TEFKERNEL_LOG_LEVEL_WARN;
    TEKLOG(level,
           "tile PE %s: %s at +0x%zx, count %d -> %d, patched %d",
           g_profile_name, name, (size_t)offset,
           old_count, new_count, patched);
    return patched;
}

typedef struct {
    const char *namespace_name;
    const char *type_name;
} diagnostic_type_t;

static const diagnostic_type_t g_diagnostic_types[] = {
    {"Terraria", "Player"},
    {"Terraria", "WorldGen"},
    {"Terraria", "Collision"},
    {"Terraria", "Item"},
    {"Terraria", "Main"},
    {"Terraria", "NetMessage"},
    {"Terraria.ObjectData", "TileObjectData"}
};

static const size_t g_crash_offsets[] = {
    0x1053f7c,
    0x1055018,
    0x1060da0,
    0x1301164,
    0x1381f64,
    0x138303c,
    0x13acd18,
    0x13af288,
    0x13b2338,
    0x13b8bcc,
    0x13cc3a8,
    0x16d9428,
    0x16de5c8,
    0x1722c6c
};

static void log_crash_method_candidates(void) {
    for (size_t target_index = 0;
         target_index < sizeof(g_crash_offsets) /
                                sizeof(g_crash_offsets[0]);
         ++target_index) {
        const uintptr_t target = g_crash_offsets[target_index];
        int best_distance = 0x200000;
        const char *best_type_name = NULL;
        const char *best_method_name = NULL;
        uintptr_t best_start = 0;

        for (size_t type_index = 0;
             type_index < sizeof(g_diagnostic_types) /
                                  sizeof(g_diagnostic_types[0]);
             ++type_index) {
            const diagnostic_type_t *type_info =
                    &g_diagnostic_types[type_index];
            patch_handle_t type = patchlib_type_get_type(
                    type_info->namespace_name, type_info->type_name);
            if (!patchlib_is_valid(type)) {
                continue;
            }

            tefstd_vector_t methods;
            tefstd_vector_init(&methods, sizeof(patch_handle_t));
            patchlib_type_get_methods(type, false, &methods);
            for (size_t i = 0;
                 i < tefstd_vector_size(&methods);
                 ++i) {
                patch_handle_t method =
                        *(patch_handle_t *)tefstd_vector_at(
                                &methods, i);
                void *entry = patchlib_is_valid(method)
                        ? patchlib_method_get_pointer(method)
                        : NULL;
                const uintptr_t start =
                        executable_address(entry) - g_image_base;
                if (entry && start <= target) {
                    const int distance = (int)(target - start);
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_type_name = type_info->type_name;
                        best_method_name =
                                patchlib_method_get_name(method);
                        best_start = start;
                    }
                }
                if (patchlib_is_valid(method)) {
                    patchlib_free(method);
                }
            }
            tefstd_vector_destroy(&methods);
            patchlib_free(type);
        }

        if (best_method_name) {
            TEKLOG_INFO(
                    "tile PE crash candidate +0x%zx: %s.%s+0x%zx",
                    g_crash_offsets[target_index], best_type_name,
                    best_method_name,
                    g_crash_offsets[target_index] - best_start);
        }
    }
}

typedef size_t (*image_class_count_t)(const void *image);
typedef const void *(*image_class_t)(const void *image, size_t index);

static void log_all_crash_method_candidates(void) {
    void *library = xdl_open("libil2cpp.so", XDL_DEFAULT);
    if (!library) {
        TEKLOG_WARN("tile PE: failed to reopen libil2cpp for diagnostics");
        return;
    }

    image_class_count_t image_class_count =
            (image_class_count_t)xdl_sym(
                    library, "il2cpp_image_get_class_count", NULL);
    image_class_t image_class =
            (image_class_t)xdl_sym(
                    library, "il2cpp_image_get_class", NULL);
    if (!image_class_count || !image_class) {
        TEKLOG_WARN("tile PE: image class enumeration is unavailable");
        xdl_close(library);
        return;
    }

    size_t assembly_count = 0;
    void **assemblies = il2cpp_domain_get_assemblies(
            il2cpp_domain_get(), &assembly_count);
    if (!assemblies) {
        xdl_close(library);
        return;
    }

    for (size_t target_index = 0;
         target_index < sizeof(g_crash_offsets) /
                                sizeof(g_crash_offsets[0]);
         ++target_index) {
        const uintptr_t target_offset =
                g_crash_offsets[target_index];
        int best_distance = 0x400000;
        uintptr_t best_start = 0;
        const char *best_namespace = NULL;
        const char *best_type = NULL;
        const char *best_method = NULL;

        for (size_t assembly_index = 0;
             assembly_index < assembly_count;
             ++assembly_index) {
            const void *image = il2cpp_assembly_get_image(
                    assemblies[assembly_index]);
            if (!image) {
                continue;
            }

            const size_t class_count = image_class_count(image);
            for (size_t class_index = 0;
                 class_index < class_count;
                 ++class_index) {
                void *klass = (void *)image_class(
                        image, class_index);
                if (!klass) {
                    continue;
                }

                void *iterator = NULL;
                void *method = NULL;
                while ((method = il2cpp_class_get_methods(
                                klass, &iterator)) != NULL) {
                    void *entry =
                            patchlib_method_get_pointer(method);
                    if (!entry) {
                        continue;
                    }

                    const uintptr_t start =
                            executable_address(entry) -
                            g_image_base;
                    if (start > target_offset) {
                        continue;
                    }
                    const int distance =
                            (int)(target_offset - start);
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_start = start;
                        best_namespace =
                                il2cpp_class_get_namespace(klass);
                        best_type = il2cpp_class_get_name(klass);
                        best_method =
                                il2cpp_method_get_name(method);
                    }
                }
            }
        }

        if (best_method) {
            TEKLOG_INFO(
                    "tile PE exact candidate +0x%zx: "
                    "%s.%s.%s+0x%zx",
                    target_offset,
                    best_namespace ? best_namespace : "",
                    best_type ? best_type : "?",
                    best_method, target_offset - best_start);
        }
    }

    xdl_close(library);
}

bool terraria_tile_pe_patch_init(void) {
    patch_handle_t worldgen =
            patchlib_type_get_type("Terraria", "WorldGen");
    if (!patchlib_is_valid(worldgen)) {
        TEKLOG_WARN("tile PE: Terraria.WorldGen not found");
        return false;
    }

    patch_handle_t place_tile = find_named_method(
            worldgen, "PlaceTile", 7);
    patchlib_free(worldgen);

    if (!patchlib_is_valid(place_tile)) {
        TEKLOG_WARN("tile PE: WorldGen.PlaceTile not found");
        return false;
    }

    void *entry = patchlib_method_get_pointer(place_tile);
    patchlib_free(place_tile);

    g_profile_name = identify_profile(entry);
    if (!g_profile_name) {
        TEKLOG_WARN("tile PE build is not recognized; Count checks stay unchanged");
        return false;
    }

    Dl_info image = {0};
    const void *address = (const void *)executable_address(entry);
    if (!dladdr(address, &image) || !image.dli_fbase) {
        TEKLOG_WARN("tile PE: failed to resolve libil2cpp image base");
        g_profile_name = NULL;
        return false;
    }
    g_image_base = (uintptr_t)image.dli_fbase;
    if (image.dli_fname) {
        snprintf(g_image_path, sizeof(g_image_path), "%s",
                 image.dli_fname);
    }

    TEKLOG_INFO("tile PE profile %s: image base %p",
                g_profile_name, (void *)g_image_base);
    log_crash_method_candidates();
    log_all_crash_method_candidates();
    return true;
}

void terraria_tile_pe_patch_apply(int vanilla_count, int total_tiles) {
    if (!g_profile_name || vanilla_count <= 0 ||
        total_tiles <= vanilla_count) {
        return;
    }

    if (g_applied_total < vanilla_count) {
        g_applied_total = vanilla_count;
    }
    if (total_tiles <= g_applied_total) {
        return;
    }

    const int old_count = g_applied_total;
    int total_patched = 0;

    for (size_t i = 0; i < sizeof(g_methods) / sizeof(g_methods[0]); ++i) {
        const tile_pe_method_t *target = &g_methods[i];
        patch_handle_t type = patchlib_type_get_type(
                target->namespace_name, target->type_name);
        if (!patchlib_is_valid(type)) {
            TEKLOG_WARN("tile PE: target type %s.%s not found",
                        target->namespace_name, target->type_name);
            continue;
        }

        patch_handle_t method = find_named_method(
                type, target->method_name, target->parameter_count);
        if (!patchlib_is_valid(method)) {
            TEKLOG_WARN("tile PE: %s.%s.%s/%d not found",
                        target->namespace_name, target->type_name,
                        target->method_name,
                        target->parameter_count);
            patchlib_free(type);
            continue;
        }

        total_patched += patch_method(
                method, target->method_name, old_count, total_tiles);
        patchlib_free(method);
        patchlib_free(type);
    }

    g_applied_total = total_tiles;
    TEKLOG_INFO("tile PE Count patch complete: %d check(s), upper bound %d",
                total_patched, total_tiles);
}
