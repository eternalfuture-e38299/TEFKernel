"""PE 触摸按键「槽位 -> 动作」查找的宿主机回归测试。

Mod 按钮在原生布局里存的是一个「未绑定」动作，真正证明某个槽位属于 Mod 的
是槽位映射表。这里覆盖两类回归：

* 没有映射的未绑定槽位必须仍然是原版的空槽位；
* 存档时恰好写进布局的 carrier 动作 id 只要映射还在就必须继续解析，
  否则下次启动游戏时 Mod 按钮就会变成一个空壳。
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/terraria/android/content/button_manager.c').read_text(encoding='utf-8')


def extract(name):
    matches = list(re.finditer(r'static (?:bool|void) ' + name + r'\([^;]*?\)\s*\{', source))
    m = matches[-1]
    pos, depth = m.end(), 1
    while depth:
        if source[pos] == '{':
            depth += 1
        if source[pos] == '}':
            depth -= 1
        pos += 1
    return source[m.start():pos]


harness = r'''
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define PATCH_NULL NULL
#define BUTTON_MAX_RENDER_BINDINGS 8
#define BUTTON_MAX_GROUP_BINDINGS 8

typedef void *patch_handle_t;

typedef struct binding_t {
    int action;
    const char *slot_id;
    bool groups;
} binding_t;

static const int unbound_action = 28;
static const int carrier_action = 27;

typedef struct { patch_handle_t array; size_t index; } button_render_group_t;
static patch_handle_t render_bindings[BUTTON_MAX_RENDER_BINDINGS];
static size_t render_binding_count;
static button_render_group_t render_groups[BUTTON_MAX_GROUP_BINDINGS];
static size_t render_group_count;

static struct { void *binding_slot, *binding_action, *binding_groups; } api;
static int state;
static int groups[4];
static const char *mapping_slot;
static const char *mapping_key;

static void copy_text(char *d, size_t n, const char *s) {
    if (!n) return;
    while (--n && *s) *d++ = *s++;
    *d = 0;
}
static patch_handle_t field_read_handle(void *field, void *object) {
    (void)field;
    return object;
}
static int field_read_int(void *field, void *object) {
    (void)field;
    return ((binding_t *)object)->action;
}
static size_t patchlib_array_length(void *array) { (void)array; return 4; }
static bool patchlib_array_at(void *array, size_t index, int *out) {
    (void)array;
    *out = groups[index];
    return true;
}
static void copy_managed_string(char *d, size_t n, void *handle) {
    copy_text(d, n, handle ? ((binding_t *)handle)->slot_id : "");
}
static void profile_key(char *d, size_t n) { copy_text(d, n, "profile-1"); }
static const char *button_state_get_binding(void *s, const char *profile,
                                            const char *slot) {
    (void)s;
    (void)profile;
    if (mapping_slot && mapping_key && strcmp(slot, mapping_slot) == 0) {
        return mapping_key;
    }
    return "";
}
'''

tests = r'''
static int failures;
static binding_t binding;
static char output[1024];

static void reset(void) {
    binding.action = unbound_action;
    binding.slot_id = "slot-a";
    binding.groups = false;
    for (int i = 0; i < 4; ++i) groups[i] = unbound_action;
    render_binding_count = 0;
    render_group_count = 0;
    mapping_slot = NULL;
    mapping_key = NULL;
}
static void map(const char *slot, const char *key) {
    mapping_slot = slot;
    mapping_key = key;
}
static void expect(const char *want, int bit) {
    custom_binding(&binding, binding.groups ? 1 : -1, output, sizeof(output));
    if (strcmp(output, want) != 0) failures |= bit;
}

int run_tests(void) {
    reset();
    map("slot-a", "mod.a:attack");
    expect("mod.a:attack", 1);              /* 未绑定槽位 + 映射 */
    reset();
    map("slot-b", "mod.a:attack");
    expect("", 2);                          /* 未绑定槽位属于原版 */

    reset();
    binding.action = carrier_action;
    map("slot-a", "mod.a:attack");
    expect("mod.a:attack", 4);              /* 残留 carrier 仍保住按钮 */
    reset();
    binding.action = carrier_action;
    map("slot-b", "mod.a:attack");
    expect("", 8);                          /* 原版 carrier 不受影响 */
    reset();
    binding.action = carrier_action;
    map("slot-a", "mod.a:attack");
    render_bindings[render_binding_count++] = &binding;
    expect("mod.a:attack", 16);             /* 本帧正在替换 */
    reset();
    binding.action = 5;
    map("slot-a", "mod.a:attack");
    expect("", 32);                         /* 原版动作优先于映射 */

    reset();
    binding.groups = true;
    groups[1] = unbound_action;
    map("slot-a::group:1", "mod.a:attack");
    expect("mod.a:attack", 64);             /* 环形（分组）槽位 */
    reset();
    binding.groups = true;
    groups[1] = carrier_action;
    map("slot-a::group:1", "mod.a:attack");
    expect("mod.a:attack", 128);            /* 分组里的残留 carrier */
    reset();
    binding.groups = true;
    groups[1] = carrier_action;
    map("slot-b::group:1", "mod.a:attack");
    expect("", 256);                        /* 原版分组 carrier */
    reset();
    binding.groups = true;
    groups[1] = 5;
    map("slot-a::group:1", "mod.a:attack");
    expect("", 512);                        /* 原版分组动作 */

    reset();
    binding.slot_id = "";
    map("slot-a", "mod.a:attack");
    expect("", 1024);                       /* 没有 SlotId 的绑定 */
    return failures;
}
'''

with tempfile.TemporaryDirectory(prefix='button-binding-lookup-') as tmp:
    tmp = Path(tmp)
    program = tmp / 'test.c'
    program.write_text(
        harness
        + extract('render_binding_contains')
        + '\n'
        + extract('render_group_contains')
        + '\n'
        + extract('group_slot')
        + '\n'
        + extract('custom_binding')
        + tests,
        encoding='utf-8',
    )
    driver = tmp / 'driver.c'
    driver.write_text('int run_tests(void);\nint main(void) { return run_tests(); }\n',
                      encoding='utf-8')
    binary = tmp / 'test'
    subprocess.run(['cc', '-std=c17', '-Wall', '-Wextra', '-Werror',
                    str(program), str(driver), '-o', str(binary)], check=True)
    result = subprocess.run([str(binary)]).returncode

if result:
    raise SystemExit(
        'FAIL bitmask=%d: 1=未绑定,2=原版空槽,4=残留carrier,8=原版carrier,'
        '16=本帧替换,32=原版动作,64=分组,128=分组残留carrier,'
        '256=原版分组carrier,512=原版分组动作,1024=无SlotId' % result)

print('PASS: 未绑定/残留 carrier/分组/原版槽位查找全部通过')
