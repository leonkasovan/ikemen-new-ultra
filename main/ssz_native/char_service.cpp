// char_service.cpp — Stub for char.ssz scaffolding.
//
// Phase 4: Stub.  When wired, char_init registers character engine
// callbacks and initialization routines.

#include "char_service.hpp"

namespace ikemen::ssz_native {

void char_init(lua_State*) {
	// Phase 4: initialize character engine — state machine, animation,
	// hitbox/collision detection, AI, character data loading.
}

void char_init() {
	char_init(nullptr);
}

} // namespace ikemen::ssz_native
