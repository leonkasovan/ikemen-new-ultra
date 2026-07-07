// common_service.hpp — Native C++ scaffolding for ssz_script/ssz/common.ssz
//
// common.ssz is the largest Phase 3 module (1199 lines).  It defines game-wide
// state (life, power, timer, score, camera, display flags, palette effects,
// config section parsing, text utilities).
//
// Design note: CommonData and all nested types live directly in
// ikemen::ssz_native as DTO/utility types (consistent with share_service and
// system_service).  Functions use a `common_` prefix.
//
// Phase 3: All function bodies are stubs.  Wired as each dependent module
// (file, string, math, mesdialog, sdlevent, sdl, table) is converted.

#pragma once

#include <cstdint>
#include <cmath>
#include <string>
#include <vector>

namespace ikemen::ssz_native {

// ── TeamMode enum ──
enum class TeamMode { Single, Simul, Turns };

// ── IXY / FXY — int/float point types ──
struct IXY { int x{}, y{}; };
struct FXY { float x{}, y{}; };

// ── Camera::Stage ──
struct CameraStageData {
	int startx{};
	int boundleft{}, boundright{}, boundhigh{};
	float verticalfollow{0.2f};
	int tension{50}, floortension{};
	int overdrawlow{};
	int localw{320}, localh{240};
	float localscl{};
	float drawOffsetY{};
	int zoffset{};
	float ztopscale{1.0f};
	float xscale{1.0f}, yscale{1.0f};  // Stage horizontal/vertical scale factors
};

// ── Camera ──
struct CameraData {
	CameraStageData stg;
	bool zoom{};
	float zoomMin{5.0f / 6.0f};
	float zoomMax{15.0f / 14.0f};
	float zoomSpeed{12.0f};

	// Runtime camera state (set during fight loop)
	float x{}, y{};       // Current camera position
	float scale{1.0f};     // Current zoom scale

	// Computed camera state (set by cam_init())
	float boundL{}, boundR{}, boundH{};
	float halfWidth{};
	float minScale{1.0f};
	float screenZoff{};
	float zoff{};
	float xOffset{}, yOffset{};
	float bgaXOffset{}, bgaYOffset{};  // Stage background action offsets (set each frame by bg action)
	float screenX{}, screenY{};
	float zoomdelay{};
	float xMin{}, xMax{};
};

// ── Camera free functions (match common.ssz camera methods) ──
/// Initialize camera from stage data. Must be called before scaleBound/xBound/yBound.
// Forward declaration is needed because CommonData is defined after CameraData
// but cam_init takes a const CommonData& parameter.
struct CommonData;
void cam_init(CameraData& cam, const CommonData& cd);

/// Clamp scale to zoomMin/zoomMax range (returns 1.0 if zoom disabled).
/// SSZ: cam.scaleBound(scl)
float cam_scale_bound(const CameraData& cam, float scl);

/// Clamp x position within stage horizontal bounds at given scale.
/// SSZ: cam.xBound(scl, x)
float cam_x_bound(const CameraData& cam, float scl, float x);

/// Clamp y position within stage vertical bounds at given scale.
/// SSZ: cam.yBound(scl, y) where GameHeight is accessed from common scope.
float cam_y_bound(const CameraData& cam, float scl, float y, float gameHeight);

/// Return base scale from stage ztopscale.
/// SSZ: cam.baseScale()
float cam_base_scale(const CameraData& cam);

/// Update camera runtime state (scale, zoff, screenX, screenY, position).
/// SSZ: cam.update(scl, x, y) — called after round transitions and during
/// the fight loop to sync camera computed fields with current position/scale.
/// xOffset/yOffset depend on stage BGA data (stg.bga.xoffset/yoffset) which
/// is not yet wired; they are stubbed to 0.0f.
void cam_update(CameraData& cam, const CommonData& cd, float scl, float x, float y);

// Helper: compute camera ground level from stage zoffset
inline float camera_ground_level(const CameraData& cam) {
	return static_cast<float>(cam.stg.zoffset);
}

// ── Layout ──
struct LayoutData {
	FXY offset;
	int displaytime{-2};
	char facing{1};
	char vfacing{1};
	short layerno{};
	FXY scale;
};

// ── PalFX ──
struct PalFXData {
	int time{};
	int mulr{256}, mulg{256}, mulb{256};
	int addr{}, addg{}, addb{};
	int amplr{}, amplg{}, amplb{};
	int cycletime{}, sintime{};
	float color{1.0f};
	int invertall{}, negType{};
	int emulr{256}, emulg{256}, emulb{256};
	int eaddr{}, eaddg{}, eaddb{};
	float ecolor{1.0f};
	int einvertall{}, enegType{};
	bool enable{};

	/// Advance the PalFX by one frame.
	/// Decrements `time` if > 0. When `time` reaches 0, resets all color
	/// transformation fields (mul/add/ampl/invert) back to neutral defaults
	/// so the palette effect expires cleanly.
	/// SSZ: com.PalFX::step()
	void step();

	/// Clear2: reset to defaults while preserving enable/negType.
	/// SSZ: com.PalFX::clear2(keepFirstFlag)
	/// When keepFirstFlag=1, preserves `enable` and `negType`.
	void clear2(int keepFirstFlag);
};

/// Transform a source palette using PalFX effective fields, matching SSZ
/// PalFX::getFxPal() exactly.  The transformed palette is written to a
/// static module-level work buffer and returned as a const reference.
///
/// SSZ: .workpal = palfx.getFxPal(src_pal, nega)
///
/// The algorithm per palette entry:
///   1. Invert all (bitwise NOT) if einvertall is set
///   2. Blend each channel towards the average using ecolor
///   3. Saturated subtraction of negative add components
///   4. Per-channel: (channel + eadd*) * emul* / 256, clamped to 0-255
///
/// @param palfx  The PalFX whose effective fields to apply
/// @param src_pal  Source palette (must be 256 entries, 0x00RRGGBB format)
/// @param nega  Whether to use negative mode (negated when enegType==0)
/// @return Reference to the internal work palette with the transformed colors
const std::vector<uint32_t>& palfx_transform_palette(
	const PalFXData& palfx,
	const std::vector<uint32_t>& src_pal,
	bool nega = false);

// ── Section — config file section parser ──
struct SectionData {
	// params: NameTable<char>  (opaque — uses table_service)
};

// ── CommonData — module-level state ──
struct CommonData {
	// System info
	std::string operatingSystem;
	int coins{}, credits{};

	// Display state
	int lifebarDisplay{1}, gameType{};
	bool gamemodeDisplay{};
	std::string gameMode, gameService, playerSide, pauseVar, bgmName;

	// Stats
	float life{1.0f};
	int power{};
	float attack{1.0f}, defence{1.0f};
	float team1VS2Life{1.0f};
	float turnsRecoveryRate{1.0f / 300.0f};

	// Score
	bool scoreDisplay{};
	int score{}, scoreTotal{}, p1score{}, p2score{};

	// Timer
	int timer{}, countdownTimer{-1};
	std::string timerFormatted, countdownFormatted;
	bool timerDisplay{}, countdownDisplay{};

	// Reward
	std::string rewardFormatted;
	bool rewardDisplay{};

	// Round
	int roundTime{999 * 6}, roundsToWin{2}, matchsToWin{};
	std::string p1winsFormatted, p2winsFormatted;
	bool p1winsDisplay{}, p2winsDisplay{};
	int p1matchWins{}, p2matchWins{};
	int p1consecutiveWins{}, p2consecutiveWins{};

	// Tournament
	std::string tourneyState, matchnoInfo;
	bool matchnoDisplay{}, ailevelDisplay{};
	int cpuLevel{};

	// First attack
	bool firstAttack{};
	int firstAttackCount{};

	// Persistence
	bool persistLife{}, persistPower{}, persistRoundtime{};
	int lifePersistence{}, powerPersistence{}, timePersistence{};

	// Win counters
	int consecutiveWins{}, winTimeCount{}, winPerfectCount{};
	int winSpecialCount{}, winPerfectSpecialCount{};
	int winHyperCount{}, winPerfectHyperCount{};
	int winThrowCount{}, winPerfectThrowCount{};

	// Abyss
	int abyssDepth{1}, abyssDepthBoss{}, abyssDepthBossSpecial{};
	int abyssBossFight{}, abyssFinalDepth{};
	std::string abyssSP1, abyssSP2, abyssSP3, abyssSP4;

	// Game state
	bool sharedLife{true};
	int sysControls{}, gameState{};
	bool postMatchFlg{}, exitMatch{};

	// Display toggles
	int inputDisplay{}, attackDisplay{};
	int powerStateP1{}, powerStateP2{};
	int lifeStateP1{}, lifeStateP2{};
	int dummyState{}, dummyDistance{}, dummyGuard{}, dummyRecovery{};
	int counterHit{};

	// Recording
	int recordState{}, playbackState{};

	// Player stats
	int playerLife{}, playerPower{}, playerAttack{}, playerDefence{}, playerReward{};

	int suaveMode{};

	// Collision / debug
	bool clsndraw{}, debugdraw{}, pause{}, step{};
	int pauseMenu{};
	bool statusDraw{true};
	float accel{1.0f};
	bool autolevel{};

	// Resolution
	int GameWidth{}, GameHeight{};
	float WidthScale{}, HeightScale{};

	// Camera
	CameraData cam;

	// Tick
	int tickCount{};
	float tickCountF{}, lastTick{};
	float nextAddTime{}, oldNextAddTime{};
	int oldTickCount{-1};

	// Screen
	float screenleft{}, screenright{};
	float xmin{}, xmax{};
	float drawscale{NAN}; // NaN = not set (SSZ: drawscale = 0.0/0.0)
	float zoomposx{}, zoomposy{};
	float turbo{};
	int gametime{}, time{}, intro{20};

	// Match state
	int home{}, match{1}, lastMatch{-1};
	int p1mw{2}, p2mw{2};
	int round{1};
	std::vector<int> tmode, numturns, rexisted;
	int p1wins{}, p2wins{}, draws{};
	int win{-1};
	std::string debugScript;
	bool forceOver{};
	bool timeover{};  // Set true when round ends by timer expiration (lifebar displays "Time")
	int brightness{};

	// Spark sound (configurable SFX played when a hit spark appears)
	int sparkSoundGroup{-1};
	int sparkSoundNumber{};

	// Utility
	static constexpr int maxSimul = 10;
	std::vector<int> com, taglevel, inputRemap, numSimul;
	std::vector<bool> autoguard, powerShare;

	// Constants
	static constexpr int IERR = -2147483647 - 1; // consts::int_t::MIN
	static constexpr bool CharLocalCoord320 = true;
};

// ── Free-function stubs ──

void common_flag_init(CommonData&);
void common_reset_remap_input(CommonData&);
void common_set_size(CommonData&, int w, int h);
bool common_tick_frame(const CommonData&);
bool common_tick_next_frame(const CommonData&);
float common_tick_interpola(const CommonData&);
bool common_add_frame_time(CommonData&, float t);
void common_reset_frame_time(CommonData&);
bool common_match_over(const CommonData&);
int common_next_line(int& i, const std::string& str);
std::vector<std::string> common_split_lines(const std::string& str);
double common_atof(const std::string& str);
int common_atoi(const std::string& str);
std::string common_load_text(const std::string& filename, bool unicode);
std::string common_read_file_name(const std::string& f, bool unicode);
std::string common_load_file(const std::string& deffile, std::string& file,
	void* load_callback = nullptr);

/// Advance the round timer by one tick: decrement roundTime when
/// countdownTimer >= 0, and update timerFormatted to display string.
/// Called once per game tick during the fight loop.
void common_timer_step(CommonData& cd);

// No-arg convenience wrappers for bridge/SSZ ABI.
// These operate on an internal static CommonData instance.
void common_flag_init();
void common_reset_remap_input();
void common_set_size(int w, int h);
bool common_tick_frame();
bool common_tick_next_frame();
float common_tick_interpola();
bool common_add_frame_time(float t);
void common_reset_frame_time();
bool common_match_over();

// Screen fill wrappers (delegate to sdlplugin)
// SSZ: .com.screenFill(color) — fill entire screen with solid color
void common_screen_fill(uint32_t color);

// SSZ: .com.rectFill(rect, color, alpha) — fill a rectangle with color and alpha
// rect: destination rectangle (SdlRect-style: x, y, w, h)
// color: RGB packed as 0x00RRGGBB
// alpha: 0-255 (256 in SSZ maps to 255 here)
void common_rect_fill(const class SdlRect& rect, uint32_t color, int alpha);

// Accessor for the internal static CommonData instance.
// Used by other native services (e.g. loader) that need to read
// common state (round, team mode, select info) without having
// their own pointer to the CommonData.
CommonData& common_get_state();

} // namespace ikemen::ssz_native
