// trigger_script_service.hpp — Native C++ scaffolding for ssz_script/ssz/trigger-script.ssz
//
// trigger-script.ssz (1633 lines) implements the trigger evaluation engine —
// 170+ Lua-callable functions that expose game state (character position,
// life, power, state, hit detection, camera, etc.) to the Lua scripting layer.
//
// All functions follow the same pattern:
//   void func(lua_State* L, int& re)
// They parse arguments from Lua, read game state from char/common/chr/cmd,
// and push results back to the Lua stack.
//
// Phase 3: Stubbed.  register_function is the sole entry point — when wired,
// it registers all 170+ callbacks with a lua_State.

#pragma once

struct lua_State;

namespace ikemen::ssz_native {

// ── State ──
// cwc: current working character (opaque ^&chr.Char reference)
struct TriggerScriptState {
	// cwc — opaque character pointer (wired when char module converts)
};

// ── Callback registration ──
// Registers all trigger-script functions with the given Lua state.
// Phase 3: stub.
void register_function(lua_State* L);

// ── Core trigger callbacks (170+ functions) ──
// All follow: void func(lua_State* L, int& re);
//
// Categories: player/parent/root/helper/target/partner/enemy/enemynear
//             alive/ctrl/statetype/movetype/physics/anim/state
//             life/power/attack/defence/lifemax/powermax
//             posX/posY/velX/velY/facing
//             frontedge/backedge/leftedge/rightedge/topedge/bottomedge
//             roundstate/roundtime/roundno/gametime/gametype
//             hitdefattr/hitcount/hitover/movehit/moveguarded
//             var/fvar/svar/tvar (and many more)
//
// When wiring, each function reads the corresponding SSZ state field and
// pushes it to Lua via L.push*().

} // namespace ikemen::ssz_native
