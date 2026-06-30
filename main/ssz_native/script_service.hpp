// script_service.hpp — Native C++ scaffolding for ssz_script/ssz/script.ssz
//
// script.ssz (2216 lines) implements the SSZ script runtime API — argument
// parsing (numArg, strArg, blArg, refArg), subsystem wrappers (SFF, sound,
// font, command, video), system-script init, trigger registration, and more.
//
// 250+ Lua-callable functions following the pattern:
//   void func(lua_State* L, int& re)
//
// Phase 3: Minimalist scaffolding with init/register_function entry points.
// Individual callback stubs deferred (too numerous to stub individually).

#pragma once

struct lua_State;

namespace ikemen::ssz_native {

// ── State ──
struct ScriptState {
	// Placeholder for module-level state when wired.
};

// ── Entry points ──

// init(L) — initializes the SSZ script module with a Lua state.
// Registers all argument-parsing and subsystem-wrapper functions.
// Phase 3: stub.
void script_init(lua_State* L);

} // namespace ikemen::ssz_native
