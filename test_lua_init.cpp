// Minimal test: just create a Lua state, nothing else
// Compile: g++ -std=c++17 -O1 -g -I external/lua-5.2.4 -I main -I main/ssz \
//   test_lua_init.cpp -L build/Debug -llua -o test_lua_init.exe

#include <stdio.h>
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

int main() {
    printf("Creating Lua state...\n");
    lua_State* L = luaL_newstate();
    if (!L) {
        printf("FAILED: luaL_newstate returned NULL\n");
        return 1;
    }
    printf("OK: Lua state created at %p\n", (void*)L);
    luaL_openlibs(L);
    printf("OK: Lua libs opened\n");
    lua_close(L);
    printf("OK: Lua state closed\n");
    return 0;
}
