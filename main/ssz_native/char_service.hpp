// char_service.hpp — Native C++ scaffolding for ssz_script/ssz/char.ssz
//
// char.ssz (7665 lines) implements the character engine — character state
// machine, animation, sprite rendering, hitbox/collision detection, AI,
// and character data loading.

#pragma once

struct lua_State;

namespace ikemen::ssz_native {

struct CharState {
	// Phase 4: placeholder — populated when char module is wired.
};

// init(L) — initializes the character engine with a Lua state.
// Phase 4: stub.
void char_init(lua_State* L);

// No-arg convenience wrapper for the SSZ bridge — calls
// char_init(nullptr) as a stub placeholder for future wiring.
void char_init();

} // namespace ikemen::ssz_native
