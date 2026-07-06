// fight_service.hpp — Native C++ implementation for ssz_script/ssz/fight.ssz
//
// fight.ssz (3578 lines) implements the fight engine — life/power bars,
// hit spark rendering, combo display, round timer, fight loop, and
// M.U.G.E.N.-compatible lifebar/fight control system.
//
// Phase 5: Full data model with all sub-structs (Lifebar, Powerbar, Face,
// Name, Time, Combo, Round, WinIcon, etc.) and fight.def section parsers.
// Rendering (draw) methods deferred until sdlplugin/sff/font are converted.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sff_service.hpp"

namespace ikemen::ssz_native {

// =========================================================================
// Helper functions (module-level)
// =========================================================================
void fight_laydraw(int ln);
void fight_laytext(int ln);
void fight_layspr(int ln);
void fight_read_spr(AnimData& a, const std::string& data);
void fight_read_anim(AnimData& a, const std::string& data);

// =========================================================================
// LifePower — per-player life/power
// =========================================================================
struct LifePowerData {
	float l{1.0f}, p{};
	int lv{};
	void set(float life, float power, int level);
};

// =========================================================================
// ActionList — animation action list
// =========================================================================
struct ActionListData {
	std::vector<int> actionList;
	int getAction(int no);
	int newAction(int no);
};

// =========================================================================
// Layout — position/scale/layer config (SSZ &.com.Layout)
// =========================================================================
struct FightLayoutData {
	int posx{}, posy{};
	int layerno{};
	float scale{1.0f};
};

// =========================================================================
// AnimFontSnd — animated text with font and sound
// =========================================================================
struct AnimFontSndData {
	int sndg{}, sndi{};
	int fontn{-1}, fontb{}, fonta{};
	std::string text;
	AnimData* anim{nullptr};
	FightLayoutData lay;
	int curtime{};
	
	void read(const std::string& prefix, const std::string& sc, ActionListData& al);
	void action();
	void draw(int layerno);
	bool noSound();
	bool noDisplay();
	bool end(int dt);
	void reset();
};

// =========================================================================
// Lifebar
// =========================================================================
struct LifebarData {
	int posx{}, posy{};
	int range_xz{}, range_xm{};
	AnimData bg0, bg0_lay;
	AnimData bg1, bg1_lay;
	AnimData bg2, bg2_lay;
	AnimData mid, mid_lay;
	AnimData front, front_lay;
	ActionListData al;
	float oldlife{}, midlife{}, midlifelim{};
	int mlifetime{};
	
	void read(const std::string& prefix, const std::string& sc);
	void step(float life, bool hit);
	void bgDraw(int layerno);
	void draw(int layerno, float life);
	void reset();
};

// =========================================================================
// Powerbar
// =========================================================================
struct PowerbarData {
	int posx{}, posy{};
	int range_xz{}, range_xm{};
	AnimData bg0, bg0_lay;
	AnimData bg1, bg1_lay;
	AnimData bg2, bg2_lay;
	AnimData mid, mid_lay;
	AnimData front, front_lay;
	int counter_fontn{}, counter_fontb{}, counter_fonta{};
	FightLayoutData counter_lay;
	ActionListData al;
	float midpower{}, midpowerlim{};
	int prevlevel{};
	
	void read(const std::string& prefix, const std::string& sc);
	void step(float power, int level);
	void bgDraw(int layerno);
	void draw(int layerno, float power, int level);
	void reset();
};

// =========================================================================
// Face — player portrait
// =========================================================================
struct FaceData {
	int posx{}, posy{};
	AnimData bg, bg_lay;
	int face_sprg{}, face_spri{};
	FightLayoutData face_lay;
	int teammate_posx{55}, teammate_posy{}, teammate_spacingx{}, teammate_spacingy{};
	AnimData teammate_bg, teammate_bg_lay;
	AnimData teammate_ko, teammate_ko_lay;
	int numko{};
	int teammate_face_sprg{}, teammate_face_spri{};
	FightLayoutData teammate_face_lay;
	ActionListData al;
	
	void read(const std::string& prefix, const std::string& sc);
	void step();
	void bgDraw(int layerno);
	void draw(int layerno);
	void reset();
};

// =========================================================================
// Name — player name display
// =========================================================================
struct NameData {
	int posx{}, posy{};
	int name_fontn{}, name_fontb{}, name_fonta{};
	FightLayoutData name_lay;
	AnimData bg, bg_lay;
	ActionListData al;
	
	void read(const std::string& prefix, const std::string& sc);
	void step();
	void bgDraw(int layerno);
	void draw(int layerno);
	void reset();
};

// =========================================================================
// Time — round timer
// =========================================================================
struct TimeData {
	int posx{}, posy{};
	int counter_fontn{}, counter_fontb{}, counter_fonta{};
	FightLayoutData counter_lay;
	AnimData bg, bg_lay;
	int framespercount{60};
	ActionListData al;
	
	void read(const std::string& prefix, const std::string& sc);
	void step();
	void bgDraw(int layerno);
	void draw(int layerno, int time);
	void drawSimple(int layerno);  // No-font fallback
	void reset();
};

// =========================================================================
// Combo — combo counter display
// =========================================================================
struct ComboData {
	int posx{}, posy{};
	float start_x{};
	int counter_fontn{}, counter_fontb{}, counter_shake{};
	FightLayoutData counter_lay;
	int text_fontn{}, text_fontb{};
	std::string text_text;
	FightLayoutData text_lay;
	int displaytime{};
	int cur{}, old{}, resttime{};
	float counterX{}, counterY{};
	int counterAS{}, counterAD{};
	int shaketime{};
	
	void read(const std::string& prefix, const std::string& sc);
	void step(int combo, int wt);
	void draw(int layerno);
	void reset();
};

// =========================================================================
// WinIcon — victory icons
// =========================================================================
struct WinIconData {
	int posx{}, posy{};
	int iconoffsetx{}, iconoffsety{};
	int useiconupto{};
	int counter_fontn{}, counter_fontb{}, counter_fonta{};
	FightLayoutData counter_lay;
	AnimData icon, icon_lay;
	ActionListData al;
	
	void read(const std::string& prefix, const std::string& sc);
	void step(int numwin);
	void draw(int layerno);
	void add(int wt);
	void reset();
	void clear();
};

// =========================================================================
// Round — round announcement (Round 1, Fight, KO, etc.)
// =========================================================================
enum class KOTy : uint8_t { None, Ko, TimeOver, DoubleKO, To };
enum class WinTy : uint8_t { Normal, Perfect, Special, Hyper, Throw, Cheap, Draw };

struct RoundData {
	int posx{}, posy{};
	int match_maxdrawgames{};
	int start_waittime{}, round_time{}, round_sndtime{};
	AnimFontSndData round_default;
	int fight_time{}, fight_sndtime{};
	AnimFontSndData fight;
	int ctrl_time{};
	int ko_time{}, ko_sndtime{};
	AnimFontSndData ko;
	AnimFontSndData dko;
	AnimFontSndData to;
	int slow_time{};
	int over_waittime{}, over_hittime{}, over_wintime{}, over_time{};
	int win_time{}, win_sndtime{};
	AnimFontSndData win;
	AnimFontSndData win2;
	AnimFontSndData drawn;
	ActionListData al;
	int cur{}, wt{}, swt{}, dt{}, wt2{}, swt2{}, dt2{};
	bool calledFight{};
	
	void read(const std::string& prefix, const std::string& sc);
	void callFight();
	bool act(KOTy ko);
	void draw(int layerno, KOTy ko,
		const std::string* winnerNames, int nameCount);
	void reset();
};

// =========================================================================
// Display helper structs (WinCount, Timer, Countdown, Score, etc.)
// =========================================================================
struct DisplayTextData {
	int posx{}, posy{};
	int text_fontn{}, text_fontb{}, text_fonta{};
	std::string text_text;
	FightLayoutData text_lay;
	AnimData bg, bg_lay;
	ActionListData al;
	
	void read(const std::string& prefix, const std::string& sc);
	void step();
	void bgDraw(int layerno);
	void draw(int layerno);
	void reset();
};

// =========================================================================
// Fight — top-level fight controller
// =========================================================================
struct FightData {
	std::string def;
	
	// Sub-components
	LifebarData lifebar[2];       // [0]=P1, [1]=P2
	PowerbarData powerbar[2];
	FaceData face[2];
	NameData name[2];
	TimeData time;
	ComboData combo[2];
	WinIconData winicon[2];
	RoundData round;
	DisplayTextData wincount[2];   // P1, P2
	DisplayTextData timer[2];      // P1, P2
	DisplayTextData countdown[2];  // P1, P2
	DisplayTextData score[2];      // P1, P2
	DisplayTextData match;
	DisplayTextData ailevel;
	DisplayTextData gamemode;
	DisplayTextData reward;
	DisplayTextData tourneystate;
	DisplayTextData matchstowin;
	DisplayTextData abyssdepth;
	DisplayTextData abyssreward;
	DisplayTextData nickname;

	// Player state
	LifePowerData lifePower[4];  // Up to 4 players
	
	// Lifecycle
	void load(const std::string& defPath);
	void step(int& tm, LifePowerData& life0, LifePowerData& life1, bool& hit, int& combo);
	void draw(int layerno, LifePowerData* life, int lifeCount,
		const std::string* names, int nameCount,
		bool nbd, int superplayer);
	void clear();
	void reset();
};

// =========================================================================
// Module-level state
// =========================================================================
struct FightState {
	FightData fight;
	bool initialized{};
};

// =========================================================================
// Module-level API
// =========================================================================
void fight_init();
FightState& fight_get_state();

} // namespace ikemen::ssz_native
