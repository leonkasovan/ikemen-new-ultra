// fight_service.hpp — Native C++ implementation for ssz_script/ssz/fight.ssz
//
// fight.ssz (3578 lines) implements the fight engine — life/power bars,
// hit spark rendering, combo display, round timer, fight loop, and
// M.U.G.E.N.-compatible lifebar/fight control.
//
// Phase 5: Core Fight struct + lifecycle. Lifebar/Powerbar/Face/Round
// sub-components deferred until font/sdlplugin rendering is converted.

#pragma once

#include <string>
#include <vector>

namespace ikemen::ssz_native {

// ── LifePower — per-player life/power state ──
struct LifePowerData {
	float l{1.0f}, p{};
	int lv{};
	void set(float life, float power, int level);
};

// ── Face — player portrait frame ──
struct FaceData {
	int face_sprg{}, face_spri{};
	int numko{};
	void read();
	void step();
	void reset();
};

// ── Fight — top-level fight control ──
struct FightData {
	std::string def;
	
	// Per-player data
	LifePowerData lifePower[2];
	FaceData face[2];

	// Lifecycle
	void load(const std::string& defPath);
	void step(int& tm, LifePowerData& life, bool& hit, int& combo);
	void draw(int layerno);
	void clear();
	void reset();
};

// ── Module-level state ──
struct FightState {
	FightData fight;
	bool initialized{};
};

// ── Module-level API ──
void fight_init();

} // namespace ikemen::ssz_native
