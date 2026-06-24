#include "animation.hpp"
#include "sff.hpp"
#include "../save/config.hpp"
#include "../alpha/sdlplugin.hpp"
#include "sszdef.h"

#include <cstdio>
#include <sstream>

namespace ikemen {

// Global animation state
std::vector<ScreenAnim> g_anims;

// ── Screen anim: update frame timer ─────────────────────────────────────

void ScreenAnim::update()
{
	if (frames.empty() || finished) return;

	// Apply per-frame velocity
	addPosX += static_cast<int>(velX);
	addPosY += static_cast<int>(velY);

	AnimFrame& f = frames[currentFrame];

	// time == -1 means hold this frame forever
	if (f.time == -1) return;

	// time == 0 means use the default (1 frame)
	int frameTime = f.time;
	if (frameTime == 0) frameTime = 1;

	frameTimer++;
	if (frameTimer >= frameTime) {
		frameTimer = 0;
		currentFrame++;
		if (currentFrame >= static_cast<int>(frames.size())) {
			// Loop back to loopstart
			currentFrame = loopstart;
		}
	}
}

// ── Screen anim: draw current frame ─────────────────────────────────────

void ScreenAnim::draw(int extraX, int extraY)
{
	if (frames.empty() || sffIndex < 0 || sffIndex >= static_cast<int>(g_sffFiles.size())) return;

	SffFile& sff = g_sffFiles[sffIndex];
	AnimFrame& f = frames[currentFrame];
	Sprite* sp = sff.getSprite(f.group, f.number);
	if (!sp || sp->rct.w <= 0 || sp->rct.h <= 0) return;

	int drawX = posX + addPosX + f.x + extraX;
	int drawY = posY + addPosY + f.y + extraY;

	// Apply scale
	int dw = static_cast<int>(sp->rct.w * scaleX);
	int dh = static_cast<int>(sp->rct.h * scaleY);

	ikemen::Rect dr = {drawX, drawY, dw, dh};
	ikemen::Rect sr = {0, 0, sp->rct.w, sp->rct.h};
	ikemen::Rect tile = {tileX, tileY, 0, 0};
	std::vector<int8_t> buf = sp->pluginbuf;

	LOG_DEBUG("ANIM", "animDraw(sffIdx=%d) sprite=(%d,%d) dst=(%d,%d %dx%d) pal=%p pxl.len=%zu",
	         sffIndex, f.group, f.number, dr.x, dr.y, dr.w, dr.h,
	         (void*)sp->colorPallet.data(), sp->pxl.size());

	// Determine alpha from frame or anim setting
	int alpha = 255;
	if (alphaSet) {
		if (srcAlpha == 255 && dstAlpha == 0) alpha = 255;
		else if (srcAlpha == 0 && dstAlpha == 255) alpha = 0;
		else alpha = srcAlpha;
	}

	// rasterxadd=0 means no raster rotation (correct for screenpack)
	// cx/ty=0 means no additional centering offset
	renderMugenZoom(dr, 0, 0, sp->pxl, sp->colorPallet,
	                static_cast<int16_t>(colorKey), sr, 0, 0, tile,
	                scaleX, scaleX, scaleY, 0.0f,
	                0, alpha, sp->rle, buf);
}

// ── Frame data parser ───────────────────────────────────────────────────

bool parseFrameData(const char* data, std::vector<AnimFrame>& frames)
{
	if (!data || data[0] == '\0') return false;

	std::string s(data);
	std::istringstream iss(s);
	std::string line;

	while (std::getline(iss, line)) {
		// Trim whitespace
		size_t start = line.find_first_not_of(" \t\r\n");
		if (start == std::string::npos) continue;
		line = line.substr(start);
		if (line.empty() || line[0] == ';') continue;  // skip comments

		AnimFrame f;
		// Parse: group,number, x,y, time[, priority, alpha]
		int grp = 0, num = 0, x = 0, y = 0, time = -1, pri = 0;
		char alphaStr[16] = "";

		int n = sscanf(line.c_str(), "%d,%d, %d,%d, %d, %d, %15s",
		               &grp, &num, &x, &y, &time, &pri, alphaStr);
		if (n < 5) continue;  // need at least group,number,x,y,time

		f.group = static_cast<int16_t>(grp);
		f.number = static_cast<int16_t>(num);
		f.x = static_cast<int16_t>(x);
		f.y = static_cast<int16_t>(y);
		f.time = time;
		f.priority = pri;
		if (n >= 7) f.alphaMode = alphaStr;

		frames.push_back(f);
	}
	return !frames.empty();
}

} // namespace ikemen
