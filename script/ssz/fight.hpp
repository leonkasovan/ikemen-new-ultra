#pragma once

#include "bg.hpp"
#include "command.hpp"
#include "sff.hpp"

#include "../save/config.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen {

// ── Life/Power ──────────────────────────────────────────────────────────

struct LifePower {
	float l = 0, p = 0;
	int   lv = 0;
	void set(float life, float power, int level) { l = life; p = power; lv = level; }
};

// ── Win info ────────────────────────────────────────────────────────────

struct WinInfo {
	int  winType = 0;
	int  winTime = 0;
	bool winPerfect = false;
	bool winSpecial = false;
	bool winHyper = false;
	bool winThrow = false;

	void reset();
};

// ── Round info ──────────────────────────────────────────────────────────

struct RoundInfo {
	int  active = 0;
	int  order = 0;
	bool drawgame = false;
	bool ko = false;
	bool koDraw = false;
	bool over = false;

	void reset();
};

// ── Combo info ──────────────────────────────────────────────────────────

struct ComboInfo {
	int  counter = 0;
	int  damage = 0;
	int  hits = 0;
	bool display = false;

	void reset();
};

// ── Timer ───────────────────────────────────────────────────────────────

struct TimerInfo {
	int  time = 0;
	int  countdown = -1;
	bool display = false;

	void reset();
};

} // namespace ikemen
