#pragma once
#include "../alpha/lua.hpp"

struct lua_State;

namespace ikemen {

struct SystemScript {
	void init(LuaState& L) {}
};

void registerSystemScriptCallbacks(lua_State* L);

} // namespace ikemen
