// bg_service.hpp — Native C++ implementation matching ssz_script/ssz/bg.ssz
//
// bg.ssz (726 lines) implements background rendering and animation —
// background layers, parallax, animation actions, control timelines,
// and rendering.
//
// Phase 5: Real implementation for BGAction, Action, BackGround
// section parsing, BGCtrl, ActiveCtrlList, and BGCTimeLine.
// Rendering (BackGround::draw) deferred until sdlplugin is converted.

#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "sff_service.hpp"   // for AnimData, FrameData, SpriteData

namespace ikemen::ssz_native {

// Forward declaration
struct SectionData;

// =========================================================================
// Constants
// =========================================================================
inline constexpr double kBgPi = 3.14159265358979323846;

// =========================================================================
// Utility functions
// =========================================================================

/// Split a parameter string by commas/whitespace (like SSZ splitParams).
std::vector<std::string> bg_split_params(const std::string& paramStr);

/// Parse a CSV string into an array of T (like SSZ cta<_t>).
template<typename T>
std::vector<T> bg_cta(const std::string& csv);

/// Parse a pair of values from a string (like SSZ readPair).
template<typename T>
void bg_read_pair(T& x, T& y, const std::string& data);

// =========================================================================
// BGAction — Background action state (position, velocity, sinusoidal)
// =========================================================================
struct BGActionData {
	float xoffset{}, yoffset{};
	float sinxoffset{}, sinyoffset{};
	float x{}, y{}, vx{}, vy{};
	float xradius{}, yradius{};
	int sinxtime{}, sinytime{};
	int sinxlooptime{}, sinylooptime{};

	void clear();
	void action();
};

// =========================================================================
// Frame — Background animation frame
// =========================================================================
struct BgFrameData {
	std::vector<float> ex;
	int time{-1};
	short group{-1}, number{0};
	short x{0}, y{0};
	uint8_t salpha{255}, dalpha{0};
	int8_t h{1}, v{1};
};

// =========================================================================
// Action — Background animation action (list of frames)
// =========================================================================
struct BgActionData {
	int no{};
	std::vector<BgFrameData> frames;
	int loopstart{0};

	void read(const std::vector<std::string>& lines, int& i);
};

// =========================================================================
// BackGround — A single background layer
// =========================================================================
struct AnimData;  // from sff_service.hpp

struct BackGroundData {
	// Anim: animation state machine (from sff)
	AnimData* anim{nullptr};
	// Owned anim data (when bg owns the animation)
	AnimData anim_owned;

	BGActionData bga;
	int id{0};
	float startx{}, starty{}, xofs{}, camstartx{};
	float deltax{1.0f}, deltay{1.0f};
	float xtscale{1.0f}, xbscale{1.0f};
	float rasterxtspeed{1.0f}, rasterxbspeed{1.0f};
	float yscalestart{100.0f};
	float yscaledelta{};
	int actionno{-1};
	float startvx{}, startvy{};
	float startxrad{}, startyrad{};
	int startsinxt{}, startsinyt{};
	int startsinxlt{}, startsinylt{};
	uint16_t twidth{}, bwidth{};
	bool visible{true}, active{true};
	bool positionlink{}, toplayer{};

	// Window clipping
	int win_x{-32768}, win_y{-32768}, win_w{65535}, win_h{65535};
	float windowdeltax{}, windowdeltay{};

	void reset();
	void read(SectionData& sc, BackGroundData* link);
	void setup();
	void draw(float x, float y, float scl, float bgscl,
		float localscl, float xscale, float yscale, float shakeY,
		const PalFXData* fx = nullptr);
};

// =========================================================================
// BgcType enum
// =========================================================================
enum class BgcType : uint8_t {
	Null, Anim, Visible, Enable, PosSet, PosAdd, SinX, SinY, VelSet, VelAdd
};

// =========================================================================
// BGCtrl — Background control event
// =========================================================================
struct BGCtrlData {
	std::vector<BackGroundData*> ctrlbg;
	int currenttime{}, starttime{}, endtime{}, looptime{-1};
	BgcType typ{BgcType::Null};
	float x{}, y{};
	int v1{}, v2{}, v3{};
	bool setx{}, sety{};
	bool positionlink{}, flag{};
	int idx{};

	void read(SectionData& sc, int index);
};

// =========================================================================
// ActiveCtrlList — Active control event list (linked list)
// =========================================================================
struct ActiveCtrlList {
	struct Cell {
		BGCtrlData* bgc{};
		Cell* next{};
	};
	Cell* top{nullptr};

	void add(BGCtrlData* bgc);
	std::vector<BGCtrlData*> act();
	void clear();
};

// =========================================================================
// BGCTimeLine — Background control timeline
// =========================================================================
struct BGCTimeLine {
	struct Node {
		Node* nextnode{nullptr};
		std::vector<BGCtrlData*> bgcList;
		int waittime{};
	};
	Node* top{nullptr};
	ActiveCtrlList al;

	void add(BGCtrlData* bgc);
	template<typename T>
	std::vector<BGCtrlData*> step(T& s);
	void clear();
};

// =========================================================================
// Module-level API
// =========================================================================

/// Initialize the background module.
void bg_init();

// ── BgState — Module-level state ──
// Previously a Phase 4 stub. Now holds the background system state.
struct BgState {
	BGCTimeLine timeline;
	std::vector<BackGroundData> layers;
	std::vector<BgActionData> actions;
};

/// Access the module-level BgState (background layers, actions, timeline).
BgState& bg_get_state();

} // namespace ikemen::ssz_native
