// config_service.hpp — Native C++ equivalent of ssz_script/save/config.ssz
//
// config.ssz (179 lines) defines engine configuration constants:
//   Video, Audio, Performance, Game, Portraits, System, and Input settings.
// All values are compile-time constants in SSZ; the native equivalent
// is a ConfigData struct with matching default values plus load/save I/O.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ikemen::ssz_native {

struct KeyBindings {
	int jn = -2;
	int u = 0, d = 0, l = 0, r = 0;
	int a = 0, b = 0, c = 0;
	int x = 0, y = 0, z = 0;
	int q = 0, w = 0, e = 0, s = 0;

	void set(int jn_, int u_, int d_, int l_, int r_,
	         int a_, int b_, int c_,
	         int x_, int y_, int z_,
	         int q_, int w_, int e_, int s_) {
		jn = jn_; u = u_; d = d_; l = l_; r = r_;
		a = a_; b = b_; c = c_;
		x = x_; y = y_; z = z_;
		q = q_; w = w_; e = e_; s = s_;
	}
};

struct ConfigData {
	// Video
	int Renderer{};
	bool AspectRatio{};
	bool FullScreen{};
	bool FullScreenExclusive{};
	int WindowType{1};
	int Width{640};
	int Height{480};
	int Brightness{256};
	float Opacity{1.0f};

	// Audio
	float GlVol{0.8f};
	float SEVol{0.8f};
	float BGMVol{0.5f};
	float PanStr{0.8f};
	int VideoVol{100};

	// Performance
	bool SaveMemory{};
	int HelperMax{56};
	int PlayerProjectileMax{50};
	int ExplodMax{256};
	int AfterImageMax{8};

	// Game
	int GameSpeed{60};
	float Attack_LifeToPowerMul{0.7f};
	float GetHit_LifeToPowerMul{0.6f};
	float Super_TargetDefenceMul{1.5f};
	float LifebarFontScale{1.0f};

	// Portraits
	int CharPortraitsGroup{9000};
	int CharFacePortraitIndex{};
	int CharBigPortraitIndex{1};
	int CharWinnerPortraitIndex{1};
	int CharLoserPortraitIndex{1};
	int CharOrderPortraitIndex{4};
	int CharVSPortraitIndex{1};
	int CharResultsPortraitIndex{2};
	int CharExtraPortraitIndex{7};
	int StagePortraitsGroup{9000};
	int StageIconPortraitIndex{};
	int StageBigPortraitIndex{1};
	int StageVSPortraitIndex{2};
	int StageWinPortraitIndex{3};
	int StageExtraPortraitIndex{4};

	// System
	std::string Executable{"Ikemen Plus Ultra.exe"};
	std::string WindowTitle{"I.K.E.M.E.N. PLUS ULTRA"};
	std::string ScreenshotFolder{"screenshots"};
	std::string listenPort{"7500"};
	std::string UserName{"Nickname"};
	std::string GlobalAnims{"data/common.air"};
	std::string GlobalCommands{"data/common.cmd"};
	std::string GlobalMatch{"data/match.cns"};
	std::string system{"script/main.lua"};
	std::string GamepadMappings{"lib/external/gamecontrollerdb.txt"};

	// Input
	bool IgnoreMostErrors{true};
	std::array<KeyBindings, 14> Input{};
};

inline ConfigData make_default_config() {
	ConfigData c;
	// P1 KEYBOARD - BATTLE (index 0)
	c.Input[0].set(-1, 273, 274, 276, 275, 97, 115, 100, 122, 120, 99, 113, 101, 118, 102);
	// P2 KEYBOARD - BATTLE (index 1)
	c.Input[1].set(-1, 264, 258, 260, 262, 117, 105, 111, 106, 107, 108, 263, 265, 261, 256);
	// P1 GAMEPAD - BATTLE (index 2)
	c.Input[2].set(0, -3, -4, -1, -2, 0, 1, 4, 2, 3, 5, 8, 9, -10, 6);
	// P2 GAMEPAD - BATTLE (index 3)
	c.Input[3].set(1, -3, -4, -1, -2, 0, 1, 4, 2, 3, 5, 8, 9, -10, 6);
	// (indices 4-9 unused)
	// P1 KEYBOARD - MENU (index 10)
	c.Input[10].set(-1, 273, 274, 276, 275, 97, 115, 100, 122, 120, 99, 113, 101, 8, 13);
	// P2 KEYBOARD - MENU (index 11)
	c.Input[11].set(-1, 264, 258, 260, 262, 117, 105, 111, 106, 107, 108, 263, 265, 266, 271);
	// P1 GAMEPAD - MENU (index 12)
	c.Input[12].set(0, -7, -8, -5, -6, 2, 3, 4, 5, 8, 9, 6, 0, 1, 7);
	// P2 GAMEPAD - MENU (index 13)
	c.Input[13].set(1, -7, -8, -5, -6, 2, 3, 4, 5, 8, 9, 6, 0, 1, 7);
	return c;
}

ConfigData& config_get_state();

bool config_save(const std::string& path, const ConfigData& cfg);
bool config_load(const std::string& path, ConfigData& cfg);

} // namespace ikemen::ssz_native
