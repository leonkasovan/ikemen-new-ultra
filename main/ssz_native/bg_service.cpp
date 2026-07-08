// bg_service.cpp — Real implementations matching ssz_script/ssz/bg.ssz
//
// Phase 5: BGAction, Action, utility functions, and data structures.
// BackGround section parsing and rendering deferred until common_service's
// SectionData and sdlplugin are converted.

#include "bg_service.hpp"
#include "common_service.hpp"
#include "sdlplugin_service.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace ikemen::ssz_native {

// =========================================================================
// Module-level state
// =========================================================================

static BgState g_bg_state;

BgState& bg_get_state() {
	return g_bg_state;
}

// =========================================================================
// Utility functions
// =========================================================================

std::vector<std::string> bg_split_params(const std::string& paramStr) {
	std::vector<std::string> params;
	std::string tmp = paramStr;
	while (true) {
		// Trim whitespace
		size_t start = tmp.find_first_not_of(" \t\r\n");
		if (start == std::string::npos) break;
		tmp = tmp.substr(start);

		// Find comma or blank
		size_t cidx = tmp.find(',');
		size_t bidx = tmp.find_first_of(" \t");

		if (bidx != std::string::npos && (cidx == std::string::npos || bidx < cidx)) {
			// Blank before comma — take up to blank
			params.push_back(tmp.substr(0, bidx));
			break;
		}

		// Take up to comma
		std::string token;
		if (cidx != std::string::npos) {
			token = tmp.substr(0, cidx);
			tmp = tmp.substr(cidx + 1);
		} else {
			token = tmp;
			tmp.clear();
		}
		// Trim the token
		size_t ts = token.find_first_not_of(" \t\r\n");
		size_t te = token.find_last_not_of(" \t\r\n");
		if (ts != std::string::npos)
			token = token.substr(ts, te - ts + 1);
		else
			token.clear();
		params.push_back(token);

		if (cidx == std::string::npos) break;
	}
	return params;
}

template<>
std::vector<int> bg_cta<int>(const std::string& csv) {
	auto spl = bg_split_params(csv);
	std::vector<int> ary(spl.size());
	for (size_t i = 0; i < spl.size(); i++) {
		ary[i] = std::atoi(spl[i].c_str());
	}
	return ary;
}

template<>
std::vector<float> bg_cta<float>(const std::string& csv) {
	auto spl = bg_split_params(csv);
	std::vector<float> ary(spl.size());
	for (size_t i = 0; i < spl.size(); i++) {
		ary[i] = static_cast<float>(std::atof(spl[i].c_str()));
	}
	return ary;
}

template<>
std::vector<short> bg_cta<short>(const std::string& csv) {
	auto spl = bg_split_params(csv);
	std::vector<short> ary(spl.size());
	for (size_t i = 0; i < spl.size(); i++) {
		ary[i] = static_cast<short>(std::atoi(spl[i].c_str()));
	}
	return ary;
}

template<>
std::vector<uint16_t> bg_cta<uint16_t>(const std::string& csv) {
	auto spl = bg_split_params(csv);
	std::vector<uint16_t> ary(spl.size());
	for (size_t i = 0; i < spl.size(); i++) {
		ary[i] = static_cast<uint16_t>(std::atoi(spl[i].c_str()));
	}
	return ary;
}

template void bg_read_pair(int& x, int& y, const std::string& data);
template void bg_read_pair(float& x, float& y, const std::string& data);
template void bg_read_pair(short& x, short& y, const std::string& data);
template void bg_read_pair(uint16_t& x, uint16_t& y, const std::string& data);

template<typename T>
void bg_read_pair(T& x, T& y, const std::string& data) {
	auto spl = bg_split_params(data);
	if (!spl.empty() && !spl[0].empty()) {
		if constexpr (std::is_same_v<T, float>)
			x = static_cast<T>(std::atof(spl[0].c_str()));
		else
			x = static_cast<T>(std::atoi(spl[0].c_str()));
	}
	if (spl.size() >= 2 && !spl[1].empty()) {
		if constexpr (std::is_same_v<T, float>)
			y = static_cast<T>(std::atof(spl[1].c_str()));
		else
			y = static_cast<T>(std::atoi(spl[1].c_str()));
	}
}

// =========================================================================
// BGActionData
// =========================================================================

void BGActionData::clear() {
	xoffset = 0.0f; yoffset = 0.0f;
	sinxoffset = 0.0f; sinyoffset = 0.0f;
	x = 0.0f; y = 0.0f; vx = 0.0f; vy = 0.0f;
	xradius = 0.0f; yradius = 0.0f;
	sinxtime = 0; sinytime = 0;
	sinxlooptime = 0; sinylooptime = 0;
}

void BGActionData::action() {
	x += vx;

	// Sinusoidal X
	if (sinxlooptime > 0) {
		sinxoffset = xradius * std::sin(2.0 * kBgPi * static_cast<double>(sinxtime) / static_cast<double>(sinxlooptime));
		sinxtime++;
		if (sinxtime >= sinxlooptime) sinxtime = 0;
	} else {
		sinxoffset = 0.0f;
	}
	xoffset = x + sinxoffset;

	// Y
	y += vy;

	// Sinusoidal Y
	if (sinylooptime > 0) {
		sinyoffset = yradius * std::sin(2.0 * kBgPi * static_cast<double>(sinytime) / static_cast<double>(sinylooptime));
		sinytime++;
		if (sinytime >= sinylooptime) sinytime = 0;
	} else {
		sinyoffset = 0.0f;
	}
	yoffset = y + sinyoffset;
}

// =========================================================================
// BgActionData
// =========================================================================

void BgActionData::read(const std::vector<std::string>& lines, int& i) {
	frames.clear();
	int oldLoopstart = loopstart;

	while (i < static_cast<int>(lines.size())) {
		const std::string& line = lines[i];

		// Stop at next section header
		if (!line.empty() && line[0] == '[') {
			i--;
			break;
		}

		// Strip comments
		std::string clean = line;
		size_t commentPos = clean.find(';');
		if (commentPos != std::string::npos)
			clean = clean.substr(0, commentPos);
		// Trim
		{
			size_t s = clean.find_first_not_of(" \t\r\n");
			if (s != std::string::npos) clean = clean.substr(s);
			else { i++; continue; }
			size_t e = clean.find_last_not_of(" \t\r\n");
			if (e != std::string::npos) clean = clean.substr(0, e + 1);
		}

		if (clean.empty()) { i++; continue; }

		// Check if line starts with a digit or minus (frame data)
		if (clean[0] >= '0' && clean[0] <= '9' || clean[0] == '-') {
			auto ary = bg_cta<int>(clean);
			if (ary.size() >= 5) {
				BgFrameData frame;
				frame.group = static_cast<short>(ary[0]);
				frame.number = static_cast<short>(ary[1]);
				frame.x = static_cast<short>(ary[2]);
				frame.y = static_cast<short>(ary[3]);
				frame.time = ary[4];
				frames.push_back(frame);
			}
		} else {
			// Check for "loopstart" directive
			std::string lower = clean;
			for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			if (lower.substr(0, 9) == "loopstart") {
				loopstart = static_cast<int>(frames.size());
			}
		}
		i++;
	}

	if (loopstart >= static_cast<int>(frames.size()))
		loopstart = oldLoopstart;
}

// =========================================================================
// BackGroundData
// =========================================================================

void BackGroundData::reset() {
	if (anim) anim->reset();
	bga.clear();
	bga.vx = startvx;
	bga.vy = startvy;
	bga.xradius = startxrad;
	bga.yradius = startyrad;
	bga.sinxtime = startsinxt;
	bga.sinytime = startsinyt;
	bga.sinxlooptime = startsinxlt;
	bga.sinylooptime = startsinylt;
}

void BackGroundData::read(SectionData& sc, BackGroundData* link) {
	// Section parsing deferred until SectionData has real get() method.
	// When wired, this will:
	//   1. Read "type" → set typ (0=normal, 1=anim, 2=parallax, 3=dummy)
	//   2. Read "layerno" → set toplayer
	//   3. Read "spriteno" → set anim frame
	//   4. Read "actionno" → set actionno
	//   5. Read "positionlink" → set positionlink, copy from link
	//   6. Read "start" → startx, starty
	//   7. Read "delta" → deltax, deltay
	//   8. Read "mask", "trans", "tile", "tilespacing" → anim settings
	//   9. Read "width", "xscale" → parallax settings
	//   10. Read "yscalestart", "yscaledelta" → parallax Y
	//   11. Read "window" → startrect
	//   12. Read "windowdelta" → windowdeltax/y
	//   13. Read "id" → id
	//   14. Read "velocity" → startvx/y
	//   15. Read "sin.x", "sin.y" → sinusoidal parameters
	(void)sc; (void)link;
}

void BackGroundData::setup() {
	// Setup deferred until anim system and SFF sprite lookup are wired.
	// When called, this will:
	//   1. Calculate xtscale/xbscale from sprite dimensions
	//   2. Set up animation frames from action or sprite
	//   3. Call anim->setup(sff)
}

void BackGroundData::draw(float x, float y, float scl, float bgscl,
	float localscl, float xscale, float yscale, float shakeY,
	const PalFXData* fx)
{
	// SSZ BackGround::draw() — parallax, zoom compensation, raster effects,
	// window clipping, and sprite rendering.
	if (!anim || !anim->spr) return;

	const auto& cd = common_get_state();

	// ── 1. Raster x-aspect ratio ──
	// SSZ: xras = (rasterxbspeed - rasterxtspeed) / rasterxtspeed
	float xras;
	if (rasterxtspeed != 0.0f) {
		xras = (rasterxbspeed - rasterxtspeed) / rasterxtspeed;
	} else {
		xras = 0.0f;
	}
	float xbs = xbscale;

	// ── 2. Horizontal zoom compensation ──
	// SSZ: dx = max(0.0, deltax * bgscl)
	float dx = std::max(0.0f, deltax * bgscl);
	// SSZ: sclx = max(0.0, scl + (1.0 - scl)*(1.0 - dx))
	float sclx = std::max(0.0f, scl + (1.0f - scl) * (1.0f - dx));

	if (sclx != 0.0f) {
		// SSZ: xbs *= max(0.0, scl + (1.0-scl)*(1.0 - dx*(xbs/xtscale))) / sclx
		float denom = 1.0f / sclx;
		float xbsFactor;
		if (xtscale != 0.0f) {
			xbsFactor = std::max(0.0f,
				scl + (1.0f - scl) * (1.0f - dx * (xbs / xtscale)));
		} else {
			xbsFactor = std::max(0.0f,
				scl + (1.0f - scl) * (1.0f - dx));
		}
		xbs *= xbsFactor * denom;

		// SSZ: tmp *= max(0.0, scl + (1.0-scl)*(1.0 - dx*(xras+1.0)))
		//       xras -= tmp - 1.0
		float tmp = denom * std::max(0.0f,
			scl + (1.0f - scl) * (1.0f - dx * (xras + 1.0f)));
		xras -= tmp - 1.0f;
		xbs *= tmp;
	}

	// ── 3. Combined x/y scale ──
	float lxscl = localscl * xscale;
	sclx *= lxscl;

	// SSZ: scly = max(0.0, scl + (1.0-scl)*(1.0 - max(0.0, deltay*bgscl))) * localscl
	float dy = std::max(0.0f, deltay * bgscl);
	float scly = std::max(0.0f,
		scl + (1.0f - scl) * (1.0f - dy)) * localscl;

	// ── 4. Background position ──
	// SSZ: x2 = startx + xofs - (x/xscale + camstartx)*deltax + bga.xoffset
	float x2 = startx + xofs
		- ((x / xscale) + camstartx) * deltax
		+ bga.xoffset;
	// SSZ: y2 = starty - (y/yscale)*deltay + bga.yoffset
	float y2 = starty
		- (y / yscale) * deltay
		+ bga.yoffset;

	// ── 5. Grid snapping (non-zoom mode) ──
	// SSZ: if !zoom, snap x2/y2 to bgscl grid under certain conditions
	if (!cd.cam.zoom) {
		if (rasterxbspeed == rasterxtspeed
			&& bga.sinxlooptime <= 0
			&& bga.sinxoffset == 0.0f)
		{
			x2 = std::floor(x2 / bgscl) * bgscl;
		}
		if (bga.sinylooptime <= 0 && bga.sinyoffset == 0.0f) {
			y2 = std::floor(y2 / bgscl) * bgscl;
		}
	}

	// ── 6. Y scale factor ──
	// SSZ: ys = (100.0 - y * yscaledelta) * bgscl / yscalestart
	float ys = (100.0f - y * yscaledelta) * bgscl / yscalestart;

	// ── 7. Apply bgscl to position ──
	x2 *= bgscl;
	// SSZ: y2 = y2*bgscl + ((GameHeight - shakeY) / scly - 240.0) / yscale
	if (scly != 0.0f && yscale != 0.0f) {
		y2 = y2 * bgscl
			+ ((static_cast<float>(cd.GameHeight) - shakeY) / scly - 240.0f)
			/ yscale;
	} else {
		y2 = y2 * bgscl;
	}
	scly *= yscale;

	// SSZ: lyscl = localscl * yscale (needed for both window clipping and raster x-add)
	float lyscl = localscl * yscale;

	// ── 8. Window clipping rect (SSZ: win_x/y/w/h → startrect) ──
	// The SSZ computes wsclx/wscly with zoom/delta formula, then transforms
	// the startrect into screen-space for the dest rect clipping.
	// Default values (-32768, -32768, 65535, 65535) mean "no clipping" —
	// in that case we pass nullptr and use the full screen.
	const SdlRect* pClipRect = nullptr;
	SdlRect clipRect;
	bool hasWindowClip = !(win_x == -32768 && win_y == -32768
		&& win_w == 65535 && win_h == 65535);
	if (hasWindowClip) {
		// SSZ: wsclx = max(0.0, scl + (1.0-scl)*(1.0 - max(0.0, windowdeltax*bgscl)))
		//        * bgscl * lxscl
		float wsclx = std::max(0.0f,
			scl + (1.0f - scl) * (1.0f - std::max(0.0f, windowdeltax * bgscl)))
			* bgscl * lxscl;
		// SSZ: wscly = max(0.0, scl + (1.0-scl)*(1.0 - max(0.0, windowdeltay*bgscl)))
		//        * bgscl * lyscl
		float wscly = std::max(0.0f,
			scl + (1.0f - scl) * (1.0f - std::max(0.0f, windowdeltay * bgscl)))
			* bgscl * lyscl;

		// SSZ: rect.x = floor((win_x - (x+camstartx)*windowdeltax) * WidthScale * wsclx)
		clipRect.x = static_cast<int>(std::floor(
			(static_cast<float>(win_x) - (x + camstartx) * windowdeltax)
			* cd.WidthScale * wsclx));
		// SSZ: rect.y = floor(((win_y - y*windowdeltay)*wscly - shakeY + (GameHeight-240))
		//        * HeightScale)
		clipRect.y = static_cast<int>(std::floor(
			((static_cast<float>(win_y) - y * windowdeltay) * wscly
				- shakeY + static_cast<float>(cd.GameHeight - 240))
			* cd.HeightScale));
		// SSZ: rect.w = ceil(win_w * WidthScale * wsclx)
		clipRect.w = static_cast<int>(std::ceil(
			static_cast<float>(win_w) * cd.WidthScale * wsclx));
		// SSZ: rect.h = ceil(win_h * HeightScale * wscly)
		clipRect.h = static_cast<int>(std::ceil(
			static_cast<float>(win_h) * cd.HeightScale * wscly));

		pClipRect = &clipRect;
	}

	// ── 9. Render via AnimData::draw() ──
	// SSZ: anim.draw(rect=, x2, y2, sclx, scly, xtscale*bgscl, xbs*bgscl, ys,
	//           xras * x2 / (ys*lyscl*(float)anim.spr.rct_h),
	//           GameWidth/2.0, fx, true)
	//
	// Native mapping (signature differs from SSZ):
	//   native: draw(alpha, x, y, xs, ys, xts, xbs, yss, rxadd, agl, trans, pal)
	//   SSZ:    draw(rect=, x2, y2, sclx, scly, xtscale*bgscl, xbs*bgscl, ys,
	//               rasterxadd, GameWidth/2.0, fx, oVer)

	// Compute rasterxadd for native (rxadd parameter)
	// SSZ: xras * x2 / (ys * lyscl * (float)anim.spr.rct_h)
	// lyscl is already computed above (section 7) = localscl * yscale
	float rasterxadd = 0.0f;
	if (ys != 0.0f && lyscl != 0.0f && anim->spr->rct_h > 0) {
		rasterxadd = xras * x2
			/ (ys * lyscl * static_cast<float>(anim->spr->rct_h));
	}

	anim->draw(
		256,                                      // alpha (full opacity)
		x2, y2,                                   // position
		sclx, scly,                               // base scales
		xtscale * bgscl,                          // xts (x top scale)
		xbs * bgscl,                              // xbs (x bottom scale)
		ys,                                       // yss (y scale)
		rasterxadd,                               // rxadd (raster x add)
		static_cast<float>(cd.GameWidth) / 2.0f,  // agl (angle/rotation)
		0,                                        // trans (blend mode, unused)
		fx,                                       // pal (PalFX — from stage or null)
		pClipRect                                 // clipRect (window clipping or null)
	);
}

// =========================================================================
// BGCtrlData
// =========================================================================

void BGCtrlData::read(SectionData& sc, int index) {
	idx = index;
	// BGCtrl section parsing deferred until SectionData has real get().
	// When wired, this will parse:
	//   "type" → typ (Anim, Visible, Enable, PosSet, PosAdd, SinX, SinY, VelSet, VelAdd)
	//   "time" → starttime, endtime, looptime
	//   "positionlink" → positionlink
	//   "value" → v1, v2, v3 (for non-xy types)
	//   "x", "y" → setx, sety, x, y (for xy types)
	(void)sc;
}

// =========================================================================
// ActiveCtrlList
// =========================================================================

void ActiveCtrlList::add(BGCtrlData* bgc) {
	auto* cell = new Cell();
	cell->bgc = bgc;

	if (top == nullptr) {
		top = cell;
		return;
	}

	if (bgc->idx < top->bgc->idx) {
		cell->next = top;
		top = cell;
		return;
	}

	Cell* tmp = top;
	while (tmp->next != nullptr) {
		if (bgc->idx < tmp->next->bgc->idx) {
			cell->next = tmp->next;
			tmp->next = cell;
			return;
		}
		tmp = tmp->next;
	}
	tmp->next = cell;
}

std::vector<BGCtrlData*> ActiveCtrlList::act() {
	std::vector<BGCtrlData*> endlist;
	Cell* tmp = top;
	Cell* prev = nullptr;

	while (tmp != nullptr) {
		// Process BGCtrl via s.bgCtrl(bgc) — deferred
		// For now just check if currenttime > endtime
		if (tmp->bgc->currenttime > tmp->bgc->endtime) {
			endlist.push_back(tmp->bgc);
			Cell* toDelete = tmp;
			if (prev == nullptr) {
				top = tmp->next;
				tmp = top;
			} else {
				prev->next = tmp->next;
				tmp = prev->next;
			}
			delete toDelete;
		} else {
			prev = tmp;
			tmp = tmp->next;
		}
	}
	return endlist;
}

void ActiveCtrlList::clear() {
	Cell* tmp = top;
	while (tmp != nullptr) {
		Cell* next = tmp->next;
		delete tmp;
		tmp = next;
	}
	top = nullptr;
}

// =========================================================================
// BGCTimeLine
// =========================================================================

void BGCTimeLine::add(BGCtrlData* bgc) {
	if (bgc->looptime >= 0 && bgc->endtime > bgc->looptime)
		bgc->endtime = bgc->looptime;

	if (bgc->starttime < 0 || bgc->starttime > bgc->endtime
		|| (bgc->looptime >= 0 && bgc->starttime >= bgc->looptime))
		return;

	int wtime = 0;
	if (bgc->currenttime != 0) {
		if (bgc->looptime < 0) return;
		wtime += bgc->looptime - bgc->currenttime;
	}
	wtime += bgc->starttime;
	bgc->currenttime = bgc->starttime;

	if (wtime < 0) {
		bgc->currenttime -= wtime;
		wtime = 0;
	}

	auto* newNode = new Node();
	newNode->waittime = wtime;
	newNode->bgcList.push_back(bgc);

	if (top == nullptr) {
		top = newNode;
		return;
	}

	// Find insertion point
	Node* tmp = top;
	Node* prev = nullptr;
	while (tmp != nullptr) {
		wtime -= tmp->waittime;
		if (wtime <= 0) break;
		prev = tmp;
		tmp = tmp->nextnode;
	}

	if (tmp == nullptr) {
		// Append at end
		if (prev == nullptr) {
			top = newNode;
		} else {
			// Find last node
			Node* last = top;
			while (last->nextnode != nullptr) last = last->nextnode;
			last->nextnode = newNode;
		}
	} else if (wtime == 0) {
		// Same time — add to existing node's list
		tmp->bgcList.push_back(bgc);
		delete newNode;
	} else {
		// Split: insert new node before tmp
		wtime += tmp->waittime;
		newNode->waittime = wtime;
		newNode->bgcList = tmp->bgcList;
		newNode->nextnode = tmp->nextnode;
		tmp->waittime -= wtime;
		tmp->bgcList.clear();
		tmp->bgcList.push_back(bgc);
		tmp->nextnode = newNode;
		if (prev == nullptr) {
			// top unchanged
		}
	}
}

template std::vector<BGCtrlData*> BGCTimeLine::step(BackGroundData& s);

template<typename T>
std::vector<BGCtrlData*> BGCTimeLine::step(T& s) {
	// Activate bgc events whose wait time has elapsed
	while (top != nullptr && top->waittime <= 0) {
		for (size_t i = 0; i < top->bgcList.size(); i++) {
			al.add(top->bgcList[i]);
		}
		Node* oldTop = top;
		top = top->nextnode;
		delete oldTop;
	}

	if (top != nullptr)
		top->waittime--;

	// Process active controls
	auto expired = al.act();

	// Re-add expired events that have looptime
	for (auto& bgc : expired) {
		add(bgc);
	}

	return expired;
}

void BGCTimeLine::clear() {
	Node* tmp = top;
	while (tmp != nullptr) {
		Node* next = tmp->nextnode;
		delete tmp;
		tmp = next;
	}
	top = nullptr;
	al.clear();
}

// =========================================================================
// Module-level API
// =========================================================================

void bg_init() {
	// Initialize the background module
}

} // namespace ikemen::ssz_native
