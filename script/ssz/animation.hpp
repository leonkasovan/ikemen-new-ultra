#pragma once

#include "sff.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen {

extern std::vector<SffFile> g_sffFiles;

// ── Animation frame ─────────────────────────────────────────────────────
// Represents a single frame in a screenpack animation

struct AnimFrame {
	int16_t group = 0, number = 0;   // sprite reference (group, number)
	int16_t x = 0, y = 0;            // pixel offset from anim position
	int time = -1;                    // display time (-1 = hold forever)
	int priority = 0;
	std::string alphaMode;            // "a"=add, "s"=sub, "d"=none, "as"=add1
};

// ── Screen animation ────────────────────────────────────────────────────
// Represents a playable animation for screenpack rendering

struct ScreenAnim {
	int sffIndex = -1;                // index into g_sffFiles
	std::vector<AnimFrame> frames;
	int currentFrame = 0;
	int frameTimer = 0;
	int loopstart = 0;                // frame index to loop back to when reaching end
	bool finished = false;
	float velX = 0.0f, velY = 0.0f;   // velocity applied per update() call

	// Position/transform
	int posX = 0, posY = 0;
	int addPosX = 0, addPosY = 0;     // accumulated from animAddPos + velocity
	float scaleX = 1.0f, scaleY = 1.0f;
	int tileX = 0, tileY = 0;
	Rect window = {0, 0, 0, 0};       // clipping window (0,0,0,0 = no clip)
	bool hasWindow = false;
	int colorKey = -1;
	int srcAlpha = 255, dstAlpha = 0;
	bool alphaSet = false;

	void update();
	void draw(int extraX = 0, int extraY = 0);
};

// ── Frame data parser ───────────────────────────────────────────────────
// Parses frame data strings in the format:
//   "group,number, x,y, time[, priority[, alpha]]"
// One frame per line, semicolons for comments

bool parseFrameData(const char* data, std::vector<AnimFrame>& frames);

extern std::vector<ScreenAnim> g_anims;

} // namespace ikemen
