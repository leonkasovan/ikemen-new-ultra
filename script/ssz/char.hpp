#pragma once

#include "fight.hpp"
#include "stage.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen {

// ── System variable indices ─────────────────────────────────────────────

constexpr int iLIFE            = 10;
constexpr int iLIFEMAX         = 11;
constexpr int iLIFEMAX2        = 12;
constexpr int iPOWER           = 13;
constexpr int iPOWERMAX        = 14;
constexpr int iPOWERMAX2       = 15;
constexpr int iATTACK          = 16;
constexpr int iDEFENCE         = 17;
constexpr int iLIEDOWN_TIME    = 18;
constexpr int iAIRJUGGLE       = 19;
constexpr int iSPARKNO         = 20;
constexpr int iGUARD_SPARKNO   = 21;
constexpr int iKO_ECHO         = 22;
constexpr int iVOLUME          = 23;
constexpr int iINTPERSISTINDEX = 24;
constexpr int iFLOATPERSISTINDEX = 25;
constexpr int iGROUND_BACK     = 26;
constexpr int iGROUND_FRONT    = 27;
constexpr int iAIR_BACK        = 28;
constexpr int iAIR_FRONT       = 29;
constexpr int iHEIGHT          = 30;
constexpr int iATTACK_DIST     = 31;
constexpr int iPROJ_ATTACK_DIST = 32;
constexpr int iPROJ_DOSCALE    = 33;
constexpr int iHEAD_POSX       = 34;
constexpr int iHEAD_POSY       = 35;
constexpr int iMID_POSX        = 36;
constexpr int iMID_POSY        = 37;
constexpr int iSHADOWOFFSET    = 38;
constexpr int iDRAW_OFFSETX    = 39;
constexpr int iDRAW_OFFSETY    = 40;
constexpr int iAIRJUMP_CNT     = 41;
constexpr int iAIRJUMP_NUM     = 42;
constexpr int iAIRJUMP_HEIGHT  = 43;
constexpr int iHITCOUNT        = 44;
constexpr int iUNIQHITCOUNT    = 45;
constexpr int iPAUSEMOVETIME   = 46;
constexpr int iSUPERMOVETIME   = 47;
constexpr int iBINDTIME        = 48;
constexpr int SYSVAR_COUNT     = 49;

// ── Player state ────────────────────────────────────────────────────────

struct PlayerState {
	float  life         = 1000;
	float  lifeMax      = 1000;
	float  power        = 0;
	float  powerMax     = 1000;
	float  attack       = 100;
	float  defence      = 100;
	int    palNo        = 1;
	int    facing       = 1;
	int    playerNo     = 0;
	int    id           = 0;
	int    teamSide     = 0;

	std::vector<int> sysvar;

	PlayerState();
	void reset();
	void loadPalette(const std::wstring& def, int palno);
	int  palno() const { return palNo; }
};

// ── Character global info ───────────────────────────────────────────────

struct CharGlobalInfo {
	std::wstring def;
	int    palno     = 1;
	int    drawpalno = -1;

	void clear();
};

// ── Characters list ─────────────────────────────────────────────────────

struct CharList {
	std::vector<PlayerState> players;
	std::vector<CharGlobalInfo> cgi;
	std::vector<std::vector<PlayerState>> chars;
	std::vector<std::wstring> code;
	std::vector<int> wakewakaLength;
	int   id = 0;

	void clear();
	void charInit(PlayerState& p, int playerNo, int teamSide);
};

extern CharList g_chars;

// ── Win / KO types ──────────────────────────────────────────────────────

enum class WinType : int { N=0, K=1, KO=2, O=3, T=4, S=5, H=6, TH=7, P=8, D=9, SU=10 };
enum class KOTy   : int { NotYet=0 };

// ── Game state globals ──────────────────────────────────────────────────

extern int    g_changeStateNest;
extern int    g_waitdown, g_shuttertime;
extern bool   g_winskipped;
extern bool   g_bgmflag;
extern KOTy   g_ko;
extern std::vector<WinType> g_winty;
extern std::vector<int> g_wakewakaLength;
extern Stage* g_stage;

} // namespace ikemen
