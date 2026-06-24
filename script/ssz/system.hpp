#pragma once

#include "action.hpp"
#include "fight.hpp"

#include <string>
#include <vector>

namespace ikemen {

// ── Character select entry ──────────────────────────────────────────────

struct SelectChar {
	std::wstring def;
	std::wstring name;
	std::wstring sprite;
	std::wstring anim;
	Sprite* facePortrait = nullptr;
	Sprite* bigPortrait  = nullptr;
	Sprite* orderPortrait= nullptr;
	Sprite* vsPortrait   = nullptr;
	Sprite* winPortrait  = nullptr;
	Sprite* loserPortrait= nullptr;
	Sprite* resultPortrait= nullptr;
	Sprite* extraPortrait = nullptr;
};

// ── Stage select entry ──────────────────────────────────────────────────

struct SelectStage {
	std::wstring def;
	std::wstring name;
	std::wstring sprite;
};

// ── Team entry ──────────────────────────────────────────────────────────

struct TeamEntry {
	std::vector<int> chars;
	int  singleFlag = 0;
};

// ── Match state ─────────────────────────────────────────────────────────

struct MatchState {
	int   matchNo = 1;
	int   roundNo = 1;
	int   homeSide = 0;
	int   p1Wins = 0, p2Wins = 0, draws = 0;
	int   p1MatchWins = 0, p2MatchWins = 0;
	int   consecutiveWins = 0;
	int   teamMode = 0; // 0=Single, 1=Simul, 2=Turns
	int   roundsToWin = 2;
	int   matchsToWin = 0;
	bool  matchOver = false;

	void reset();
};

// ── Character / Stage Select ──────────────────────────────────────────

struct Select {
	std::vector<SelectChar>  charlist;
	std::vector<SelectStage> stagelist;

	int   columns   = 8;
	int   rows      = 4;
	float cellsizex = 1;
	float cellsizey = 1;
	float cellscalex = 1;
	float cellscaley = 1;
	float randxscl  = 1;
	float randyscl  = 1;
	void* randomspr = nullptr;

	void        addChar(const std::wstring& def);
	void        addStage(const std::wstring& def);
	SelectChar  getChar(int n) const;
	SelectStage getStage(int n) const;
	void        clear();
};

struct SelectInfo {
	Select sel;
	std::vector<std::vector<int>> p;
};

struct FightDef {
	int dummy = 0;
	void new_(int) {}
	std::wstring load(const std::wstring&) { return L""; }
};

struct CommonSettings {
	int   lifebarDisplay   = 0;
	float life             = 1.0f;
	float power            = 1.0f;
	float team1VS2Life     = 1.0f;
	float turnsRecoveryRate = 1.0f;
	int   tmode[2]         = {0, 0};
	int   numturns[2]      = {2, 2};
	int   numSimul[2]      = {2, 2};
};

struct SystemModule {
	SelectInfo selinf;
	FightDef   fig;
};

extern SystemModule   syst;
extern CommonSettings com;

} // namespace ikemen
