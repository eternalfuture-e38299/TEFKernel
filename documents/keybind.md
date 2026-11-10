# ⌨️ PC 原生键位绑定（keybind_manager）

把 Mod 自定义按键注入 Terraria PC 原生「设置 → 控制」菜单。玩家可以像原版键位一样直接改键，
Mod 通过 `PlayerInput.Triggers` 读取当前绑定，不需要自己维护键码表。

> 仅桌面端（Windows / Linux / macOS）提供；Android 使用 `button_manager` 的触摸按键。

## API

头文件：`include/terraria/content/keybind_manager.h`

```c
typedef struct terraria_keybind_scope_t {
    const char *modloader_id;
    const char *mod_id;
} terraria_keybind_scope_t;

typedef void (*terraria_keybind_callback_t)(const terraria_keybind_scope_t *scope,
                                            const char *id, void *user);

typedef struct terraria_keybind_desc_t {
    terraria_keybind_scope_t scope;
    const char *id;           // Mod 内唯一：1-64 个 ASCII 字母/数字/_.-
    const char *name;         // 菜单显示名 + 全局唯一 trigger（为空则用 id）
    const char *default_key;  // 默认键名，如 "F6"；NULL/"" 表示未绑定
    terraria_keybind_callback_t on_press;
    terraria_keybind_callback_t on_release;
    void *user;
    bool enabled;
} terraria_keybind_desc_t;
```

| 函数 | 说明 |
|---|---|
| `terraria_keybind_manager_available()` | 是否可用 |
| `terraria_keybind_manager_error()` | 失败原因 |
| `terraria_keybind_manager_install()` | 安装钩子（内核桌面初始化时自动调用，幂等） |
| `terraria_keybind_manager_shutdown()` | 卸载 |
| `terraria_keybind_manager_register(desc)` | 注册键位并注入游戏菜单 |
| `terraria_keybind_manager_unregister(scope, id)` | 注销 |
| `terraria_keybind_manager_unregister_owner(scope)` | 注销某个 Mod 的全部键位 |
| `terraria_keybind_manager_down(scope, id)` | 当前是否按住 |
| `terraria_keybind_manager_pressed(scope, id)` | 本帧是否刚按下 |
| `terraria_keybind_manager_released(scope, id)` | 本帧是否刚松开 |
| `terraria_keybind_manager_set_enabled(scope, id, bool)` | 启停回调 |

## 注入原理

1. `PlayerInput.Initialize` / `Load` 前后，把 `name` 加入 `PlayerInput.KnownTriggers`；
2. 往当前 `PlayerInputProfile.InputModes[Keyboard / KeyboardUI].KeyStatus` 写入默认键列表；
3. `UIManageControls.CreateBindingGroup` 前置把 `name` 追加到原生按键菜单的绑定列表；
4. `Main.Update` 后置读取 `PlayerInput.Triggers.Current / JustPressed / JustReleased` 派发回调；
5. 聊天输入或 `PlayerInput.WritingText` 为真时抑制回调，避免打字误触。

玩家在菜单里改键后，`KeyStatus` 更新，`PlayerInput.Triggers` 自动反映新键，Mod 无需重新注册。

## 测试模块

`test/keybind_test.c`：

- 注册「按键测试」，默认 `F6`；
- 按下后调用 `Main.NewText(string, byte, byte, byte)` 在聊天栏输出；
- 编译：

```bash
gcc -shared -fPIC -I include test/keybind_test.c -o libkeybind_test.so
```

> `name` 是全局 trigger，多个 Mod 同时使用相同名称会注册失败；建议带 Mod 名前缀。

