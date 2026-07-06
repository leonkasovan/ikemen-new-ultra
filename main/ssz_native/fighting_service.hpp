// fighting_service.hpp — Native C++ implementation for ssz_script/ssz/fighting.ssz
//
// fighting.ssz (671 lines) implements fight/match orchestration:
// win count management for auto-leveling, the main game() fight loop
// (round init, camera control, character actions, debug overlay, pause
// menu), and round-end/match-end transition logic.
//
// Phase 6: Full implementation. All three major components are realized:
//   1. WincntMgr — persistent auto-leveling via NameTable<int[]>
//   2. game() — the fight orchestration loop (all sub-functions)
//   3. main() — public entry point

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "table_service.hpp"

namespace ikemen::ssz_native {

// =========================================================================
// WincntMgr — Win count manager for auto-leveling
// =========================================================================
// Matches &WincntMgr in fighting.ssz. Tracks per-character win counts
// across palette slots, persisted to debug/autolevelssz.log.
struct WincntMgrData {
	// File path for persistence (always "save/debug/autolevelssz.log")
	std::string wincFN{"save/debug/autolevelssz.log"};
	// NameTable mapping char def path -> int[] (per-palette win counts)
	table::NameTable<std::vector<int>> winct;

	// Create a zero-initialized array of the given size
	static std::vector<int> zeroAry(int sz);

	// Init — read persistence file if autolevel is on
	void init();

	// Deinit — write persistence file on match end
	void deinit();

	// Get win count item for a character, extending to NumCharPalettes as needed
	std::vector<int> getItem(const std::string& name);

	// Set win count item for a character, averaging non-selectable palettes
	void setItem(int pn, const std::vector<int>& item);

	// Win point multiplier for simul/turns mode
	int winPoint(int i);

	// Record a win for player slot i
	void win(int i);

	// Record a loss for player slot i
	void lose(int i);

	// Get the auto-level for character slot i
	int getLevel(int i);
};

// =========================================================================
// FightingState — Module-level state for the fight orchestration loop
// =========================================================================
struct FightingState {
	// ── WincntMgr ──
	WincntMgrData wm;

	// ── game() local state ──
	// These persist across rounds within a match

	// Debug text input line
	std::string line;

	// Camera / scroll tracking
	float x{0.0f}, y{0.0f}, l{0.0f}, r{0.0f}, bl{0.0f}, br{0.0f};
	float scl{1.0f}, sclmul{1.0f};
	float newx{0.0f}, newy{0.0f};

	// Old wins snapshot for reset
	int oldp1wins{}, oldp2wins{}, olddraws{};

	// Pause menu state
	bool pmSt{false}, pmEsc{false};

	// Per-character saved state (life, power, vars) for round reset
	std::vector<int> savLif, savPow;
	std::vector<std::vector<int>> savVar;
	std::vector<std::vector<float>> savFvar;

	// Stage time accumulator (persisted through share)
	int stagetime{0};
};

// =========================================================================
// Module-level API
// =========================================================================

/// Initialize the fighting module (resets state).
void fighting_init();

/// Run the fight orchestration loop (match flow).
/// Called once per match from the bridge — drives the entire match lifecycle.
void fighting_main();

/// Get the module-level fighting state.
FightingState& fighting_get_state();

} // namespace ikemen::ssz_native
