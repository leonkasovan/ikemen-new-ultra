// fighting_service.hpp — Stub for ssz_script/ssz/fighting.ssz (Phase 4).
//
// fighting.ssz (671 lines) implements fight/match orchestration:
// win count management, combo/chain system, hit state, camera control,
// and the main game loop. The native implementation is placeholder-only.

#pragma once

namespace ikemen::ssz_native {

struct FightingState {
	int winCountP1{}, winCountP2{};
	int roundCount{};
	bool matchOver{};
};

void fighting_init();
void fighting_main();
FightingState& fighting_get_state();

} // namespace ikemen::ssz_native
