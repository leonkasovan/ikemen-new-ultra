#pragma once

#include "action.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen {

// ── Background definition ────────────────────────────────────────────────

struct BgDef {
	std::wstring bgType;
	std::vector<std::wstring> layers;
	float xscale = 1, yscale = 1;
	float xoffset = 0, yoffset = 0;

	void clear();
};

// ── Background controller ────────────────────────────────────────────────

struct BGCtrl {
	float time = 0;
	std::vector<float> params;
};

struct BGCTLNode {
	int waittime = 0;
	std::vector<BGCtrl> bgcList;
	BGCTLNode* nextnode = nullptr;
};

// ── Background layer ─────────────────────────────────────────────────────

struct BgLayer {
	Anim<Frame> anim;
	int   id = 0;
	int   typ = 0;       // 0:normal 1:anim 2:parallax 3:dummy
	bool  toplayer = false;
	bool  positionlink = false;
	float startx = 0, starty = 0;
	float startvx = 0, startvy = 0;
	float deltax = 1, deltay = 1;
	int   actionno = 0;
	float rasterxtspeed = 1, rasterxbspeed = 1;
	float yscalestart = 100, yscaledelta = 0;
	uint16_t twidth = 0, bwidth = 0;
	float velocityx = 0, velocityy = 0;
	float sinx_amplitude = 0, sinx_frequency = 1, sinx_phase = 0;
	float siny_amplitude = 0, siny_frequency = 1, siny_phase = 0;
	bool  enabled = true;

	void clear();
	void read(class Section& sc, BgLayer* link);
};

// ── Background ───────────────────────────────────────────────────────────

class Background {
public:
	void clear();
	void add(BGCtrl& c);
	void step();

	BgDef def;
	std::vector<BgLayer> layers;

private:
	BGCTLNode* m_top = nullptr;
	std::vector<BGCtrl> m_al;
};

} // namespace ikemen
