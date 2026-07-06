// fight_service.cpp — Real implementations for fight.ssz (Phase 5).
// Full fight engine: Lifebar, Powerbar, Face, Name, Time, Combo, Round,
// WinIcon, and Fight with fight.def section parsers and step() lifecycle.
// Rendering (draw) methods deferred until sdlplugin/sff/font are converted.

#include "fight_service.hpp"
#include "common_service.hpp"
#include "sdlplugin_service.hpp"
#include "ssz_trace.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace ikemen::ssz_native {

// =========================================================================
// Module-level state
// =========================================================================
static FightState g_fight_state;

FightState& fight_get_state() { return g_fight_state; }

// =========================================================================
// Utility helpers
// =========================================================================

namespace {

std::string trim(const std::string& s) {
	size_t start = s.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) return {};
	size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end - start + 1);
}

std::string to_lower(const std::string& s) {
	std::string r = s;
	for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return r;
}

int parse_int(const std::string& s) { return std::atoi(trim(s).c_str()); }
float parse_float(const std::string& s) { return static_cast<float>(std::atof(trim(s).c_str())); }

// Parse a key=value line from section data.
bool parse_kv(const std::string& line, std::string& key, std::string& val) {
	size_t eq = line.find('=');
	if (eq == std::string::npos) return false;
	key = to_lower(trim(line.substr(0, eq)));
	val = trim(line.substr(eq + 1));
	return true;
}

// Collect section body lines into a map of key->value.
// The prefix filters keys (e.g., "bg0." strips the prefix).
using SectionMap = std::vector<std::pair<std::string, std::string>>;
SectionMap parse_section(const std::string& body, const std::string& prefix = "") {
	SectionMap result;
	std::istringstream stream(body);
	std::string line;
	while (std::getline(stream, line)) {
		std::string trimmed = trim(line);
		if (trimmed.empty() || trimmed[0] == ';') continue;
		std::string key, val;
		if (!parse_kv(trimmed, key, val)) continue;
		// Apply prefix filter
		if (!prefix.empty()) {
			if (key.find(prefix) != 0) continue;
			key = key.substr(prefix.size());
		}
		result.push_back({key, val});
	}
	return result;
}

// Extract section body from lines starting at index i, until next section or end.
std::string extract_section_body(const std::vector<std::string>& lines, size_t& i) {
	std::string body;
	i++; // skip section header
	while (i < lines.size()) {
		const auto& line = lines[i];
		if (!line.empty() && line[0] == '[') { i--; break; }
		if (!body.empty()) body += "\n";
		body += line;
		i++;
	}
	return body;
}

} // anonymous namespace

// =========================================================================
// LifePowerData
// =========================================================================
void LifePowerData::set(float life, float power, int level) {
	l = life; p = power; lv = level;
}

// =========================================================================
// ActionListData
// =========================================================================
int ActionListData::getAction(int no) {
	for (size_t i = 0; i < actionList.size(); i++)
		if (actionList[i] == no) return static_cast<int>(i);
	return -1;
}
int ActionListData::newAction(int no) {
	actionList.push_back(no);
	return static_cast<int>(actionList.size()) - 1;
}

// =========================================================================
// AnimFontSndData
// =========================================================================
void AnimFontSndData::read(const std::string& prefix, const std::string& sc, ActionListData& al) {
	auto kv = parse_section(sc, prefix);
	for (auto& [key, val] : kv) {
		if (key == "sndg") sndg = parse_int(val);
		else if (key == "sndi") sndi = parse_int(val);
		else if (key == "fontn") fontn = parse_int(val);
		else if (key == "fontb") fontb = parse_int(val);
		else if (key == "fonta") fonta = parse_int(val);
		else if (key == "text") text = val;
		else if (key == "displaytime") curtime = parse_int(val);
		// anim read deferred (needs sff/sprite lookup)
	}
	(void)al;
}
void AnimFontSndData::action() { if (curtime > 0) curtime--; }
void AnimFontSndData::draw(int layerno) { (void)layerno; }
bool AnimFontSndData::noSound() { return sndg < 0; }
bool AnimFontSndData::noDisplay() { return fontn < 0; }
bool AnimFontSndData::end(int dt) { return curtime > dt; }
void AnimFontSndData::reset() { *this = AnimFontSndData{}; }

// =========================================================================
// LifebarData
// =========================================================================
void LifebarData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "range.x") range_xz = parse_int(val);   // "range.x" -> range_xz
		else if (key == prefix + "range.y") range_xm = parse_int(val);
		else if (key == prefix + "range.z") range_xz = parse_int(val);   // SSZ uses .x, .y, .z
		else if (key == prefix + "range.x.z") range_xz = parse_int(val); // direct match
		else if (key == prefix + "range.x.m") range_xm = parse_int(val);
	}
	(void)range_xz; (void)range_xm; // used by draw
}
void LifebarData::step(float life, bool hit) {
	oldlife = midlife;
	midlife = life;
	if (hit) mlifetime = 60;
	if (mlifetime > 0) mlifetime--;
	midlifelim = midlife;
}
void LifebarData::bgDraw(int layerno) {
	// SSZ: render bg0, bg1, bg2 backgrounds for this lifebar at the given layer
	// These are rendered at full size (no life-based clipping)
	const auto& cd = common_get_state();
	
	// Helper: render an AnimData background layer
	auto drawLayer = [&](AnimData& anim, AnimData& lay) {
		(void)lay;
		if (!anim.spr) anim.updateSprite();
		if (!anim.spr) return;
		FrameData* frame = anim.drawFrame();
		if (!frame) return;
		
		SdlRect sr, dr, tile;
		sr.set(anim.spr->rct_x, anim.spr->rct_y, anim.spr->rct_w, anim.spr->rct_h);
		dr.set(0, 0, cd.GameWidth, cd.GameHeight);
		tile.set(0, 0, 0, 0);
		
		float drawX = (static_cast<float>(posx) + static_cast<float>(cd.GameWidth - 320) / 2.0f) * cd.WidthScale;
		float drawY = (static_cast<float>(posy) + static_cast<float>(cd.GameHeight - 240)) * cd.HeightScale;

		std::vector<int8_t> pb;
		pb.reserve(1024);
		renderMugenZoom(dr, 0.0f, 0.0f, anim.spr->pxl, anim.spr->colorPallet,
			-1, sr, -drawX, -drawY, tile,
			cd.WidthScale, cd.WidthScale, cd.HeightScale,
			0.0f, 0u, 256, anim.spr->rle, pb);
	};
	
	(void)layerno; // Layer check deferred (FightLayoutData not wired for sub-layers)
	drawLayer(bg0, bg0_lay);
	drawLayer(bg1, bg1_lay);
	drawLayer(bg2, bg2_lay);
}

void LifebarData::draw(int layerno, float life) {
	SSZ_TRACE_CAT(TRACE_SYS, "LifebarData::draw");
	const auto& cd = common_get_state();
	
	// SSZ: compute two rects — lrct (current-life portion) and mrct (max-life remainder)
	// laydraw renders mid layer into mrct (the "missing" life portion) and
	// front layer into lrct (the "current" life portion).
	
	float baseX = static_cast<float>(posx) + static_cast<float>(cd.GameWidth - 320) / 2.0f;
	float baseY = static_cast<float>(posy) + static_cast<float>(cd.GameHeight - 240);
	
	// Compute width in pixels matching SSZ setLifeWidth logic
	int rangeWidth = (range_xz < range_xm) ? (range_xm - range_xz + 1) : (range_xz - range_xm + 1);
	float fullWidth = static_cast<float>(rangeWidth) * cd.WidthScale;
	
	// Current-life rect (lrct)
	float currentLife = mid.spr ? (life < midlife ? life : midlife) : midlife;
	float lrctW = fullWidth * currentLife;
	float lrctX;
	if (range_xz < range_xm) {
		lrctX = (baseX + static_cast<float>(range_xz)) * cd.WidthScale;
	} else {
		lrctX = (baseX + static_cast<float>(range_xz + 1)) * cd.WidthScale - lrctW;
	}
	
	// Max-life remainder rect (mrct) — the part of the bar that has no current life
	float mrctW = fullWidth * (1.0f - currentLife);
	float mrctX;
	if (range_xz < range_xm) {
		mrctX = lrctX + lrctW;
	} else {
		mrctX = (baseX + static_cast<float>(range_xz)) * cd.WidthScale - lrctW;
	}
	if (mrctW > fullWidth - lrctW) mrctW = fullWidth - lrctW;
	
	// Render mid layer into the "missing" portion (mrct)
	auto drawClipped = [&](AnimData& anim, const SdlRect& clipRect, float xOff, float yOff) {
		if (!anim.spr) anim.updateSprite();
		if (!anim.spr) return;
		FrameData* frame = anim.drawFrame();
		if (!frame) return;
		
		SdlRect sr, tile;
		sr.set(anim.spr->rct_x, anim.spr->rct_y, anim.spr->rct_w, anim.spr->rct_h);
		tile.set(0, 0, 0, 0);
		
		std::vector<int8_t> pb;
		pb.reserve(1024);
		renderMugenZoom(clipRect, 0.0f, 0.0f, anim.spr->pxl, anim.spr->colorPallet,
			-1, sr, xOff, yOff, tile,
			cd.WidthScale, cd.WidthScale, cd.HeightScale,
			0.0f, 0u, 256, anim.spr->rle, pb);
	};
	
	// Draw mid layer (missing-life portion)
	SdlRect mrct;
	mrct.set(static_cast<int>(mrctX), static_cast<int>(baseY * cd.HeightScale),
		static_cast<int>(mrctW), cd.GameHeight);
	drawClipped(mid, mrct, 0.0f, 0.0f);
	
	// Draw front layer (current-life portion)
	SdlRect lrct;
	lrct.set(static_cast<int>(lrctX), static_cast<int>(baseY * cd.HeightScale),
		static_cast<int>(lrctW), cd.GameHeight);
	drawClipped(front, lrct, 0.0f, 0.0f);
	
	(void)layerno;
}

void LifebarData::reset() { *this = LifebarData{}; }

// =========================================================================
// PowerbarData
// =========================================================================
void PowerbarData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "range.x.z") range_xz = parse_int(val);
		else if (key == prefix + "range.x.m") range_xm = parse_int(val);
		else if (key == prefix + "counter.fontn") counter_fontn = parse_int(val);
		else if (key == prefix + "counter.fontb") counter_fontb = parse_int(val);
		else if (key == prefix + "counter.fonta") counter_fonta = parse_int(val);
	}
}
void PowerbarData::step(float power, int level) {
	midpower = power;
	prevlevel = level;
}
void PowerbarData::bgDraw(int layerno) {
	const auto& cd = common_get_state();
	
	auto drawLayer = [&](AnimData& anim) {
		if (!anim.spr) anim.updateSprite();
		if (!anim.spr) return;
		FrameData* frame = anim.drawFrame();
		if (!frame) return;
		
		SdlRect sr, dr, tile;
		sr.set(anim.spr->rct_x, anim.spr->rct_y, anim.spr->rct_w, anim.spr->rct_h);
		dr.set(0, 0, cd.GameWidth, cd.GameHeight);
		tile.set(0, 0, 0, 0);
		
		float drawX = (static_cast<float>(posx) + static_cast<float>(cd.GameWidth - 320) / 2.0f) * cd.WidthScale;
		float drawY = (static_cast<float>(posy) + static_cast<float>(cd.GameHeight - 240)) * cd.HeightScale;
		
		std::vector<int8_t> pb;
		pb.reserve(1024);
		renderMugenZoom(dr, 0.0f, 0.0f, anim.spr->pxl, anim.spr->colorPallet,
			-1, sr, -drawX, -drawY, tile,
			cd.WidthScale, cd.WidthScale, cd.HeightScale,
			0.0f, 0u, 256, anim.spr->rle, pb);
	};
	
	(void)layerno;
	drawLayer(bg0);
	drawLayer(bg1);
	drawLayer(bg2);
}

void PowerbarData::draw(int layerno, float power, int level) {
	SSZ_TRACE_CAT(TRACE_SYS, "PowerbarData::draw");
	const auto& cd = common_get_state();
	(void)level;
	
	float baseX = static_cast<float>(posx) + static_cast<float>(cd.GameWidth - 320) / 2.0f;
	float baseY = static_cast<float>(posy) + static_cast<float>(cd.GameHeight - 240);
	
	int rangeWidth = (range_xz < range_xm) ? (range_xm - range_xz + 1) : (range_xz - range_xm + 1);
	float fullWidth = static_cast<float>(rangeWidth) * cd.WidthScale;
	
	float pw = power;
	if (pw > 1.0f) pw = 1.0f;
	if (pw < 0.0f) pw = 0.0f;
	
	float lrctW = fullWidth * pw;
	float lrctX;
	if (range_xz < range_xm) {
		lrctX = (baseX + static_cast<float>(range_xz)) * cd.WidthScale;
	} else {
		lrctX = (baseX + static_cast<float>(range_xz + 1)) * cd.WidthScale - lrctW;
	}
	
	float mrctW = fullWidth * (1.0f - pw);
	float mrctX;
	if (range_xz < range_xm) {
		mrctX = lrctX + lrctW;
	} else {
		mrctX = (baseX + static_cast<float>(range_xz)) * cd.WidthScale - lrctW;
	}
	if (mrctW > fullWidth - lrctW) mrctW = fullWidth - lrctW;
	
	auto drawClipped = [&](AnimData& anim, const SdlRect& clipRect, float xOff, float yOff) {
		if (!anim.spr) anim.updateSprite();
		if (!anim.spr) return;
		FrameData* frame = anim.drawFrame();
		if (!frame) return;
		
		SdlRect sr, tile;
		sr.set(anim.spr->rct_x, anim.spr->rct_y, anim.spr->rct_w, anim.spr->rct_h);
		tile.set(0, 0, 0, 0);
		
		std::vector<int8_t> pb;
		pb.reserve(1024);
		renderMugenZoom(clipRect, 0.0f, 0.0f, anim.spr->pxl, anim.spr->colorPallet,
			-1, sr, xOff, yOff, tile,
			cd.WidthScale, cd.WidthScale, cd.HeightScale,
			0.0f, 0u, 256, anim.spr->rle, pb);
	};
	
	SdlRect mrct;
	mrct.set(static_cast<int>(mrctX), static_cast<int>(baseY * cd.HeightScale),
		static_cast<int>(mrctW), cd.GameHeight);
	drawClipped(mid, mrct, 0.0f, 0.0f);
	
	SdlRect lrct;
	lrct.set(static_cast<int>(lrctX), static_cast<int>(baseY * cd.HeightScale),
		static_cast<int>(lrctW), cd.GameHeight);
	drawClipped(front, lrct, 0.0f, 0.0f);
	
	(void)layerno;
}

void PowerbarData::reset() { *this = PowerbarData{}; }

// =========================================================================
// FaceData
// =========================================================================
void FaceData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "face.sprg") face_sprg = parse_int(val);
		else if (key == prefix + "face.spri") face_spri = parse_int(val);
		else if (key == prefix + "teammate.posx") teammate_posx = parse_int(val);
		else if (key == prefix + "teammate.posy") teammate_posy = parse_int(val);
		else if (key == prefix + "teammate.spacingx") teammate_spacingx = parse_int(val);
		else if (key == prefix + "teammate.spacingy") teammate_spacingy = parse_int(val);
		else if (key == prefix + "teammate.face.sprg") teammate_face_sprg = parse_int(val);
		else if (key == prefix + "teammate.face.spri") teammate_face_spri = parse_int(val);
	}
}
void FaceData::step() { /* face animation — deferred */ }
void FaceData::bgDraw(int layerno) {
	const auto& cd = common_get_state();
	
	// Render face background sprite
	auto drawBg = [&](AnimData& anim) {
		if (!anim.spr) anim.updateSprite();
		if (!anim.spr) return;
		FrameData* frame = anim.drawFrame();
		if (!frame) return;
		
		SdlRect sr, dr, tile;
		sr.set(anim.spr->rct_x, anim.spr->rct_y, anim.spr->rct_w, anim.spr->rct_h);
		dr.set(0, 0, cd.GameWidth, cd.GameHeight);
		tile.set(0, 0, 0, 0);
		
		float drawX = (static_cast<float>(posx) + static_cast<float>(cd.GameWidth - 320) / 2.0f) * cd.WidthScale;
		float drawY = (static_cast<float>(posy) + static_cast<float>(cd.GameHeight - 240)) * cd.HeightScale;
		
		std::vector<int8_t> pb;
		pb.reserve(1024);
		renderMugenZoom(dr, 0.0f, 0.0f, anim.spr->pxl, anim.spr->colorPallet,
			-1, sr, -drawX, -drawY, tile,
			cd.WidthScale, cd.WidthScale, cd.HeightScale,
			0.0f, 0u, 256, anim.spr->rle, pb);
	};
	
	(void)layerno;
	drawBg(bg);
}

void FaceData::draw(int layerno) {
	SSZ_TRACE_CAT(TRACE_SYS, "FaceData::draw");
	const auto& cd = common_get_state();
	(void)layerno;
	
	// Face portrait rendering — uses face_sprg/face_spri to look up a sprite
	// from the loaded SFF. For now, this is a simplified version that
	// renders the face background (which doubles as the portrait frame).
	// Full SSZ implementation also applies PalFX from facefx.
	
	// Face portrait sprite lookup deferred until SFF sprite group/no API
	// is wired for external sprite access. For now, draw the background only.
	
	// TODO: Load sprite from SFF using face_sprg/face_spri and render
	// with PalFX. SSZ stores the sprite in a temporary AnimData.
}

void FaceData::reset() {
	numko = 0; face_sprg = 0; face_spri = 0;
	posx = 0; posy = 0;
}

// =========================================================================
// NameData
// =========================================================================
void NameData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "fontn") name_fontn = parse_int(val);
		else if (key == prefix + "fontb") name_fontb = parse_int(val);
		else if (key == prefix + "fonta") name_fonta = parse_int(val);
	}
}
void NameData::step() {}
void NameData::bgDraw(int layerno) {
	const auto& cd = common_get_state();
	
	// Render name background sprite
	auto drawBg = [&](AnimData& anim) {
		if (!anim.spr) anim.updateSprite();
		if (!anim.spr) return;
		FrameData* frame = anim.drawFrame();
		if (!frame) return;
		
		SdlRect sr, dr, tile;
		sr.set(anim.spr->rct_x, anim.spr->rct_y, anim.spr->rct_w, anim.spr->rct_h);
		dr.set(0, 0, cd.GameWidth, cd.GameHeight);
		tile.set(0, 0, 0, 0);
		
		float drawX = (static_cast<float>(posx) + static_cast<float>(cd.GameWidth - 320) / 2.0f) * cd.WidthScale;
		float drawY = (static_cast<float>(posy) + static_cast<float>(cd.GameHeight - 240)) * cd.HeightScale;
		
		std::vector<int8_t> pb;
		pb.reserve(1024);
		renderMugenZoom(dr, 0.0f, 0.0f, anim.spr->pxl, anim.spr->colorPallet,
			-1, sr, -drawX, -drawY, tile,
			cd.WidthScale, cd.WidthScale, cd.HeightScale,
			0.0f, 0u, 256, anim.spr->rle, pb);
	};
	
	(void)layerno;
	drawBg(bg);
}

void NameData::draw(int layerno) {
	SSZ_TRACE_CAT(TRACE_SYS, "NameData::draw");
	(void)layerno;
	// Name text rendering requires font_service which is not yet converted.
	// The SSZ renders player name text using fnt.Font::drawText().
	// TODO: When font_service is wired, call:
	//   f.drawText(posx, posy, scaleX, scaleY, bank, alphaS, alphaD, scrrect, alignment, name);
}
void NameData::reset() { *this = NameData{}; }

// =========================================================================
// TimeData
// =========================================================================
void TimeData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "counter.fontn") counter_fontn = parse_int(val);
		else if (key == prefix + "counter.fontb") counter_fontb = parse_int(val);
		else if (key == prefix + "counter.fonta") counter_fonta = parse_int(val);
		else if (key == prefix + "framespercount") framespercount = parse_int(val);
	}
}
void TimeData::step() {}
void TimeData::bgDraw(int layerno) {
	const auto& cd = common_get_state();
	
	// Render timer background sprite
	if (!bg.spr) bg.updateSprite();
	if (!bg.spr) return;
	FrameData* frame = bg.drawFrame();
	if (!frame) return;
	
	SdlRect sr, dr, tile;
	sr.set(bg.spr->rct_x, bg.spr->rct_y, bg.spr->rct_w, bg.spr->rct_h);
	dr.set(0, 0, cd.GameWidth, cd.GameHeight);
	tile.set(0, 0, 0, 0);
	
	float drawX = (static_cast<float>(posx) + static_cast<float>(cd.GameWidth - 320) / 2.0f) * cd.WidthScale;
	float drawY = (static_cast<float>(posy) + static_cast<float>(cd.GameHeight - 240)) * cd.HeightScale;
	
	std::vector<int8_t> pb;
	pb.reserve(1024);
	renderMugenZoom(dr, 0.0f, 0.0f, bg.spr->pxl, bg.spr->colorPallet,
		-1, sr, -drawX, -drawY, tile,
		cd.WidthScale, cd.WidthScale, cd.HeightScale,
		0.0f, 0u, 256, bg.spr->rle, pb);
	
	(void)layerno;
}
void TimeData::draw(int layerno, int time) {
	SSZ_TRACE_CAT(TRACE_SYS, "TimeData::draw");
	(void)layerno; (void)time;
	// Timer counter text requires font_service.
	// TODO: When font_service is wired, render time as formatted text.
}
void TimeData::drawSimple(int layerno) {
	// No-font fallback — renders only the background sprite
	bgDraw(layerno);
}
void TimeData::reset() { *this = TimeData{}; }

// =========================================================================
// ComboData
// =========================================================================
void ComboData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "start.x") start_x = parse_float(val);
		else if (key == prefix + "counter.fontn") counter_fontn = parse_int(val);
		else if (key == prefix + "counter.fontb") counter_fontb = parse_int(val);
		else if (key == prefix + "counter.shake") counter_shake = parse_int(val);
		else if (key == prefix + "text.fontn") text_fontn = parse_int(val);
		else if (key == prefix + "text.fontb") text_fontb = parse_int(val);
		else if (key == prefix + "text.text") text_text = val;
		else if (key == prefix + "displaytime") displaytime = parse_int(val);
	}
}
void ComboData::step(int combo, int wt) {
	(void)wt;
	old = cur; cur = combo;
	if (combo > 1) resttime = displaytime;
	if (resttime > 0) resttime--;
	if (cur > 1) { counterX = start_x; shaketime = counter_shake; }
	if (shaketime > 0) shaketime--;
}
void ComboData::draw(int layerno) {
	SSZ_TRACE_CAT(TRACE_SYS, "ComboData::draw");
	(void)layerno;
	// Combo counter text requires font_service.
	// SSZ renders combo number and "Hits" text using fnt.Font::drawText().
	// TODO: When font_service is wired, render cur counter value and text_text.
}
void ComboData::reset() { *this = ComboData{}; }

// =========================================================================
// WinIconData
// =========================================================================
void WinIconData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "iconoffsetx") iconoffsetx = parse_int(val);
		else if (key == prefix + "iconoffsety") iconoffsety = parse_int(val);
		else if (key == prefix + "useiconupto") useiconupto = parse_int(val);
		else if (key == prefix + "counter.fontn") counter_fontn = parse_int(val);
		else if (key == prefix + "counter.fontb") counter_fontb = parse_int(val);
		else if (key == prefix + "counter.fonta") counter_fonta = parse_int(val);
	}
}
void WinIconData::step(int numwin) { (void)numwin; }
void WinIconData::draw(int layerno) {
	SSZ_TRACE_CAT(TRACE_SYS, "WinIconData::draw");
	const auto& cd = common_get_state();
	(void)layerno;
	
	// WinIcon rendering — render a sprite for each win
	// SSZ: for each win, draw icon.spr at posx + i*iconoffsetx, posy
	// with icon_lay layout. Counter text uses font.
	
	if (!icon.spr) icon.updateSprite();
	if (!icon.spr) return;
	FrameData* frame = icon.drawFrame();
	if (!frame) return;
	
	SdlRect sr, dr, tile;
	sr.set(icon.spr->rct_x, icon.spr->rct_y, icon.spr->rct_w, icon.spr->rct_h);
	dr.set(0, 0, cd.GameWidth, cd.GameHeight);
	tile.set(0, 0, 0, 0);
	
	float baseX = (static_cast<float>(posx) + static_cast<float>(cd.GameWidth - 320) / 2.0f) * cd.WidthScale;
	float baseY = (static_cast<float>(posy) + static_cast<float>(cd.GameHeight - 240)) * cd.HeightScale;
	
	// TODO: iterate actual win count once connected to CommonData
	for (int i = 0; i < 1; i++) {
		float drawX = baseX + static_cast<float>(i * iconoffsetx);
		float drawY = baseY + static_cast<float>(i * iconoffsety);
		
		std::vector<int8_t> pb;
		pb.reserve(1024);
		renderMugenZoom(dr, 0.0f, 0.0f, icon.spr->pxl, icon.spr->colorPallet,
			-1, sr, -drawX, -drawY, tile,
			cd.WidthScale, cd.WidthScale, cd.HeightScale,
			0.0f, 0u, 256, icon.spr->rle, pb);
	}
}
void WinIconData::add(int wt) { (void)wt; }
void WinIconData::reset() { *this = WinIconData{}; }
void WinIconData::clear() { *this = WinIconData{}; }

// =========================================================================
// RoundData
// =========================================================================
void RoundData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "match.maxdrawgames") match_maxdrawgames = parse_int(val);
		else if (key == prefix + "start.waittime") start_waittime = parse_int(val);
		else if (key == prefix + "round.time") round_time = parse_int(val);
		else if (key == prefix + "round.sndtime") round_sndtime = parse_int(val);
		else if (key == prefix + "fight.time") fight_time = parse_int(val);
		else if (key == prefix + "fight.sndtime") fight_sndtime = parse_int(val);
		else if (key == prefix + "ctrl.time") ctrl_time = parse_int(val);
		else if (key == prefix + "ko.time") ko_time = parse_int(val);
		else if (key == prefix + "ko.sndtime") ko_sndtime = parse_int(val);
		else if (key == prefix + "slow.time") slow_time = parse_int(val);
		else if (key == prefix + "over.waittime") over_waittime = parse_int(val);
		else if (key == prefix + "over.hittime") over_hittime = parse_int(val);
		else if (key == prefix + "over.wintime") over_wintime = parse_int(val);
		else if (key == prefix + "over.time") over_time = parse_int(val);
		else if (key == prefix + "win.time") win_time = parse_int(val);
		else if (key == prefix + "win.sndtime") win_sndtime = parse_int(val);
	}
}
void RoundData::callFight() { calledFight = true; }
bool RoundData::act(KOTy ko) {
	(void)ko;
	// SSZ: state machine for round announcements (Round 1 → Fight → KO → Win)
	// Returns true when round should advance
	if (cur > 0) cur--;
	return false;
}
void RoundData::draw(int layerno, KOTy ko,
	const std::string* winnerNames, int nameCount)
{
	SSZ_TRACE_CAT(TRACE_SYS, "RoundData::draw");
	
	// Save/restore brightness is SSZ-only — not applicable natively.
	// SSZ: int ob = .com.brightness; .com.brightness = .cfg.Brightness;
	
	// Helper: draw an AnimFontSndData at the round position
	auto drawAnimFont = [&](AnimFontSndData& afsd, int ln) {
		(void)ln;
		// Draw the animation sprite (if present)
		if (afsd.anim && afsd.anim->spr) {
			const auto& cd = common_get_state();
			SdlRect sr, dr, tile;
			sr.set(afsd.anim->spr->rct_x, afsd.anim->spr->rct_y,
				afsd.anim->spr->rct_w, afsd.anim->spr->rct_h);
			dr.set(0, 0, cd.GameWidth, cd.GameHeight);
			tile.set(0, 0, 0, 0);
			
			float drawX = (static_cast<float>(posx + afsd.lay.posx)
				+ static_cast<float>(cd.GameWidth - 320) / 2.0f) * cd.WidthScale;
			float drawY = (static_cast<float>(posy + afsd.lay.posy)
				+ static_cast<float>(cd.GameHeight - 240)) * cd.HeightScale;
			
			std::vector<int8_t> pb;
			pb.reserve(1024);
			renderMugenZoom(dr, 0.0f, 0.0f, afsd.anim->spr->pxl, afsd.anim->spr->colorPallet,
				-1, sr, -drawX, -drawY, tile,
				cd.WidthScale, cd.WidthScale, cd.HeightScale,
				0.0f, 0u, 256, afsd.anim->spr->rle, pb);
		}
		// TODO: When font_service is wired, render text:
		//   f.drawText(posx + lay.posx, posy + lay.posy, scale, scale,
		//              fontb, ..., fontn, text);
	};
	
	// State machine matching SSZ:
	// cur=0: Round announcement ("Round 1", etc.)
	// cur=1: "Fight!"
	// cur=2: KO/TimeOver + Winner
	
	switch (cur) {
	case 0:
		if (wt >= 0) break;
		// Draw round number text ("Round X")
		// SSZ uses round_default or per-round sprite based on .com.round
		drawAnimFont(round_default, layerno);
		break;
		
	case 1:
		if (wt >= 0) break;
		// Draw "Fight!"
		drawAnimFont(fight, layerno);
		break;
		
	case 2:
		if (ko == KOTy::None) break; // No KO yet — don't draw anything
		// Draw KO/TimeOver/DKO
		switch (ko) {
		case KOTy::Ko:
			drawAnimFont(this->ko, layerno);
			break;
		case KOTy::DoubleKO:
			drawAnimFont(this->dko, layerno);
			break;
		default:
			drawAnimFont(this->to, layerno);
			break;
		}
		// Draw winner text (when wt2 has expired)
		if (wt2 < 0 && winnerNames) {
			// SSZ: branches on draw type and number of winners
			if (ko == KOTy::DoubleKO) {
				drawAnimFont(this->drawn, layerno);
			} else if (nameCount >= 2) {
				drawAnimFont(this->win2, layerno);
			} else if (nameCount >= 1) {
				drawAnimFont(this->win, layerno);
			}
		}
		break;
	}
	
	(void)layerno;
}

void RoundData::reset() { *this = RoundData{}; calledFight = false; cur = 0; }

// =========================================================================
// DisplayTextData
// =========================================================================
void DisplayTextData::read(const std::string& prefix, const std::string& sc) {
	auto kv = parse_section(sc);
	for (auto& [key, val] : kv) {
		if (key == prefix + "posx") posx = parse_int(val);
		else if (key == prefix + "posy") posy = parse_int(val);
		else if (key == prefix + "fontn") text_fontn = parse_int(val);
		else if (key == prefix + "fontb") text_fontb = parse_int(val);
		else if (key == prefix + "fonta") text_fonta = parse_int(val);
		else if (key == prefix + "text") text_text = val;
	}
}
void DisplayTextData::step() {}
void DisplayTextData::bgDraw(int layerno) { (void)layerno; }
void DisplayTextData::draw(int layerno) { (void)layerno; }
void DisplayTextData::reset() { *this = DisplayTextData{}; }

// =========================================================================
// FightData
// =========================================================================

void FightData::load(const std::string& defPath) {
	SSZ_TRACE_CAT(TRACE_SYS, "FightData::load");
	def = defPath;

	// Read fight.def
	std::string buf = common_load_text(defPath, false);
	if (buf.empty()) return;

	auto lines = common_split_lines(buf);

	// Parse sections
	for (size_t i = 0; i < lines.size(); i++) {
		const auto& line = lines[i];
		std::string trimmed = trim(line);
		if (trimmed.empty() || trimmed[0] == ';') continue;
		if (trimmed[0] != '[') continue;

		// Section header
		size_t close = trimmed.find(']');
		if (close == std::string::npos) continue;
		std::string sectionName = to_lower(trimmed.substr(1, close - 1));
		std::string body = extract_section_body(lines, i);

		// Route to sub-component parsers based on section name
		if (sectionName.find("lifebar") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos || sectionName.find("2") != std::string::npos) ? 1 : 0;
			lifebar[p].read("", body);
		} else if (sectionName.find("powerbar") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos || sectionName.find("2") != std::string::npos) ? 1 : 0;
			powerbar[p].read("", body);
		} else if (sectionName.find("face") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos) ? 1 : 0;
			face[p].read("", body);
		} else if (sectionName.find("name") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos) ? 1 : 0;
			name[p].read("", body);
		} else if (sectionName == "time" || sectionName == "timer") {
			time.read("", body);
		} else if (sectionName.find("combo") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos) ? 1 : 0;
			combo[p].read("", body);
		} else if (sectionName.find("winicon") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos) ? 1 : 0;
			winicon[p].read("", body);
		} else if (sectionName == "round" || sectionName == "rounds") {
			round.read("", body);
		} else if (sectionName.find("wincount") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos) ? 1 : 0;
			wincount[p].read("", body);
		} else if (sectionName.find("score") != std::string::npos) {
			int p = (sectionName.find("p2") != std::string::npos) ? 1 : 0;
			score[p].read("", body);
		}
		// Other sections: match, ailevel, gamemode, reward, etc.
	}

	// Initialize per-player state
	for (auto& lp : lifePower) {
		lp.l = 1.0f; lp.p = 0.0f; lp.lv = 0;
	}
	for (auto& f : face) f.reset();
}

void FightData::step(int& tm, LifePowerData& life0, LifePowerData& life1, bool& hit, int& combo) {
	SSZ_TRACE_CAT(TRACE_SYS, "FightData::step");
	// Update per-player life/power from external state
	lifePower[0] = life0;
	lifePower[1] = life1;
	if (tm >= 0) tm--;

	// Step sub-components
	lifebar[0].step(lifePower[0].l, hit);
	lifebar[1].step(lifePower[1].l, hit);
	powerbar[0].step(lifePower[0].p, lifePower[0].lv);
	powerbar[1].step(lifePower[1].p, lifePower[1].lv);
	face[0].step();
	face[1].step();
	name[0].step();
	name[1].step();
	time.step();
	this->combo[0].step(combo, 0);
	this->combo[1].step(combo, 0);
	winicon[0].step(0);
	winicon[1].step(0);
	round.act(KOTy::None);
}

void FightData::draw(int layerno, LifePowerData* life, int lifeCount,
	const std::string* names, int nameCount,
	bool nbd, int superplayer)
{
	SSZ_TRACE_CAT(TRACE_SYS, "FightData::draw");
	
	(void)lifeCount;
	(void)nameCount;
	(void)superplayer;
	
	// SSZ: statusDraw / lifebarDisplay gate — checked at start
	// For native, these would check CommonData flags.
	
	if (!nbd) {
		// ── Lifebars ──
		// P1 lifebars (even indices in SSZ)
		lifebar[0].bgDraw(layerno);
		lifebar[0].draw(layerno, life ? life[0].l : 1.0f);
		// P2 lifebars (odd indices in SSZ)
		lifebar[1].bgDraw(layerno);
		lifebar[1].draw(layerno, life ? life[1].l : 1.0f);
		
		// ── Powerbars ──
		powerbar[0].bgDraw(layerno);
		powerbar[0].draw(layerno, life ? life[0].p : 0.0f, life ? life[0].lv : 0);
		powerbar[1].bgDraw(layerno);
		powerbar[1].draw(layerno, life ? life[1].p : 0.0f, life ? life[1].lv : 0);
		
		// ── Faces (reverse order for z-ordering) ──
		face[0].bgDraw(layerno);
		face[1].bgDraw(layerno);
		face[0].draw(layerno);
		face[1].draw(layerno);
		
		// ── Names ──
		name[0].bgDraw(layerno);
		name[1].bgDraw(layerno);
		name[0].draw(layerno);
		name[1].draw(layerno);
		
		// ── Timer ──
		time.bgDraw(layerno);
		time.drawSimple(layerno);
		
		// ── Win icons ──
		winicon[0].draw(layerno);
		winicon[1].draw(layerno);
		
		// ── Display text sections ──
		// These are font-only elements (wincount, timer, countdown, score,
		// match no, AI level, game mode, reward, tourney, abyss, etc.)
		// All require font_service which is not yet converted.
		// SSZ passes formatted text + shared font ref to DisplayTextData::draw().
		// When font_service is wired:
		//   wincount[0].draw(layerno);
		//   wincount[1].draw(layerno);
		//   timer[0].draw(layerno);  timer[1].draw(layerno);
		//   countdown[0].draw(layerno);  countdown[1].draw(layerno);
		//   score[0].draw(layerno);  score[1].draw(layerno);
		//   match.draw(layerno);
		//   ailevel.draw(layerno);
		//   gamemode.draw(layerno);
		//   reward.draw(layerno);
		//   tourneystate.draw(layerno);
		//   matchstowin.draw(layerno);
		//   abyssdepth.draw(layerno);
		//   abyssreward.draw(layerno);
		//   nickname.draw(layerno);
	}
	
	// ── Combo counter (always drawn, even when nbd) ──
	combo[0].draw(layerno);
	combo[1].draw(layerno);
}

void FightData::clear() {
	SSZ_TRACE_CAT(TRACE_SYS, "FightData::clear");
	def.clear();
	for (auto& lp : lifePower) { lp.l = 1.0f; lp.p = 0.0f; lp.lv = 0; }
	lifebar[0].reset(); lifebar[1].reset();
	powerbar[0].reset(); powerbar[1].reset();
	face[0].reset(); face[1].reset();
	name[0].reset(); name[1].reset();
	time.reset();
	combo[0].reset(); combo[1].reset();
	winicon[0].reset(); winicon[1].reset();
	round.reset();
}

void FightData::reset() {
	SSZ_TRACE_CAT(TRACE_SYS, "FightData::reset");
	for (auto& lp : lifePower) { lp.l = 1.0f; lp.p = 0.0f; }
	lifebar[0].reset(); lifebar[1].reset();
	powerbar[0].reset(); powerbar[1].reset();
	face[0].reset(); face[1].reset();
	name[0].reset(); name[1].reset();
	time.reset();
	combo[0].reset(); combo[1].reset();
	winicon[0].reset(); winicon[1].reset();
	round.reset();
}

// =========================================================================
// Module-level API
// =========================================================================

void fight_init() {
	SSZ_TRACE_CAT(TRACE_SYS, "fight_init");
	g_fight_state = FightState{};
	g_fight_state.initialized = true;
}

void fight_laydraw(int ln) {
	SSZ_TRACE_CAT(TRACE_SYS, "fight_laydraw");
	(void)ln;
}
void fight_laytext(int ln) {
	SSZ_TRACE_CAT(TRACE_SYS, "fight_laytext");
	(void)ln;
}
void fight_layspr(int ln) {
	SSZ_TRACE_CAT(TRACE_SYS, "fight_layspr");
	(void)ln;
}
void fight_read_spr(AnimData& a, const std::string& data) {
	SSZ_TRACE_CAT(TRACE_SYS, "fight_read_spr");
	(void)a; (void)data;
}
void fight_read_anim(AnimData& a, const std::string& data) {
	SSZ_TRACE_CAT(TRACE_SYS, "fight_read_anim");
	(void)a; (void)data;
}

} // namespace ikemen::ssz_native
