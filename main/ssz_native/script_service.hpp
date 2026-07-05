// script_service.hpp — Native C++ implementation for ssz_script/ssz/script.ssz
//
// script.ssz (2216 lines) implements the SSZ script runtime API — argument
// parsing (numArg, strArg, blArg, refArg), subsystem wrappers (SFF, sound,
// font, command, video), key state query functions, game state getters/setters,
// and system-script init.
//
// Phase 5: Full implementation. All 190+ Lua-callable functions are implemented
// as lua_CFunction-compatible C functions that delegate to native service
// modules (common_service, sdlevent_service, sound_resource_service, etc.).
//
// Registration: script_init(lua_State* L) registers all functions with Lua
// via lua_register() so they're callable from Lua scripts.

#pragma once

#include <string>

// lua.hpp required for ref_arg<T>() template used across native services
#include <lua.hpp>

namespace ikemen::ssz_native {

// ── State ──
struct ScriptState {
	std::string line;  // inputText line buffer (matches SSZ `%char line;`)
};

// ── ref_arg — SSZ refArg pattern equivalent ──
//
// Extracts a typed C++ pointer from a Lua userdata by checking the expected
// metatable name. Matches the SSZ pattern:
//   ^&.sc.Sff sff = .sc.refArg!&.sc.Sff?(L=, re=, argc=, nret);
//
// Usage from native C++ service functions:
//   auto* sff = ref_arg<SffUD>(L, 1, "IKEMEN.Sff");
//   if (!sff) return 0;  // lua_error already called on type mismatch
//   sff->sff->doSomething();
//
// The Lua-callable version is registered as "refArg" in script_init().

template<typename T>
inline T* ref_arg(lua_State* L, int idx, const char* mt_name) {
	void* ud = luaL_checkudata(L, idx, mt_name);
	if (!ud) {
		lua_pushstring(L, "Expected a valid object (wrong type).");
		lua_error(L);
		return nullptr;
	}
	return static_cast<T*>(ud);
}

// ── Entry points ──

// init(L) — registers all 190+ Lua-callable functions on the given Lua state.
// Called from bridge.cpp ScriptInit() when IKEMEN_NATIVE_SCRIPT_LIB=1.
void script_init(lua_State* L);

// No-arg convenience wrapper — calls script_init on stored Lua state if set.
void script_init();

// Set the Lua state for script registration (called from bridge when available).
void script_set_lua_state(lua_State* L);

// ── Module-level state access ──
ScriptState& script_get_state();

} // namespace ikemen::ssz_native
