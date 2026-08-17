# 📚 TEFSTD 标准库使用指南

TEFSTD 是 TEFKernel 的跨平台标准库，提供了常用的数据结构和算法实现。所有头文件采用 **MIT 许可证**，可自由集成到您的项目中。

---

## 📑 目录

<details>
<summary><b>📖 点击展开完整目录</b></summary>

- [📦 Vector - 动态数组](#-vector---动态数组)
  - [结构体定义](#结构体定义)
  - [初始化与销毁](#初始化与销毁)
  - [元素操作](#元素操作)
  - [容量管理](#容量管理)
  - [完整示例](#vector-完整示例)

- [🗺️ HashMap - 哈希表](#-hashmap---哈希表)
  - [结构体定义](#结构体定义-1)
  - [初始化与销毁](#初始化与销毁-1)
  - [插入与查找](#插入与查找)
  - [删除与检查](#删除与检查)
  - [迭代器](#迭代器)
  - [完整示例](#hashmap-完整示例)

- [⛷️ SkipMap - 跳表映射](#-skipmap---跳表映射)
  - [结构体定义](#结构体定义-2)
  - [初始化与销毁](#初始化与销毁-2)
  - [插入与查找](#插入与查找-1)
  - [范围查询](#范围查询)
  - [完整示例](#skipmap-完整示例)

- [🔧 哈希工具函数](#-哈希工具函数)

- [⚡ 性能对比](#-性能对比)

</details>

---

## 📦 Vector - 动态数组

Vector 是一个自动扩容的连续内存容器，类似于 C++ 的 `std::vector`。

### 结构体定义

```c
typedef struct {
    void *data;        ///< 指向元素数组的起始地址（连续内存）
    size_t size;       ///< 当前存储的有效元素个数
    size_t capacity;   ///< 当前已分配的容量（以元素个数计）
    size_t elem_size;  ///< 单个元素的大小（字节）
} tefstd_vector_t;
```

### 初始化与销毁

```c
// 初始化一个空的 vector
bool tefstd_vector_init(tefstd_vector_t *vec, size_t elem_size);

// 从现有数组初始化
bool tefstd_vector_init_from_array(tefstd_vector_t *vec, size_t elem_size, 
                                   void* array, size_t array_length);

// 销毁 vector 并释放内存
void tefstd_vector_destroy(tefstd_vector_t *vec);
```

**使用示例：**

```c
#include "tefstd/vector.h"

// 创建一个 int 类型的 vector
tefstd_vector_t int_vec;
if (!tefstd_vector_init(&int_vec, sizeof(int))) {
    printf("Failed to initialize vector\n");
    return;
}

// 从数组初始化
int arr[] = {1, 2, 3, 4, 5};
tefstd_vector_t arr_vec;
tefstd_vector_init_from_array(&arr_vec, sizeof(int), arr, 5);

// 使用完后销毁
tefstd_vector_destroy(&int_vec);
tefstd_vector_destroy(&arr_vec);
```

### 元素操作

```c
// 追加元素
bool tefstd_vector_push_back(tefstd_vector_t *vec, const void *elem);

// 弹出最后一个元素
bool tefstd_vector_pop_back(tefstd_vector_t *vec, void *out_elem);

// 获取指定索引处的元素指针
void* tefstd_vector_at(const tefstd_vector_t *vec, size_t index);

// 删除指定索引处的元素
bool tefstd_vector_erase(tefstd_vector_t *vec, size_t index, void *out_elem);

// 移除所有匹配的值（按字节比较）
bool tefstd_vector_remove_value(tefstd_vector_t *vec, const void *value);

// 清空所有元素（不释放内存）
void tefstd_vector_clear(tefstd_vector_t *vec);
```

**使用示例：**

```c
tefstd_vector_t vec;
tefstd_vector_init(&vec, sizeof(int));

// 添加元素
int val;
for (int i = 0; i < 10; i++) {
    val = i * 10;
    tefstd_vector_push_back(&vec, &val);
}

// 遍历元素
for (size_t i = 0; i < tefstd_vector_size(&vec); i++) {
    int* ptr = (int*)tefstd_vector_at(&vec, i);
    printf("vec[%zu] = %d\n", i, *ptr);
}

// 删除索引 3 处的元素
tefstd_vector_erase(&vec, 3, NULL);

// 移除所有值为 50 的元素
int target = 50;
tefstd_vector_remove_value(&vec, &target);

// 弹出最后一个元素
int last;
tefstd_vector_pop_back(&vec, &last);
printf("Last element: %d\n", last);

// 清空
tefstd_vector_clear(&vec);

tefstd_vector_destroy(&vec);
```

### 容量管理

```c
// 获取元素数量
size_t tefstd_vector_size(const tefstd_vector_t *vec);

// 获取当前容量
size_t tefstd_vector_capacity(const tefstd_vector_t *vec);

// 预留容量（避免频繁 realloc）
bool tefstd_vector_reserve(tefstd_vector_t *vec, size_t new_cap);
```

**使用示例：**

```c
tefstd_vector_t vec;
tefstd_vector_init(&vec, sizeof(int));

printf("Initial size: %zu, capacity: %zu\n", 
       tefstd_vector_size(&vec), 
       tefstd_vector_capacity(&vec));

// 预留 100 个元素的空间
tefstd_vector_reserve(&vec, 100);
printf("After reserve: capacity = %zu\n", tefstd_vector_capacity(&vec));

// 添加元素不会触发频繁 realloc
for (int i = 0; i < 100; i++) {
    tefstd_vector_push_back(&vec, &i);
}

tefstd_vector_destroy(&vec);
```

### Vector 完整示例

```c
#include <stdio.h>
#include "tefstd/vector.h"

typedef struct {
    char name[32];
    int health;
    int mana;
} Player;

int main() {
    // 创建 Player 类型的 vector
    tefstd_vector_t players;
    tefstd_vector_init(&players, sizeof(Player));
    
    // 添加玩家
    Player p1 = {"Hero", 100, 50};
    Player p2 = {"Mage", 80, 120};
    Player p3 = {"Rogue", 90, 30};
    
    tefstd_vector_push_back(&players, &p1);
    tefstd_vector_push_back(&players, &p2);
    tefstd_vector_push_back(&players, &p3);
    
    // 遍历所有玩家
    printf("=== All Players ===\n");
    for (size_t i = 0; i < tefstd_vector_size(&players); i++) {
        Player* p = (Player*)tefstd_vector_at(&players, i);
        printf("Name: %s, HP: %d, MP: %d\n", p->name, p->health, p->mana);
    }
    
    // 移除第二个玩家（索引 1）
    Player removed;
    tefstd_vector_erase(&players, 1, &removed);
    printf("\nRemoved: %s\n", removed.name);
    
    // 再次遍历
    printf("\n=== Remaining Players ===\n");
    for (size_t i = 0; i < tefstd_vector_size(&players); i++) {
        Player* p = (Player*)tefstd_vector_at(&players, i);
        printf("Name: %s, HP: %d, MP: %d\n", p->name, p->health, p->mana);
    }
    
    tefstd_vector_destroy(&players);
    return 0;
}
```

---

## 🗺️ HashMap - 哈希表

HashMap 基于开放寻址法实现，提供平均 O(1) 复杂度的查找、插入和删除操作。

### 结构体定义

```c
typedef struct {
    void *keys;        ///< 键数组（连续内存）
    void *values;      ///< 值数组（连续内存）
    uint8_t *states;   ///< 状态数组：0=空, 1=占用, 2=删除（墓碑）
    size_t size;       ///< 当前有效元素个数
    size_t capacity;   ///< 当前容量（总是2的幂）
    size_t key_size;   ///< 键的大小（字节）
    size_t value_size; ///< 值的大小（字节）
} tefstd_hashmap_t;

typedef struct {
    const tefstd_hashmap_t *map;
    size_t index;
} tefstd_hashmap_iter_t;
```

### 初始化与销毁

```c
// 初始化哈希表
bool tefstd_hashmap_init(tefstd_hashmap_t* map, 
                         size_t key_size, 
                         size_t value_size);

// 销毁哈希表
void tefstd_hashmap_free(tefstd_hashmap_t* map);
```

### 插入与查找

```c
// 插入或更新键值对
bool tefstd_hashmap_put(tefstd_hashmap_t* map, 
                        const void* key, 
                        const void* value);

// 查找键对应的值
void* tefstd_hashmap_get(tefstd_hashmap_t* map, const void* key);

// 检查键是否存在（更快）
bool tefstd_hashmap_has(const tefstd_hashmap_t* map, const void* key);

// 获取元素数量
size_t tefstd_hashmap_len(const tefstd_hashmap_t* map);

// 清空哈希表
void tefstd_hashmap_clear(tefstd_hashmap_t* map);
```

### 删除与检查

```c
// 删除指定键值对
bool tefstd_hashmap_del(tefstd_hashmap_t* map, const void* key);
```

### 迭代器

```c
// 创建迭代器
tefstd_hashmap_iter_t tefstd_hashmap_iter(const tefstd_hashmap_t* map);

// 获取下一个键值对
bool tefstd_hashmap_next(tefstd_hashmap_iter_t* iter,
                         void* key_out,
                         void* value_out);
```

### HashMap 完整示例

```c
#include <stdio.h>
#include <string.h>
#include "tefstd/hashmap.h"

int main() {
    // 创建 HashMap: 键为字符串 (char*)，值为整数 (int)
    tefstd_hashmap_t map;
    tefstd_hashmap_init(&map, sizeof(char*), sizeof(int));
    
    // 插入键值对
    const char* key1 = "apple";
    const char* key2 = "banana";
    const char* key3 = "orange";
    int val1 = 100;
    int val2 = 200;
    int val3 = 300;
    
    tefstd_hashmap_put(&map, &key1, &val1);
    tefstd_hashmap_put(&map, &key2, &val2);
    tefstd_hashmap_put(&map, &key3, &val3);
    
    // 查找值
    int* found = (int*)tefstd_hashmap_get(&map, &key2);
    if (found) {
        printf("banana = %d\n", *found);
    }
    
    // 更新值
    int new_val = 250;
    tefstd_hashmap_put(&map, &key2, &new_val);
    
    // 再次查找
    found = (int*)tefstd_hashmap_get(&map, &key2);
    printf("banana updated = %d\n", *found);
    
    // 检查键是否存在
    const char* key4 = "grape";
    if (tefstd_hashmap_has(&map, &key4)) {
        printf("grape exists!\n");
    } else {
        printf("grape not found\n");
    }
    
    // 遍历所有键值对
    printf("\n=== All Entries ===\n");
    tefstd_hashmap_iter_t iter = tefstd_hashmap_iter(&map);
    char* k;
    int v;
    while (tefstd_hashmap_next(&iter, &k, &v)) {
        printf("%s = %d\n", k, v);
    }
    
    // 删除一个键
    tefstd_hashmap_del(&map, &key1);
    printf("\nAfter deleting apple, size = %zu\n", tefstd_hashmap_len(&map));
    
    tefstd_hashmap_free(&map);
    return 0;
}
```

### HashMap 存储复杂结构体示例

```c
#include <stdio.h>
#include <string.h>
#include "tefstd/hashmap.h"

typedef struct {
    int x;
    int y;
    int z;
} Vector3;

// 自定义哈希函数（使用 tefstd_hash_mem）
uint64_t hash_vector3(const Vector3* v) {
    return tefstd_hash_mem(v, sizeof(Vector3));
}

int main() {
    // 键为字符串，值为 Vector3 结构体
    tefstd_hashmap_t map;
    tefstd_hashmap_init(&map, sizeof(char*), sizeof(Vector3));
    
    const char* keys[] = {"pos1", "pos2", "pos3"};
    Vector3 positions[] = {
        {10, 20, 30},
        {40, 50, 60},
        {70, 80, 90}
    };
    
    for (int i = 0; i < 3; i++) {
        tefstd_hashmap_put(&map, &keys[i], &positions[i]);
    }
    
    // 查找
    const char* target = "pos2";
    Vector3* found = (Vector3*)tefstd_hashmap_get(&map, &target);
    if (found) {
        printf("pos2 = (%d, %d, %d)\n", found->x, found->y, found->z);
    }
    
    tefstd_hashmap_free(&map);
    return 0;
}
```

---

## ⛷️ SkipMap - 跳表映射

SkipMap 是基于跳表实现的有序键值映射，所有元素按键排序，支持范围查询。

### 结构体定义

```c
typedef struct skipnode {
    void *key;
    void *value;
    struct skipnode **forward;  ///< 多层前向指针数组
} tefstd_skipnode_t;

typedef struct {
    tefstd_skipnode_t *header;  ///< 头节点（哨兵）
    size_t size;                ///< 当前元素个数
    size_t key_size;            ///< 键的大小
    size_t value_size;          ///< 值的大小
    int level;                  ///< 当前最大层数
} tefstd_skipmap_t;

typedef struct {
    tefstd_skipmap_t *map;
    tefstd_skipnode_t *current;
    void *end_key;              ///< 范围结束键（NULL 表示无上限）
    bool inclusive;             ///< 是否包含结束键
} skipmap_iter_t;
```

### 初始化与销毁

```c
// 初始化跳表
bool tefstd_skipmap_init(tefstd_skipmap_t *map, 
                         size_t key_size, 
                         size_t value_size);

// 销毁跳表
void tefstd_skipmap_free(tefstd_skipmap_t *map);
```

### 插入与查找

```c
// 插入或更新键值对
bool tefstd_skipmap_put(tefstd_skipmap_t *map, 
                        const void *key, 
                        const void *value);

// 查找键对应的值
void* tefstd_skipmap_get(tefstd_skipmap_t *map, const void *key);

// 删除键值对
bool tefstd_skipmap_del(tefstd_skipmap_t *map, const void *key);

// 获取最小键对应的值
void* tefstd_skipmap_min(tefstd_skipmap_t *map);

// 获取最大键对应的值
void* tefstd_skipmap_max(tefstd_skipmap_t *map);
```

### 范围查询

```c
// 创建范围查询迭代器
skipmap_iter_t tefstd_skipmap_range(tefstd_skipmap_t *map, 
                                    const void *start, 
                                    const void *end, 
                                    bool inclusive);

// 获取下一个键值对
bool tefstd_skipmap_next(skipmap_iter_t *iter, 
                         void *key_out, 
                         void *value_out);
```

### SkipMap 完整示例

```c
#include <stdio.h>
#include "tefstd/skipmap.h"

int main() {
    // 创建跳表: 键为整数，值为字符串
    tefstd_skipmap_t map;
    tefstd_skipmap_init(&map, sizeof(int), sizeof(char*));
    
    // 插入数据（键会自动排序）
    int keys[] = {50, 10, 30, 20, 40};
    const char* values[] = {"fifty", "ten", "thirty", "twenty", "forty"};
    
    for (int i = 0; i < 5; i++) {
        tefstd_skipmap_put(&map, &keys[i], &values[i]);
    }
    
    printf("=== All Entries (sorted by key) ===\n");
    skipmap_iter_t iter = tefstd_skipmap_range(&map, NULL, NULL, false);
    int k;
    char* v;
    while (tefstd_skipmap_next(&iter, &k, &v)) {
        printf("%d -> %s\n", k, v);
    }
    
    printf("\n=== Range Query: [20, 40] ===\n");
    int start = 20;
    int end = 40;
    iter = tefstd_skipmap_range(&map, &start, &end, true);
    while (tefstd_skipmap_next(&iter, &k, &v)) {
        printf("%d -> %s\n", k, v);
    }
    
    // 获取最小和最大键
    char** min_val = (char**)tefstd_skipmap_min(&map);
    char** max_val = (char**)tefstd_skipmap_max(&map);
    if (min_val && max_val) {
        printf("\nMin: %d, Max: %d\n", 
               *(int*)tefstd_skipmap_get(&map, &keys[0]), 
               *(int*)tefstd_skipmap_get(&map, &keys[4]));
    }
    
    tefstd_skipmap_free(&map);
    return 0;
}
```

### SkipMap 范围查询高级示例

```c
#include <stdio.h>
#include "tefstd/skipmap.h"

// 按分数范围查询玩家
typedef struct {
    char name[32];
    int score;
} PlayerScore;

int main() {
    tefstd_skipmap_t score_map;
    tefstd_skipmap_init(&score_map, sizeof(int), sizeof(PlayerScore));
    
    // 插入玩家数据（按分数排序）
    PlayerScore players[] = {
        {"Alice", 1500},
        {"Bob", 2300},
        {"Charlie", 1800},
        {"David", 1200},
        {"Eve", 2700},
        {"Frank", 2000}
    };
    
    for (int i = 0; i < 6; i++) {
        tefstd_skipmap_put(&score_map, &players[i].score, &players[i]);
    }
    
    // 查询分数在 [1800, 2300] 之间的玩家
    printf("=== Players with score between 1800 and 2300 ===\n");
    int min_score = 1800;
    int max_score = 2300;
    skipmap_iter_t iter = tefstd_skipmap_range(&score_map, &min_score, &max_score, true);
    int score;
    PlayerScore player;
    while (tefstd_skipmap_next(&iter, &score, &player)) {
        printf("%s: %d\n", player.name, player.score);
    }
    
    tefstd_skipmap_free(&score_map);
    return 0;
}
```

---

## 🔧 哈希工具函数

```c
// 计算字符串哈希值
uint64_t tefstd_hash_str(const char* str);

// 计算内存块哈希值
uint64_t tefstd_hash_mem(const void* data, size_t len);
```

**使用示例：**

```c
#include "tefstd/hashmap.h"
#include <stdio.h>

int main() {
    const char* text = "Hello World";
    uint64_t hash1 = tefstd_hash_str(text);
    uint64_t hash2 = tefstd_hash_mem(text, strlen(text));
    
    printf("Hash (string): %016llX\n", hash1);
    printf("Hash (memory): %016llX\n", hash2);
    
    return 0;
}
```

---

## ⚡ 性能对比

| 数据结构    | 插入     | 查找     | 删除     | 有序遍历 | 范围查询    | 适用场景           |
|:------------|:---------|:---------|:---------|:---------|:------------|:-------------------|
| **Vector**  | O(1)*    | O(1)     | O(n)     | ✅ O(n)  | ❌          | 随机访问，尾部操作 |
| **HashMap** | O(1)~    | O(1)~    | O(1)~    | ❌       | ❌          | 快速键值查找       |
| **SkipMap** | O(log n) | O(log n) | O(log n) | ✅ O(n)  | ✅ O(log n) | 有序数据，范围查询 |

> * 尾部插入平均 O(1)，扩容时 O(n)
    > ~ 平均 O(1)，最坏 O(n)

### 选择建议

```c
// 需要频繁访问任意位置 → 用 Vector
tefstd_vector_t vec;

// 需要快速键值查找 → 用 HashMap
tefstd_hashmap_t map;

// 需要有序键值对 + 范围查询 → 用 SkipMap
tefstd_skipmap_t skipmap;
```

---

## 📝 总结

| 特性           | Vector      | HashMap    | SkipMap   |
|:---------------|:------------|:-----------|:----------|
| **API 简洁度** | ⭐⭐⭐⭐⭐  | ⭐⭐⭐⭐   | ⭐⭐⭐⭐  |
| **内存效率**   | ⭐⭐⭐⭐⭐  | ⭐⭐⭐⭐   | ⭐⭐⭐    |
| **查找性能**   | ⭐⭐⭐      | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐  |
| **有序性**     | ✅ (按索引) | ❌         | ✅ (按键) |
| **范围查询**   | ❌          | ❌         | ✅        |

---

*Happy Coding! 🚀✨*