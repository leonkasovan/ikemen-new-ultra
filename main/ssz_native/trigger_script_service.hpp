// trigger_script_service.hpp — Native C++ implementation for ssz_script/ssz/trigger-script.ssz
//
// trigger-script.ssz (1633 lines) implements the trigger evaluation engine —
// 170+ Lua-callable functions that expose game state (character position,
// life, power, state, hit detection, camera, etc.) to the Lua scripting layer.
//
// All functions follow the standard lua_CFunction pattern:
//   int func(lua_State* L)
// They parse arguments from Lua, read game state from char/common/chr/cmd,
// and push results back to the Lua stack.
//
// Phase 5: Full implementation. All 130+ trigger functions registered in
// register_function(). Functions delegate to common_service for game state
// and char_service for per-character state (cwc).

#pragma once

#include <string>

struct lua_State;

namespace ikemen::ssz_native {

// ── State ──
// cwc: current working character pointer (opaque)
struct TriggerScriptState {
	void* cwc{nullptr};
};

// ── Callback registration ──
// Registers all trigger-script functions with the given Lua state.
// Called from the bridge when IKEMEN_NATIVE_TRIGGER_SCRIPT_LIB=1.
void register_function(lua_State* L);

// No-arg convenience wrapper for the SSZ bridge.
void register_function();

// ── State accessors ──
TriggerScriptState& trigger_script_get_state();

} // namespace ikemen::ssz_native
