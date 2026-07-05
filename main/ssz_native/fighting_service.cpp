// fighting_service.cpp — Stub for fighting.ssz scaffolding (Phase 4).
//
// fighting.ssz (671 lines) implements fight/match orchestration.
// The native implementation is not yet wired — this file provides
// placeholder state and init. Full fight loop integration is deferred
// until the char_service, fight_service, and SDL rendering layers
// are converted.

#include "fighting_service.hpp"
#include "ssz_trace.hpp"

namespace ikemen::ssz_native {

static FightingState g_fighting_state;

FightingState& fighting_get_state() { return g_fighting_state; }

void fighting_init() {
	SSZ_TRACE_CAT(TRACE_SYS, "fighting_init");
	g_fighting_state = FightingState{};
}

void fighting_main() {
	SSZ_TRACE_CAT(TRACE_SYS, "fighting_main");
	// Stub — fight orchestration not yet implemented.
	// Will call round init → fight loop → round end → match over.
}

} // namespace ikemen::ssz_native
