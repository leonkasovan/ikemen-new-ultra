// stage_service.hpp — Native C++ implementation matching ssz_script/ssz/stage.ssz
//
// stage.ssz (736 lines) implements stage data management — background layers,
// camera bounds, music, stage loading, and environmental shake effects.
//
// Phase 5: Real implementation for EnvShake, def file parsing, and stage
// lifecycle. Background rendering (bgDraw) and SFF loading are deferred
// until bg_service and sff_service are converted.

#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>
#include <unordered_map>

// Pi constant — avoids M_PI portability issues across toolchains.
inline constexpr double kStagePi = 3.14159265358979323846;

namespace ikemen::ssz_native {

// Forward declarations
struct PalFXData; // defined in common_service.hpp

// ── PlayerData ──
// Maps to &.Player in stage.ssz. Player start positions and facing.
struct PlayerData {
	int startx{-70};
	int starty{0};
	int facing{1};
};

// ── ShadowData ──
// Maps to &.Shadow in stage.ssz. Shadow rendering parameters.
struct ShadowData {
	int intensity{128};
	uint32_t color{0x808080};
	float yscale{0.4f};
	int fadeend{std::numeric_limits<int>::min()};
	int fadebgn{std::numeric_limits<int>::min()};
	int fadeend_orig{std::numeric_limits<int>::min()};
	int fadebgn_orig{std::numeric_limits<int>::min()};
};

// ── EnvShakeData ──
// Maps to &EnvShake in stage.ssz. Screen shake effect for hits.
struct EnvShakeData {
	int time{};
	float freq{static_cast<float>(kStagePi) / 3.0f};
	int ampl{-4};
	float phase{NAN};

	void clear();
	void setDefPhase();
	void next();
	float getOffset() const;
};

// ── StageBgCtrlDef ──
// Maps to the internal bgcdef in stage.ssz (bgctrldef default state).
struct StageBgCtrlDef {
	int looptime{-1};
	// ctrlbg: vector of bg indices (opaque until bg_service converts)
};

// ── StageData ──
// Maps to &Stage in stage.ssz. Holds all stage state.
struct StageData {
	// ── Public fields (from SSZ) ──
	std::string def;
	std::string spr;
	std::string bgmusic;
	std::string name, displayname, author;
	std::string nameLow, displaynameLow, authorLow;

	// Opaque handles (wired when dependent modules convert):
	// sf:     &.sff.Sff           (SFF sprite file)
	// bg:     %&.bg.BackGround[]  (background layers)
	// actionList: %&.bg.Action[]  (animation action list)
	// actionTable: IntTable<int, bg.Action?> (action lookup)
	// bgctrlList: %&.bg.BGCtrl[]  (background control list)
	// bgctl:  &.bg.BGCTimeLine    (background control timeline)
	// bgcdef: &.bg.BGCtrl         (default bgctrl)
	// bga:    &.bg.BGAction       (background action state)
	// airFileMethods: &.sff.AirFileMethods

	// ── Parsed fields ──
	ShadowData sdw;
	PlayerData p1, p2, p3, p4;

	float leftbound{NAN};
	float rightbound{NAN};
	int screenleft{15}, screenright{15};
	int zoffsetlink{-1};
	int reflection{0};
	bool hires{false}, resetbg{true}, debugbg{false}, reflect{true};
	float localscl{};
	float xscale{1.0f}, yscale{1.0f};

	StageBgCtrlDef bgcdef;

	// ── Lifecycle ──

	/// Initialize default values (matches SSZ new()).
	void init();

	/// Parse and load a stage .def file. Returns empty string on success,
	/// or an error message on failure.
	std::string load(const std::string& defPath);

	/// Get an action by numeric ID via the action table.
	// bg::Action* getAction(int no);  // deferred

	/// Create a new animation action with the given number.
	// bg::Action* newAction(int no);  // deferred

	/// Advance background animation (step all bg layers + ctrl timeline).
	void action();

	/// Draw all background layers for the given layer flag.
	void bgDraw(bool t, float x, float y, float scl);

	/// Clear all stage data and reset to default state.
	void clear();

	/// Reset stage for next round (reset animations, ctrl timelines).
	void reset();
};

// ── Module-level state ──

/// Initialize the stage module. Called once at startup.
void stage_init();

/// Load a stage by .def path. Thread-safe wrapper.
std::string stage_load(const std::string& defPath);

/// Advance stage animations. Called each frame.
void stage_action();

/// Draw backgrounds for the given layer flag.
void stage_bg_draw(bool t, float x, float y, float scl);

/// Clear the currently loaded stage.
void stage_clear();

/// Reset stage for next round.
void stage_reset();

/// Access the module-global EnvShake state.
EnvShakeData& stage_get_env_shake();	/// Access the module-global bgPalFX for background palette effects.
	PalFXData& stage_get_bg_palfx();

/// Access the module-global bgmusic string.
std::string& stage_get_bgmusic();

} // namespace ikemen::ssz_native
