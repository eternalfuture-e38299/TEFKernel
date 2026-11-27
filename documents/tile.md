# 自定义图格：多格 / 交互 / 物品槽位

`include/terraria/content/tile_manager.h` 里的 `terraria_tile_handle_t` 同时适用于
PC（Mono）与 Android（il2cpp），下面三组能力都是平台无关的。

## 1. 多格图格（TileObjectData 几何）

原有的 `object_style` 预设（1x1 / 1x2 / 2x2 / 3x3 …）继续可用；需要任意尺寸时填写：

```c
terraria_tile_handle_t bed = {
    .parent_modloader_id = "my.loader",
    .parent_id = "my.mod",
    .internal_name = "double_bed",
    .runtime_id = -1,
    .frame_important = true,

    .tile_width = 4,            /* 0 = 用 object_style 预设 */
    .tile_height = 2,
    .origin_x = 0, .origin_y = 0,

    .coordinate_width = 16,     /* 0 = 16 */
    .coordinate_padding = 0,    /* 0 = 2（家具通用）；负数 = 显式 0 */
    /* .coordinate_heights = (const int[]){16,16}, .coordinate_height_count = 2, */

    .style_wrap_limit = 0, .style_multiplier = 0, .style_line_skip = 0,
    .style_horizontal = true,
    .draw_y_offset = 0,
    .anchor_wall = false,       /* true = 不需要下方支撑 */
};
terraria_tile_manager_register_tile(&bed);
```

TEFKernel 会：

- 复制一个合适的 `TileObjectData` 模板并覆盖 `Width / Height / Origin /
  CoordinateHeights / CoordinateWidth / CoordinatePadding / Style* / DrawYOffset`；
- 自动把 `Main.tileFrameImportant` 置 true；
- 在 `WorldGen.TileFrameImportant` 后校验整个结构：缺格或下方无支撑时整块拆掉
  （掉落物只从最左上角产出）。

已知限制：暂未暴露 `AnchorTop/Left/Right`、alternate anchors、门/床等特殊模块。

## 2. 交互图格

```c
static bool on_interact(terraria_tile_handle_t *t, int x, int y, int player) {
    /* 返回 true = 已处理，本次右键不会再交给原版 */
    return true;
}

lamp.interactable = true;
lamp.tile_ops.on_interact = on_interact;
```

TEFKernel 会挂 `Player.TileInteractionsUse` 前缀，并按原版的
`tileInteractAttempted && releaseUseTile` 条件过滤（避免每帧重复触发），
处理后会吞掉这次使用键。

## 3. 物品槽位（容器）

```c
chest.container = true;
chest.container_slots = 8;      /* 1..40，0 = 40 */
```

- 放下容器图格时自动创建 `Chest`（`TileObject.Place` 后置）；
- 右键打开原版箱子界面（`Player.OpenChest`）；
- 有物品时无法挖掉（和原版箱子一致），空箱子破坏时自动移除 Chest 记录；
- 自动设置 `Main.tileContainer` 与 `TileID.Sets.IsAContainer`；
- 箱子数据由原版存档系统保存/读取。

槽位读写：

```c
int item = 0, stack = 0, prefix = 0;
terraria_tile_container_get_slot(&chest, x, y, 0, &item, &stack, &prefix);
terraria_tile_container_set_slot(&chest, x, y, 0, ItemID.Torch, 10, 0);
terraria_tile_container_clear(&chest, x, y);

int idx = terraria_tile_container_index(&chest, x, y); /* -1 = 无 */
int n = terraria_tile_container_slot_count(&chest);
int ox, oy;
terraria_tile_manager_get_origin(&chest, x, y, &ox, &oy);
```

PC 用 `Item[]` 槽位，Android 用 `ChestItem[]`（值类型结构），差异已在内核内部处理。

> 注意：世界尚未加载时（`Main.tile` 为 null）槽位接口会安全返回 false。
