#include "bridge_symbols.h"
#include "lauxlib.h"
#include "lualib.h"
#include "support_api.h"
#include "terraria/item_slot.h"
#include "terraria/recipe_query.h"
#include "terraria/world_entity.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
void support_fixture_init(void);
void support_fixture_cleanup(void);
void support_fixture_tick(void);
void support_fixture_menu(bool menu);
void support_fixture_world(void *value);
void support_fixture_recipe_result(int id);
int support_fixture_roots(void);
int support_fixture_interactions(void);
static void script(lua_State *L, const char *s) {
    if (luaL_dostring(L, s) != LUA_OK) {
        fprintf(stderr, "Lua test error: %s\n", lua_tostring(L, -1));
        abort();
    }
}
int main(void) {
    bridge_init();
    support_fixture_init();
    char folder[] = "/tmp/tef-support-lua-XXXXXX";
    assert(mkdtemp(folder));
    char path[512];
    snprintf(path, sizeof(path), "%s/world.wld", folder);
    assert(terraria_world_entity_load(path));
    lua_State *L = luaL_newstate();
    assert(L);
    luaL_openlibs(L);
    lua_newtable(L);
    lua_newtable(L);
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "existing");
    lua_setfield(L, -2, "recipe");
    lua_pushliteral(L, "test.mod");
    lua_setfield(L, -2, "id");
    lualoader_support_register(L);
    lua_setglobal(L, "mod");
    script(L, "local s=mod.support_available();assert(s.world and s.slots and s.recipes)\n"
              "assert(mod.recipe.existing)\n"
              "assert(mod.world.active())\n"
              "assert(mod.world.put('machine',10,20,'abc'..string.char(0)..'z'))\n"
              "assert(mod.world.get('machine',10,20)=='abc'..string.char(0)..'z')\n"
              "assert(mod.world.get('machine',11,20)==nil)\n"
              "assert(not mod.world.put('../invalid',1,1,'x'))\n"
              "local list,total=mod.world.list(0,10);assert(total==1 and list[1].x==10)\n"
              "assert(not pcall(mod.world.put,'a',-1,0,'x'))\n"
              "assert(not pcall(mod.world.put,'a',1,1,string.rep('x',65537)))\n"
              "a=mod.slots.create(2);b=mod.slots.create(1);assert(a:size()==2)\n"
              "local cursor=mod.slots.query_cursor();assert(cursor.type==42 and cursor.stack==8)\n"
              "assert(mod.slots.query_cursor().stack==8)\n"
              "assert(a:interact(1,0));assert(a:read(1).type==42 and a:read(1).prefix==3)\n"
              "assert(mod.slots.query_cursor().type==0)\n"
              "assert(a:swap(1,b,1));assert(a:read(1).type==0 and b:read(1).type==42)\n"
              "assert(a:read(3)==nil);assert(not a:swap(3,b,1))\n"
              "assert(not pcall(mod.slots.create,257))\n"
              "a:close();a:close();assert(not pcall(a.read,a,1))\n"
              "local indices,total=mod.recipe.find(100,false,0,1);assert(total==2 and #indices==1 "
              "and indices[1]==0)\n"
              "local rest,total=mod.recipe.find(100,false,1,128);assert(total==2 and #rest==1 and "
              "rest[1]==2)\n"
              "local none,total=mod.recipe.find(100,false,3,10);assert(total==2 and #none==0)\n"
              "local uses,total=mod.recipe.find(5,true,0,128);assert(total==2 and #uses==2)\n"
              "local r=mod.recipe.get(0);assert(r.result.type==100 and r.materials[1].type==5 and "
              "r.stations[1]==13 and r.conditions==1)\n"
              "assert(mod.recipe.get(3)==nil)\n"
              "ticks=0;assert(mod.world.on_update(function()ticks=ticks+1 end))\n");
    assert(support_fixture_roots() == 1);
    assert(support_fixture_interactions() == 1);
    support_fixture_tick();
    script(L, "assert(ticks==1)");
    support_fixture_menu(true);
    support_fixture_tick();
    script(L, "assert(ticks==1);assert(not "
              "mod.world.active());assert(mod.world.get('machine',10,20)==nil);assert(not "
              "mod.world.put('machine',10,20,'wrong-world'))");
    support_fixture_menu(false);
    support_fixture_world((void *)51);
    script(L, "assert(not mod.world.active());assert(mod.world.get('machine',10,20)==nil)");
    support_fixture_world((void *)50);
    script(L, "assert(mod.world.active())");
    support_fixture_recipe_result(101);
    script(L, "assert(mod.recipe.get(0).result.type==100)");
    terraria_recipe_query_invalidate();
    script(L, "assert(mod.recipe.get(0).result.type==101)");
    script(L, "assert(mod.world.on_update(function()error('expected update failure')end))");
    support_fixture_tick();
    script(L, "assert(string.find(mod.world.update_error(),'expected update failure'))");
    support_fixture_tick();
    /* Unloading must remove native pointers before lua_close. */
    script(L, "assert(mod.world.on_update(function()ticks=ticks+1 end))");
    lualoader_support_close(L);
    assert(support_fixture_roots() == 0);
    support_fixture_tick();
    script(L, "assert(ticks==1);assert(not pcall(mod.world.active))");
    lua_close(L);
    support_fixture_tick();
    support_fixture_cleanup();
    assert(terraria_world_entity_load(NULL));
    assert(!rmdir(folder));
    puts("PASS: real Lua bindings, slot GC/transfer/cursor reference, recipe cache/pagination, "
         "callback failure/unload");
}
