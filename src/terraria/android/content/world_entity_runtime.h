#ifndef TEFKERNEL_WORLD_ENTITY_RUNTIME_H
#define TEFKERNEL_WORLD_ENTITY_RUNTIME_H
#include <stdbool.h>
/* Private scene guard. File saves may continue after the game returns to menus;
 * Lua/native consumers must not access the previous world's records there. */
void tef_world_entity_runtime_bind(bool loaded);
bool tef_world_entity_runtime_accessible(void);
#endif
