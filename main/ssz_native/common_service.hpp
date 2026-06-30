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
};

// ── Camera ──
struct CameraData {
	CameraStageData stg;
	bool zoom{};
	float zoomMin{5.0f / 6.0f};
	float zoomMax{15.0f / 14.0f};
	float zoomSpeed{12.0f};
};

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
};

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
	float drawscale{};
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
	int brightness{};

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
int common_next_line(int i, const std::string& str);
std::vector<std::string> common_split_lines(const std::string& str);
double common_atof(const std::string& str);
int common_atoi(const std::string& str);
std::string common_load_text(const std::string& filename, bool unicode);
std::string common_read_file_name(const std::string& f, bool unicode);
std::string common_load_file(const std::string& deffile, std::string& file,
	void* load_callback = nullptr);

} // namespace ikemen::ssz_native
