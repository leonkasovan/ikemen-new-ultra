// system_script_service.hpp — Native C++ implementation for ssz_script/ssz/system-script.ssz
//
// system-script.ssz (2403 lines) implements the system-level Lua bridge —
// game initialization, match flow, rendering, audio, select screen, pause
// menu, results screen, and the main game loop.
//
// 200+ Lua-callable functions following the lua_CFunction pattern:
//   int func(lua_State* L)
//
// Phase 5: Full implementation. All 120+ system functions registered in
// system_script_init(). Functions delegate to native service modules
// (script, command, sdlplugin, sdlevent, common, config, char, etc.).

#pragma once

struct lua_State;

namespace ikemen::ssz_native {

struct SystemScriptState {
	// Module-level state placeholder for future system-script state.
};

// init(L) — initializes the system script module with a Lua state.
// Registers all system-level callbacks (game loop, rendering, audio, etc.).
// Also calls script_init(L) to register the core script.ssz functions.
void system_script_init(lua_State* L);

// No-arg convenience wrapper for the SSZ bridge.
void system_script_init();

// ── State accessor ──
SystemScriptState& system_script_get_state();

} // namespace ikemen::ssz_native
