// system_script_service.hpp — Native C++ scaffolding for ssz_script/ssz/system-script.ssz
//
// system-script.ssz (2403 lines) implements the system-level Lua bridge —
// game initialization, match flow, rendering, audio, select screen, pause
// menu, results screen, and the main game loop.
//
// 200+ Lua-callable functions following the pattern:
//   void func(lua_State* L, int& re)
//
// Phase 3: Minimalist scaffolding. Individual callback stubs deferred.

#pragma once

struct lua_State;

namespace ikemen::ssz_native {

struct SystemScriptState {
	// Placeholder for module-level state when wired.
};

// init(L) — initializes the system script module with a Lua state.
// Registers all system-level callbacks (game loop, rendering, audio, etc.).
// Phase 3: stub.
void system_script_init(lua_State* L);

} // namespace ikemen::ssz_native
