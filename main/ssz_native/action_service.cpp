// action_service.cpp — Real implementations for action.ssz (Phase 5).
// Defines Rect, Frame, ActionData types used by the animation system.
// Frames store collision boxes (clsn), sprite references, and timing.

#include "action_service.hpp"

namespace ikemen::ssz_native {

void action_init() {
	// Action module initialization.
	// The data structures (Rect, Frame, ActionData) are plain structs
	// used by other native modules (char, sff, bg) — no runtime state
	// needs to be initialized at the module level.
}

} // namespace ikemen::ssz_native
