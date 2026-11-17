/*******************************************************************************
 * tefkernel - automatic PE buff count patching
 *
 * BuffID.Count is compiled into instruction immediates. This module discovers
 * all IL2CPP methods at runtime, limits scanning to Buff-related methods, and
 * rewrites validated Count boundary checks. No build-specific RVA table is
 * required.
 ******************************************************************************/

#include "internal/terraria/android/content/buff_pe_patch.h"

#include "internal/log.h"
#include "patchlib/il2cpp_api.h"
#include "patchlib/method.h"
#include "xdl.h"

#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX_METHOD_SCAN_BYTES (1024u * 1024u)

typedef struct {
    uintptr_t start;
    uintptr_t end;
    bool thumb;
    const char *class_name;
    const char *method_name;
} method_range_t;

typedef size_t (*image_class_count_t)(const void *image);
typedef const void *(*image_class_t)(const void *image, size_t index);

static int g_applied_total;
static bool g_initialized;
static void *g_il2cpp_handle;

static uintptr_t normalize_address(void *address) {
    uintptr_t result = (uintptr_t)address;
#if defined(__arm__)
    result &= ~(uintptr_t)1;
#endif
    return result;
}

static bool is_buff_related_method(const char *namespace_name,
                                   const char *class_name,
                                   const char *method_name) {
    /* 只改 GUIBuffs.Draw 里那一处 Count 过滤。
     * GUIBuffs 的其他方法（ItemDraw / 文本 / 网格）里的 400/401 可能是
     * 布局或循环常量，扩大后会越界/黑屏；Player.UpdateBuffs、AddBuff、
     * Lang.GetBuffName 等更是不能碰。 */
    if (class_name && strstr(class_name, "GUIBuffs") &&
        method_name && strcmp(method_name, "Draw") == 0) {
        return true;
    }
    (void)namespace_name;
    return false;
}

static bool write_executable(uintptr_t address, const void *value,
                             size_t size) {
    const size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    if (!page_size || !address || !value || !size) {
        return false;
    }

    const uintptr_t page_start =
            address & ~(uintptr_t)(page_size - 1);
    const uintptr_t page_end =
            (address + size + page_size - 1) &
            ~(uintptr_t)(page_size - 1);
    const size_t protection_size =
            (size_t)(page_end - page_start);

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

static bool arm64_is_cmp_immediate(uint32_t instruction,
                                   unsigned *immediate) {
    if ((instruction & 0x7f80001fu) != 0x7100001fu) {
        return false;
    }
    if (immediate) {
        *immediate = (instruction >> 10) & 0xfff;
    }
    return true;
}

static bool arm64_is_conditional_branch(uint32_t instruction) {
    return (instruction & 0xff000010u) == 0x54000000u;
}

static bool is_count_bound_condition(unsigned condition,
                                     bool count_minus_one) {
    if (count_minus_one) {
        return condition == 0x8 || /* HI */
               condition == 0x9 || /* LS */
               condition == 0xc || /* GT */
               condition == 0xd;   /* LE */
    }
    return condition == 0x2 || /* HS */
           condition == 0x3 || /* LO */
           condition == 0xb || /* LT */
           condition == 0xc || /* GE */
           condition == 0xd;   /* LE */
}

static int patch_arm64_range(uintptr_t start, uintptr_t end,
                             int old_count, int new_count) {
    if (new_count <= 0 || new_count > 0xfff) {
        return 0;
    }

    int patched = 0;
    for (uintptr_t address = start; address + sizeof(uint32_t) <= end;
         address += 4) {
        uint32_t instruction = 0;
        memcpy(&instruction, (const void *)address,
               sizeof(instruction));

        unsigned immediate = 0;
        if (!arm64_is_cmp_immediate(instruction, &immediate)) {
            continue;
        }

        int replacement_value = 0;
        if ((int)immediate == old_count) {
            replacement_value = new_count;
        } else if ((int)immediate == old_count - 1) {
            replacement_value = new_count - 1;
        } else if ((int)immediate == old_count - 2) {
            /* GUIBuffs.Draw: (buffType - 1) <= 0x18F (399 = Count-2) */
            replacement_value = new_count - 2;
        } else {
            continue;
        }

        bool has_conditional_branch = false;
        const bool checking_count_minus_one =
                (int)immediate == old_count - 1 ||
                (int)immediate == old_count - 2;
        for (uintptr_t next = address + 4;
             next <= address + 12 && next + 4 <= end;
             next += 4) {
            uint32_t branch = 0;
            memcpy(&branch, (const void *)next, sizeof(branch));
            if (arm64_is_conditional_branch(branch) &&
                is_count_bound_condition(
                        branch & 0xfu,
                        checking_count_minus_one)) {
                has_conditional_branch = true;
                break;
            }
        }
        if (!has_conditional_branch) {
            continue;
        }

        const uint32_t replacement =
                (instruction & ~(0xfffu << 10)) |
                ((uint32_t)replacement_value << 10);
        if (write_executable(address, &replacement,
                             sizeof(replacement))) {
            patched++;
            TEKLOG_INFO(
                    "buff PE: ARM64 compare patched at 0x%zx, %u -> %d",
                    (size_t)(address - start), immediate,
                    replacement_value);
        }
    }
    return patched;
}

static bool arm_is_cmp_immediate(uint32_t instruction,
                                 unsigned *immediate) {
    if ((instruction & 0x0ff00000u) != 0x03500000u) {
        return false;
    }
    if (immediate) {
        *immediate = instruction & 0xfffu;
    }
    return true;
}

static bool arm_is_conditional_branch(uint32_t instruction) {
    const unsigned condition = instruction >> 28;
    return condition != 0xf &&
           (instruction & 0x0e000000u) == 0x0a000000u;
}

static unsigned arm_condition(uint32_t instruction) {
    return instruction >> 28;
}

static int patch_arm_range(uintptr_t start, uintptr_t end,
                           int old_count, int new_count) {
    if (new_count <= 0 || new_count > 0xfff) {
        return 0;
    }

    int patched = 0;
    for (uintptr_t address = start; address + sizeof(uint32_t) <= end;
         address += 4) {
        uint32_t instruction = 0;
        memcpy(&instruction, (const void *)address,
               sizeof(instruction));

        unsigned immediate = 0;
        if (!arm_is_cmp_immediate(instruction, &immediate)) {
            continue;
        }

        int replacement_value = 0;
        if ((int)immediate == old_count) {
            replacement_value = new_count;
        } else if ((int)immediate == old_count - 1) {
            replacement_value = new_count - 1;
        } else if ((int)immediate == old_count - 2) {
            /* GUIBuffs.Draw: (buffType - 1) <= 0x18F (399 = Count-2) */
            replacement_value = new_count - 2;
        } else {
            continue;
        }

        bool has_conditional_branch = false;
        const bool checking_count_minus_one =
                (int)immediate == old_count - 1 ||
                (int)immediate == old_count - 2;
        for (uintptr_t next = address + 4;
             next <= address + 12 && next + 4 <= end;
             next += 4) {
            uint32_t branch = 0;
            memcpy(&branch, (const void *)next, sizeof(branch));
            if (arm_is_conditional_branch(branch) &&
                is_count_bound_condition(
                        arm_condition(branch),
                        checking_count_minus_one)) {
                has_conditional_branch = true;
                break;
            }
        }
        if (!has_conditional_branch) {
            continue;
        }

        const uint32_t replacement =
                (instruction & ~0xfffu) |
                (uint32_t)replacement_value;
        if (write_executable(address, &replacement,
                             sizeof(replacement))) {
            patched++;
            TEKLOG_INFO(
                    "buff PE: ARM compare patched at 0x%zx, %u -> %d",
                    (size_t)(address - start), immediate,
                    replacement_value);
        }
    }
    return patched;
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
        const unsigned unrotated =
                rotate_right(value, 32 - rotation);
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

static bool thumb_is_conditional_branch(uint16_t instruction) {
    const unsigned condition = (instruction >> 8) & 0xf;
    return (instruction & 0xf000) == 0xd000 && condition < 0xe;
}

static unsigned thumb_condition(uint16_t instruction) {
    return (instruction >> 8) & 0xf;
}

static bool thumb_is_conditional_branch_wide(uint16_t first,
                                             uint16_t second) {
    return (first & 0xf800) == 0xf000 &&
           (second & 0xd000) == 0x8000;
}

static bool thumb_is_32bit(uint16_t instruction) {
    const unsigned top = instruction >> 11;
    return top == 0x1d || top == 0x1e || top == 0x1f;
}

static int patch_thumb_range(uintptr_t start, uintptr_t end,
                             int old_count, int new_count) {
    int patched = 0;
    const int encoded_new = thumb_encode_imm12((unsigned)new_count);
    const int encoded_replaced =
            thumb_encode_imm12((unsigned)(new_count - 1));

    for (uintptr_t address = start;
         address + 4 <= end;
         address += 2) {
        uint16_t first = 0;
        uint16_t second = 0;
        memcpy(&first, (const void *)address, sizeof(first));
        memcpy(&second, (const void *)(address + 2),
               sizeof(second));

        if ((first & 0xfff0) != 0xf5b0 ||
            (second & 0x0f00) != 0x0f00) {
            continue;
        }

        const int immediate =
                thumb_decode_imm12(second & 0x0fff);
        int encoded = -1;
        int replacement_value = 0;
        if (immediate == old_count) {
            encoded = encoded_new;
            replacement_value = new_count;
        } else if (immediate == old_count - 1) {
            encoded = encoded_replaced;
            replacement_value = new_count - 1;
        } else if (immediate == old_count - 2) {
            encoded = thumb_encode_imm12((unsigned)(new_count - 2));
            replacement_value = new_count - 2;
        } else {
            continue;
        }

        uintptr_t branch_address = 0;
        size_t branch_size = 0;
        const bool checking_count_minus_one =
                immediate == old_count - 1 ||
                immediate == old_count - 2;
        uintptr_t next = address + 4;
        while (next < address + 20 && next + 2 <= end) {
            uint16_t branch_first = 0;
            memcpy(&branch_first, (const void *)next,
                   sizeof(branch_first));
            if (thumb_is_conditional_branch(branch_first) &&
                is_count_bound_condition(
                        thumb_condition(branch_first),
                        checking_count_minus_one)) {
                branch_address = next;
                branch_size = sizeof(branch_first);
                break;
            }

            if (thumb_is_32bit(branch_first) &&
                next + 4 <= end) {
                uint16_t branch_second = 0;
                memcpy(&branch_second,
                       (const void *)(next + 2),
                       sizeof(branch_second));
                if (thumb_is_conditional_branch_wide(
                            branch_first, branch_second) &&
                    is_count_bound_condition(
                            thumb_condition(branch_first),
                            checking_count_minus_one)) {
                    branch_address = next;
                    branch_size = 4;
                    break;
                }
                next += 4;
            } else {
                next += 2;
            }
        }
        if (!branch_address) {
            continue;
        }

        bool changed = false;
        if (encoded >= 0) {
            const uint16_t compare =
                    (uint16_t)(0x7000 | encoded);
            changed = write_executable(
                    address + 2, &compare, sizeof(compare));
        } else if (branch_size == 2) {
            const uint16_t nop = 0xbf00;
            changed = write_executable(
                    branch_address, &nop, sizeof(nop));
        } else {
            const uint16_t nop[2] = {0xf3af, 0x8000};
            changed = write_executable(
                    branch_address, nop, sizeof(nop));
        }

        if (changed) {
            patched++;
            TEKLOG_INFO(
                    "buff PE: Thumb compare patched at 0x%zx, %d -> %d",
                    (size_t)(address - start), immediate,
                    replacement_value);
        }
    }
    return patched;
}

static int compare_method_ranges(const void *left, const void *right) {
    const method_range_t *a = (const method_range_t *)left;
    const method_range_t *b = (const method_range_t *)right;
    if (a->start < b->start) return -1;
    if (a->start > b->start) return 1;
    return 0;
}


static int compare_uptr(const void *left, const void *right) {
    const uintptr_t a = *(const uintptr_t *) left;
    const uintptr_t b = *(const uintptr_t *) right;
    return (a > b) - (a < b);
}

static method_range_t *collect_methods(
        void *il2cpp_handle, size_t *out_count) {
    image_class_count_t image_class_count =
            (image_class_count_t)xdl_sym(
                    il2cpp_handle,
                    "il2cpp_image_get_class_count", NULL);
    image_class_t image_class =
            (image_class_t)xdl_sym(
                    il2cpp_handle,
                    "il2cpp_image_get_class", NULL);
    if (!image_class_count || !image_class) {
        return NULL;
    }

    size_t assembly_count = 0;
    void **assemblies = il2cpp_domain_get_assemblies(
            il2cpp_domain_get(), &assembly_count);
    if (!assemblies) {
        return NULL;
    }

    size_t capacity = 1024;
    size_t count = 0;
    method_range_t *ranges =
            malloc(capacity * sizeof(method_range_t));
    if (!ranges) {
        return NULL;
    }

    size_t all_capacity = 4096;
    size_t all_count = 0;
    uintptr_t *all_starts =
            malloc(all_capacity * sizeof(uintptr_t));
    if (!all_starts) {
        free(ranges);
        return NULL;
    }

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
            void *klass =
                    (void *)image_class(image, class_index);
            if (!klass) {
                continue;
            }

            const char *namespace_name =
                    il2cpp_class_get_namespace(klass);
            const char *class_name =
                    il2cpp_class_get_name(klass);
            void *iterator = NULL;
            void *method = NULL;

            while ((method = il2cpp_class_get_methods(
                            klass, &iterator)) != NULL) {
                const char *method_name =
                        il2cpp_method_get_name(method);
                void *entry =
                        patchlib_method_get_pointer(method);
                if (!entry) {
                    continue;
                }

                const uintptr_t normalized =
                        normalize_address(entry);
                Dl_info image_info = {0};
                if (!dladdr((const void *)normalized,
                            &image_info) ||
                    !image_info.dli_fname ||
                    !strstr(image_info.dli_fname,
                            "libil2cpp.so")) {
                    continue;
                }

                /* 记录所有方法的起点，后面用它把扫描范围裁到真实方法边界，
                 * 避免按“下一个匹配方法”计算 end 时扫进无关代码。 */
                if (all_count == all_capacity) {
                    all_capacity *= 2;
                    uintptr_t *grown = realloc(
                            all_starts,
                            all_capacity * sizeof(uintptr_t));
                    if (!grown) {
                        free(all_starts);
                        free(ranges);
                        return NULL;
                    }
                    all_starts = grown;
                }
                all_starts[all_count++] = normalized;

                if (!is_buff_related_method(
                            namespace_name, class_name,
                            method_name)) {
                    continue;
                }

                if (count == capacity) {
                    capacity *= 2;
                    method_range_t *grown = realloc(
                            ranges,
                            capacity * sizeof(method_range_t));
                    if (!grown) {
                        free(all_starts);
                        free(ranges);
                        return NULL;
                    }
                    ranges = grown;
                }

                ranges[count++] = (method_range_t){
                    .start = normalized,
                    .end = 0,
                    .thumb = ((uintptr_t)entry & 1u) != 0,
                    .class_name = class_name,
                    .method_name = method_name
                };
            }
        }
    }

    qsort(ranges, count, sizeof(method_range_t),
          compare_method_ranges);

    size_t unique = 0;
    for (size_t i = 0; i < count; ++i) {
        if (unique == 0 ||
            ranges[i].start != ranges[unique - 1].start) {
            ranges[unique++] = ranges[i];
        }
    }

    qsort(all_starts, all_count, sizeof(uintptr_t), compare_uptr);
    size_t all_unique = 0;
    for (size_t i = 0; i < all_count; ++i) {
        if (all_unique == 0 ||
            all_starts[i] != all_starts[all_unique - 1]) {
            all_starts[all_unique++] = all_starts[i];
        }
    }

    for (size_t i = 0; i < unique; ++i) {
        const uintptr_t start = ranges[i].start;
        uintptr_t next = start + MAX_METHOD_SCAN_BYTES;

        /* 二分找到真正紧邻的下一个方法起点 */
        size_t lo = 0, hi = all_unique;
        while (lo < hi) {
            const size_t mid = (lo + hi) / 2;
            if (all_starts[mid] <= start) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        if (lo < all_unique && all_starts[lo] < next) {
            next = all_starts[lo];
        }
        ranges[i].end = next;
    }

    free(all_starts);
    *out_count = unique;
    return ranges;
}

bool terraria_buff_pe_patch_init(void) {
#if !defined(__aarch64__) && !defined(__arm__)
    TEKLOG_WARN("buff PE: unsupported CPU architecture");
    return false;
#else
    g_il2cpp_handle = xdl_open("libil2cpp.so", XDL_DEFAULT);
    if (!g_il2cpp_handle) {
        TEKLOG_WARN("buff PE: libil2cpp.so not found");
        return false;
    }
    g_initialized = true;
    TEKLOG_INFO("buff PE: automatic method scanner enabled");
    return true;
#endif
}

void terraria_buff_pe_patch_apply(int vanilla_count,
                                  int total_buffs) {
    if (!g_initialized || vanilla_count <= 0 ||
        total_buffs <= vanilla_count ||
        total_buffs <= g_applied_total) {
        return;
    }

    if (g_applied_total < vanilla_count) {
        g_applied_total = vanilla_count;
    }

    if (!g_il2cpp_handle) {
        g_il2cpp_handle = xdl_open("libil2cpp.so", XDL_DEFAULT);
    }
    if (!g_il2cpp_handle) {
        TEKLOG_WARN("buff PE: libil2cpp disappeared during scan");
        return;
    }

    size_t method_count = 0;
    method_range_t *methods =
            collect_methods(g_il2cpp_handle, &method_count);

    if (!methods) {
        TEKLOG_WARN("buff PE: failed to enumerate IL2CPP methods");
        return;
    }

    int patched = 0;
    for (size_t i = 0; i < method_count; ++i) {
        method_range_t *method = &methods[i];
        if (method->end <= method->start) {
            continue;
        }

        int method_patched = 0;
#if defined(__aarch64__)
        method_patched = patch_arm64_range(
                method->start, method->end,
                g_applied_total, total_buffs);
#elif defined(__arm__)
        method_patched = method->thumb
                ? patch_thumb_range(
                        method->start, method->end,
                        g_applied_total, total_buffs)
                : patch_arm_range(
                        method->start, method->end,
                        g_applied_total, total_buffs);
#endif
        if (method_patched > 0) {
            patched += method_patched;
            TEKLOG_INFO(
                    "buff PE: patched %d check(s) in %s",
                    method_patched,
                    method->method_name
                            ? method->method_name : "?");
        }
    }

    free(methods);
    g_applied_total = total_buffs;
    TEKLOG_INFO(
            "buff PE: automatic scan complete, %zu methods, "
            "%d check(s), upper bound %d",
            method_count, patched, total_buffs);
}
