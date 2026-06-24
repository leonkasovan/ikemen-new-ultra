#pragma once

#include "sff.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen {

// ── Collision rectangle ──────────────────────────────────────────────────

struct CsnRect {
	int l = 0, t = 0, r = -1, b = -1;
};

// ── Animation frame ──────────────────────────────────────────────────────

struct Frame {
	std::vector<CsnRect> clsn;
	std::vector<float>   ex;
	int          time = -1;
	int16_t      group = -1, number = 0;
	int16_t      x = 0, y = 0;
	uint8_t      salpha = 255, dalpha = 0;
	int8_t       h = 1, v = 1;

	FrameMethods<Frame> fam;

	CsnRect* clsn1();
	CsnRect* clsn2();
};

// ── Action (animation sequence) ──────────────────────────────────────────

class Action {
public:
	int          no = 0;
	Anim<Frame>  ani;

	Action();

	void copy(Action& a);
	void read(const std::vector<std::wstring>& lines, size_t& i);
};

// ── Drawn collision ─────────────────────────────────────────────────────

struct DrawnClsn {
	std::vector<CsnRect> clsn;
	float x = 0, y = 0, xscale = 1, yscale = 1;

	void set(const std::vector<CsnRect>& cl, float x, float y, float xs, float ys);
};

} // namespace ikemen
