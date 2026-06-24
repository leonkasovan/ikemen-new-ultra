#pragma once

#include "bg.hpp"
#include "sff.hpp"
#include "../table.hpp"
#include "../math.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace ikemen {

// ── Environmental shake ─────────────────────────────────────────────────

struct EnvShake {
	int   time = 0;
	float freq = 1.04719755f; // PI/3
	int   ampl = -4;
	float phase = 0.0f / 0.0f;

	void clear();
	void setDefPhase();
	void next();
	float getOffset();
};

extern EnvShake g_envShake;

// ── Stage shadow ────────────────────────────────────────────────────────

struct Shadow {
	int   intensity = 128;
	uint32_t color = 0x808080;
	float yscale = 0.4f;
	int   fadeEnd = 0, fadeBgn = 0;
};

// ── Player position ─────────────────────────────────────────────────────

struct PlayerPos {
	int startx = -70, starty = 0, facing = 1;
};

// ── Stage definition ────────────────────────────────────────────────────

struct StageDef {
	std::wstring name;
	std::wstring bgmusic;
	std::wstring sffFile;
	int          localcoord[2] = {320, 240};
	float        xscale = 1, yscale = 1;
	int          zoffset = 200;
};

// ── Stage ───────────────────────────────────────────────────────────────

class Stage {
public:
	std::wstring load(const std::wstring& def);

	// Parsed data
	std::wstring defFile;
	std::wstring sprFile;
	std::wstring bgmusic;
	std::wstring name, displayName, author;
	std::wstring nameLow, displayNameLow, authorLow;

	Shadow sdw;
	PlayerPos p1, p2, p3, p4;
	float leftbound = NAN, rightbound = NAN;
	int   screenleft = 15, screenright = 15;
	int   reflection = 0;
	bool  hires = false, resetBg = true, debugBg = false;
	float localscl = 1, xscale = 1, yscale = 1;

	std::vector<BgLayer> bgLayers;

private:
	void parseInfo(class Section& sc);
	void parseCamera(Section& sc);
	void parsePlayerInfo(Section& sc);
	void parseBound(Section& sc);
	void parseStageInfo(Section& sc);
	void parseShadow(Section& sc);
	void parseReflection(Section& sc);
	void parseScaling(Section& sc);
	void parseMusic(Section& sc, bool unicode);
	void parseBgDef(Section& sc, bool unicode);
	void parseBg(Section& sc, int& linkIdx);
	void parseBgCtrlDef(Section& sc);
	void parseBgCtrl(Section& sc);

	void clear();
	void reset();
};

// ── Global stage state ──────────────────────────────────────────────────

extern std::wstring g_stageBgMusic;

} // namespace ikemen
