// fight_service.cpp — Real implementations for fight.ssz (Phase 5).
// Core Fight lifecycle, LifePower, and Face management.
// Lifebar/Powerbar/Combo/Round rendering deferred (depends on font/sdlplugin).

#include "fight_service.hpp"
#include "common_service.hpp"

namespace ikemen::ssz_native {

// =========================================================================
// Module-level state
// =========================================================================
static FightState g_fight_state;

// =========================================================================
// LifePowerData
// =========================================================================

void LifePowerData::set(float life, float power, int level) {
	l = life;
	p = power;
	lv = level;
}

// =========================================================================
// FaceData
// =========================================================================

void FaceData::read() {
	// Stub — face config parsing deferred (depends on fight.def format)
}

void FaceData::step() {
	// Stub — face animation stepping deferred
}

void FaceData::reset() {
	numko = 0;
	face_sprg = 0;
	face_spri = 0;
}

// =========================================================================
// FightData
// =========================================================================

void FightData::load(const std::string& defPath) {
	def = defPath;
	
	// Read fight.def for lifebar/font config
	std::string buf = common_load_text(defPath, false);
	if (buf.empty()) return;

	// Parse sections for basic fight config
	auto lines = common_split_lines(buf);
	for (size_t i = 0; i < lines.size(); i++) {
		const auto& line = lines[i];
		if (line.empty() || line[0] == ';') continue;
		
		if (line[0] == '[') {
			// Section headers — parse as needed
			// [Lifebar], [Powerbar], [Face], etc.
			continue;
		}
	}
	
	// Initialize per-player state
	for (auto& lp : lifePower) {
		lp.l = 1.0f;
		lp.p = 0.0f;
		lp.lv = 0;
	}
	for (auto& f : face) {
		f.reset();
	}
}

void FightData::step(int& tm, LifePowerData& life, bool& hit, int& combo) {
	// SSZ: step() advances fight state — life/power animation, combo timing
	// For now, just update life values from inputs
	(void)tm;

	// Store life/power values
	lifePower[0].l = life.l;
	lifePower[0].p = life.p;
	lifePower[1].l = life.l; // simplified — needs separate per-player tracking
	lifePower[1].p = life.p;

	(void)hit;
	(void)combo;
}

void FightData::draw(int layerno) {
	// Rendering deferred — needs font/sff/sdlplugin for lifebar, face, etc.
	(void)layerno;
}

void FightData::clear() {
	def.clear();
	for (auto& lp : lifePower) { lp.l = 1.0f; lp.p = 0.0f; lp.lv = 0; }
	for (auto& f : face) f.reset();
}

void FightData::reset() {
	// SSZ: reset for next round
	for (auto& lp : lifePower) { lp.l = 1.0f; lp.p = 0.0f; }
	for (auto& f : face) f.reset();
}

// =========================================================================
// Module-level API
// =========================================================================

void fight_init() {
	g_fight_state = FightState{};
	g_fight_state.initialized = true;
}

} // namespace ikemen::ssz_native
