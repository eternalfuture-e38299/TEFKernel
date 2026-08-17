# 📚 TEFSTD Standard Library User Guide

TEFSTD is TEFKernel's cross-platform standard library, providing common data structures and algorithm implementations. All header files are licensed under the **MIT License** and can be freely integrated into your projects.

---

## 📑 Table of Contents

<details>
<summary><b>📖 Click to expand full table of contents</b></summary>

- [📦 Vector - Dynamic Array](#-vector---dynamic-array)
  - [Structure Definition](#structure-definition)
  - [Initialization and Destruction](#initialization-and-destruction)
  - [Element Operations](#element-operations)
  - [Capacity Management](#capacity-management)
  - [Complete Example](#vector-complete-example)

- [🗺️ HashMap - Hash Table](#-hashmap---hash-table)
  - [Structure Definition](#structure-definition-1)
  - [Initialization and Destruction](#initialization-and-destruction-1)
  - [Insertion and Lookup](#insertion-and-lookup)
  - [Deletion and Checking](#deletion-and-checking)
  - [Iterator](#iterator)
  - [Complete Example](#hashmap-complete-example)

- [⛷️ SkipMap - Skip List Map](#-skipmap---skip-list-map)
  - [Structure Definition](#structure-definition-2)
  - [Initialization and Destruction](#initialization-and-destruction-2)
  - [Insertion and Lookup](#insertion-and-lookup-1)
  - [Range Query](#range-query)
  - [Complete Example](#skipmap-complete-example)

- [🔧 Hash Utility Functions](#-hash-utility-functions)

- [⚡ Performance Comparison](#-performance-comparison)

</details>

---

## 📦 Vector - Dynamic Array

Vector is an auto-resizing contiguous memory container, similar to C++'s `std::vector`.

### Structure Definition

```c
typedef struct {
    void *data;        ///< Pointer to the start address of the element array (contiguous memory)
    size_t size;       ///< Number of valid elements currently stored
    size_t capacity;   ///< Currently allocated capacity (in number of elements)
    size_t elem_size;  ///< Size of a single element (in bytes)
} tefstd_vector_t;
```

### Initialization and Destruction

```c
// Initialize an empty vector
bool tefstd_vector_init(tefstd_vector_t *vec, size_t elem_size);

// Initialize from an existing array
bool tefstd_vector_init_from_array(tefstd_vector_t *vec, size_t elem_size, 
                                   void* array, size_t array_length);

// Destroy vector and free memory
void tefstd_vector_destroy(tefstd_vector_t *vec);
```

**Usage Example:**

```c
#include "tefstd/vector.h"

// Create an int vector
tefstd_vector_t int_vec;
if (!tefstd_vector_init(&int_vec, sizeof(int))) {
    printf("Failed to initialize vector\n");
    return;
}

// Initialize from array
int arr[] = {1, 2, 3, 4, 5};
tefstd_vector_t arr_vec;
tefstd_vector_init_from_array(&arr_vec, sizeof(int), arr, 5);

// Destroy after use
tefstd_vector_destroy(&int_vec);
tefstd_vector_destroy(&arr_vec);
```

### Element Operations

```c
// Append element
bool tefstd_vector_push_back(tefstd_vector_t *vec, const void *elem);

// Pop last element
bool tefstd_vector_pop_back(tefstd_vector_t *vec, void *out_elem);

// Get pointer to element at specified index
void* tefstd_vector_at(const tefstd_vector_t *vec, size_t index);

// Erase element at specified index
bool tefstd_vector_erase(tefstd_vector_t *vec, size_t index, void *out_elem);

// Remove all matching values (byte-wise comparison)
bool tefstd_vector_remove_value(tefstd_vector_t *vec, const void *value);

// Clear all elements (does not free memory)
void tefstd_vector_clear(tefstd_vector_t *vec);
```

**Usage Example:**

```c
tefstd_vector_t vec;
tefstd_vector_init(&vec, sizeof(int));

// Add elements
int val;
for (int i = 0; i < 10; i++) {
    val = i * 10;
    tefstd_vector_push_back(&vec, &val);
}

// Iterate elements
for (size_t i = 0; i < tefstd_vector_size(&vec); i++) {
    int* ptr = (int*)tefstd_vector_at(&vec, i);
    printf("vec[%zu] = %d\n", i, *ptr);
}

// Erase element at index 3
tefstd_vector_erase(&vec, 3, NULL);

// Remove all elements with value 50
int target = 50;
tefstd_vector_remove_value(&vec, &target);

// Pop last element
int last;
tefstd_vector_pop_back(&vec, &last);
printf("Last element: %d\n", last);

// Clear
tefstd_vector_clear(&vec);

tefstd_vector_destroy(&vec);
```

### Capacity Management

```c
// Get number of elements
size_t tefstd_vector_size(const tefstd_vector_t *vec);

// Get current capacity
size_t tefstd_vector_capacity(const tefstd_vector_t *vec);

// Reserve capacity (avoid frequent realloc)
bool tefstd_vector_reserve(tefstd_vector_t *vec, size_t new_cap);
```

**Usage Example:**

```c
tefstd_vector_t vec;
tefstd_vector_init(&vec, sizeof(int));

printf("Initial size: %zu, capacity: %zu\n", 
       tefstd_vector_size(&vec), 
       tefstd_vector_capacity(&vec));

// Reserve space for 100 elements
tefstd_vector_reserve(&vec, 100);
printf("After reserve: capacity = %zu\n", tefstd_vector_capacity(&vec));

// Adding elements won't trigger frequent realloc
for (int i = 0; i < 100; i++) {
    tefstd_vector_push_back(&vec, &i);
}

tefstd_vector_destroy(&vec);
```

### Vector Complete Example

```c
#include <stdio.h>
#include "tefstd/vector.h"

typedef struct {
    char name[32];
    int health;
    int mana;
} Player;

int main() {
    // Create vector of Player type
    tefstd_vector_t players;
    tefstd_vector_init(&players, sizeof(Player));
    
    // Add players
    Player p1 = {"Hero", 100, 50};
    Player p2 = {"Mage", 80, 120};
    Player p3 = {"Rogue", 90, 30};
    
    tefstd_vector_push_back(&players, &p1);
    tefstd_vector_push_back(&players, &p2);
    tefstd_vector_push_back(&players, &p3);
    
    // Iterate all players
    printf("=== All Players ===\n");
    for (size_t i = 0; i < tefstd_vector_size(&players); i++) {
        Player* p = (Player*)tefstd_vector_at(&players, i);
        printf("Name: %s, HP: %d, MP: %d\n", p->name, p->health, p->mana);
    }
    
    // Remove second player (index 1)
    Player removed;
    tefstd_vector_erase(&players, 1, &removed);
    printf("\nRemoved: %s\n", removed.name);
    
    // Iterate again
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

## 🗺️ HashMap - Hash Table

HashMap is implemented using open addressing, providing average O(1) complexity for lookup, insertion, and deletion operations.

### Structure Definition

```c
typedef struct {
    void *keys;        ///< Key array (contiguous memory)
    void *values;      ///< Value array (contiguous memory)
    uint8_t *states;   ///< State array: 0=empty, 1=occupied, 2=deleted (tombstone)
    size_t size;       ///< Number of valid elements currently stored
    size_t capacity;   ///< Current capacity (always a power of 2)
    size_t key_size;   ///< Size of key (in bytes)
    size_t value_size; ///< Size of value (in bytes)
} tefstd_hashmap_t;

typedef struct {
    const tefstd_hashmap_t *map;
    size_t index;
} tefstd_hashmap_iter_t;
```

### Initialization and Destruction

```c
// Initialize hash table
bool tefstd_hashmap_init(tefstd_hashmap_t* map, 
                         size_t key_size, 
                         size_t value_size);

// Destroy hash table
void tefstd_hashmap_free(tefstd_hashmap_t* map);
```

### Insertion and Lookup

```c
// Insert or update key-value pair
bool tefstd_hashmap_put(tefstd_hashmap_t* map, 
                        const void* key, 
                        const void* value);

// Look up value by key
void* tefstd_hashmap_get(tefstd_hashmap_t* map, const void* key);

// Check if key exists (faster)
bool tefstd_hashmap_has(const tefstd_hashmap_t* map, const void* key);

// Get number of elements
size_t tefstd_hashmap_len(const tefstd_hashmap_t* map);

// Clear hash table
void tefstd_hashmap_clear(tefstd_hashmap_t* map);
```

### Deletion and Checking

```c
// Delete specified key-value pair
bool tefstd_hashmap_del(tefstd_hashmap_t* map, const void* key);
```

### Iterator

```c
// Create iterator
tefstd_hashmap_iter_t tefstd_hashmap_iter(const tefstd_hashmap_t* map);

// Get next key-value pair
bool tefstd_hashmap_next(tefstd_hashmap_iter_t* iter,
                         void* key_out,
                         void* value_out);
```

### HashMap Complete Example

```c
#include <stdio.h>
#include <string.h>
#include "tefstd/hashmap.h"

int main() {
    // Create HashMap: key is string (char*), value is integer (int)
    tefstd_hashmap_t map;
    tefstd_hashmap_init(&map, sizeof(char*), sizeof(int));
    
    // Insert key-value pairs
    const char* key1 = "apple";
    const char* key2 = "banana";
    const char* key3 = "orange";
    int val1 = 100;
    int val2 = 200;
    int val3 = 300;
    
    tefstd_hashmap_put(&map, &key1, &val1);
    tefstd_hashmap_put(&map, &key2, &val2);
    tefstd_hashmap_put(&map, &key3, &val3);
    
    // Look up value
    int* found = (int*)tefstd_hashmap_get(&map, &key2);
    if (found) {
        printf("banana = %d\n", *found);
    }
    
    // Update value
    int new_val = 250;
    tefstd_hashmap_put(&map, &key2, &new_val);
    
    // Look up again
    found = (int*)tefstd_hashmap_get(&map, &key2);
    printf("banana updated = %d\n", *found);
    
    // Check if key exists
    const char* key4 = "grape";
    if (tefstd_hashmap_has(&map, &key4)) {
        printf("grape exists!\n");
    } else {
        printf("grape not found\n");
    }
    
    // Iterate all key-value pairs
    printf("\n=== All Entries ===\n");
    tefstd_hashmap_iter_t iter = tefstd_hashmap_iter(&map);
    char* k;
    int v;
    while (tefstd_hashmap_next(&iter, &k, &v)) {
        printf("%s = %d\n", k, v);
    }
    
    // Delete a key
    tefstd_hashmap_del(&map, &key1);
    printf("\nAfter deleting apple, size = %zu\n", tefstd_hashmap_len(&map));
    
    tefstd_hashmap_free(&map);
    return 0;
}
```

### HashMap Storing Complex Structures Example

```c
#include <stdio.h>
#include <string.h>
#include "tefstd/hashmap.h"

typedef struct {
    int x;
    int y;
    int z;
} Vector3;

// Custom hash function (using tefstd_hash_mem)
uint64_t hash_vector3(const Vector3* v) {
    return tefstd_hash_mem(v, sizeof(Vector3));
}

int main() {
    // Key is string, value is Vector3 structure
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
    
    // Look up
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

## ⛷️ SkipMap - Skip List Map

SkipMap is an ordered key-value map implemented using a skip list. All elements are sorted by key and support range queries.

### Structure Definition

```c
typedef struct skipnode {
    void *key;
    void *value;
    struct skipnode **forward;  ///< Multi-level forward pointer array
} tefstd_skipnode_t;

typedef struct {
    tefstd_skipnode_t *header;  ///< Header node (sentinel)
    size_t size;                ///< Current number of elements
    size_t key_size;            ///< Size of key
    size_t value_size;          ///< Size of value
    int level;                  ///< Current maximum level
} tefstd_skipmap_t;

typedef struct {
    tefstd_skipmap_t *map;
    tefstd_skipnode_t *current;
    void *end_key;              ///< Range end key (NULL means no upper bound)
    bool inclusive;             ///< Whether to include the end key
} skipmap_iter_t;
```

### Initialization and Destruction

```c
// Initialize skip list
bool tefstd_skipmap_init(tefstd_skipmap_t *map, 
                         size_t key_size, 
                         size_t value_size);

// Destroy skip list
void tefstd_skipmap_free(tefstd_skipmap_t *map);
```

### Insertion and Lookup

```c
// Insert or update key-value pair
bool tefstd_skipmap_put(tefstd_skipmap_t *map, 
                        const void *key, 
                        const void *value);

// Look up value by key
void* tefstd_skipmap_get(tefstd_skipmap_t *map, const void *key);

// Delete key-value pair
bool tefstd_skipmap_del(tefstd_skipmap_t *map, const void *key);

// Get value for minimum key
void* tefstd_skipmap_min(tefstd_skipmap_t *map);

// Get value for maximum key
void* tefstd_skipmap_max(tefstd_skipmap_t *map);
```

### Range Query

```c
// Create range query iterator
skipmap_iter_t tefstd_skipmap_range(tefstd_skipmap_t *map, 
                                    const void *start, 
                                    const void *end, 
                                    bool inclusive);

// Get next key-value pair
bool tefstd_skipmap_next(skipmap_iter_t *iter, 
                         void *key_out, 
                         void *value_out);
```

### SkipMap Complete Example

```c
#include <stdio.h>
#include "tefstd/skipmap.h"

int main() {
    // Create skip list: key is integer, value is string
    tefstd_skipmap_t map;
    tefstd_skipmap_init(&map, sizeof(int), sizeof(char*));
    
    // Insert data (keys will be automatically sorted)
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
    
    // Get minimum and maximum keys
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

### SkipMap Advanced Range Query Example

```c
#include <stdio.h>
#include "tefstd/skipmap.h"

// Query players by score range
typedef struct {
    char name[32];
    int score;
} PlayerScore;

int main() {
    tefstd_skipmap_t score_map;
    tefstd_skipmap_init(&score_map, sizeof(int), sizeof(PlayerScore));
    
    // Insert player data (sorted by score)
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
    
    // Query players with score between [1800, 2300]
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

## 🔧 Hash Utility Functions

```c
// Calculate string hash value
uint64_t tefstd_hash_str(const char* str);

// Calculate memory block hash value
uint64_t tefstd_hash_mem(const void* data, size_t len);
```

**Usage Example:**

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

## ⚡ Performance Comparison

| Data Structure | Insertion | Lookup   | Deletion | Ordered Traversal | Range Query | Use Case                    |
|:---------------|:----------|:---------|:---------|:------------------|:------------|:----------------------------|
| **Vector**     | O(1)*     | O(1)     | O(n)     | ✅ O(n)           | ❌          | Random access, tail ops     |
| **HashMap**    | O(1)~     | O(1)~    | O(1)~    | ❌                | ❌          | Fast key-value lookup       |
| **SkipMap**    | O(log n)  | O(log n) | O(log n) | ✅ O(n)           | ✅ O(log n) | Ordered data, range queries |

> * Tail insertion average O(1), O(n) during reallocation
    > ~ Average O(1), worst-case O(n)

### Selection Guide

```c
// Need frequent random access → Use Vector
tefstd_vector_t vec;

// Need fast key-value lookup → Use HashMap
tefstd_hashmap_t map;

// Need ordered key-value pairs + range queries → Use SkipMap
tefstd_skipmap_t skipmap;
```

---

## 📝 Summary

| Feature                | Vector        | HashMap    | SkipMap     |
|:-----------------------|:--------------|:-----------|:------------|
| **API Simplicity**     | ⭐⭐⭐⭐⭐    | ⭐⭐⭐⭐   | ⭐⭐⭐⭐    |
| **Memory Efficiency**  | ⭐⭐⭐⭐⭐    | ⭐⭐⭐⭐   | ⭐⭐⭐      |
| **Lookup Performance** | ⭐⭐⭐        | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐    |
| **Ordered**            | ✅ (by index) | ❌         | ✅ (by key) |
| **Range Query**        | ❌            | ❌         | ✅          |

---

*Happy Coding! 🚀✨*