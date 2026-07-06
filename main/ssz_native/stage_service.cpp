// stage_service.cpp — Real implementations matching ssz_script/ssz/stage.ssz
//
// Phase 5: EnvShake, def file parsing, and stage lifecycle.
// Background rendering (bgDraw) and SFF loading deferred until
// bg_service and sff_service are converted.

#include "stage_service.hpp"
#include "common_service.hpp"
#include "bg_service.hpp"
#include "ssz_trace.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace ikemen::ssz_native {

// ── Internal helpers (used before their first call) ──

namespace {

std::string trim_str(const std::string& s) {
	const char* ws = " \t\r\n";
	size_t start = s.find_first_not_of(ws);
	if (start == std::string::npos) return {};
	size_t end = s.find_last_not_of(ws);
	return s.substr(start, end - start + 1);
}

std::string to_lower(std::string s) {
	for (auto& c : s)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return s;
}

} // anonymous namespace

// =========================================================================
// Module-level globals
// =========================================================================

static StageData g_stage;
static EnvShakeData g_env_shake;
static std::string g_bgmusic;

// =========================================================================
// EnvShakeData
// =========================================================================

void EnvShakeData::clear() {
	time = 0;
	freq = static_cast<float>(kStagePi) / 3.0f;
	ampl = -4;
	phase = NAN;
}

void EnvShakeData::setDefPhase() {
	if (!std::isnan(phase)) return;
	phase = (freq >= static_cast<float>(kStagePi) / 2.0f)
		? static_cast<float>(kStagePi) / 2.0f
		: 0.0f;
}

void EnvShakeData::next() {
	if (time <= 0) return;
	time--;
	phase += freq;
}

float EnvShakeData::getOffset() const {
	if (time <= 0) return 0.0f;
	return static_cast<float>(ampl) * 0.5f * std::sin(phase);
}

// =========================================================================
// StageData
// =========================================================================

void StageData::init() {
	p2.startx *= -1;
	p2.facing = -1;
	// cam.stg.clear() — done by common_service users
	localscl = NAN; // Will be set during load()
	clear();
}

std::string StageData::load(const std::string& defPath) {
	def = defPath;

	// ── Read and parse the .def file ──
	// SSZ: buf = .com.loadText(def, unicode);
	// We read without unicode detection for now (default false)
	std::string buf = common_load_text(defPath, false);
	if (buf.empty())
		return defPath + ":\r\nFailed to open file";

	// SSZ: lines = .com.splitLines(buf);
	//       .s.each!^/char?([void(^/char l=){l=.s.trim(l);}], lines);
	std::vector<std::string> lines = common_split_lines(buf);

	// Trim each line
	for (auto& line : lines) {
		// Trim leading whitespace
		size_t start = line.find_first_not_of(" \t\r\n");
		if (start != std::string::npos)
			line = line.substr(start);
		else
			line.clear();

		// Trim trailing whitespace
		size_t end = line.find_last_not_of(" \t\r\n");
		if (end != std::string::npos)
			line = line.substr(0, end + 1);
	}

	// Remove empty lines and comments (lines starting with ;)
	std::vector<std::string> cleaned;
	for (const auto& line : lines) {
		if (line.empty() || line[0] == ';')
			continue;
		cleaned.push_back(line);
	}
	lines = std::move(cleaned);

	// ── Parse sections ──
	// SSZ uses a two-pass approach:
	//   Pass 1: info, camera, playerinfo, scaling, bound, stageinfo,
	//           shadow, reflection, music, bgdef, bg
	//   Pass 2: bgctrldef, bgctrl
	//
	// Flags to ensure each section is processed only once (matching SSZ)
	bool infoflg = true, cameraflg = true, playerinfoflg = true;
	bool scalingflg = true, boundflg = true, stageinfoflg = true;
	bool shadowflg = true, reflectionflg = true, musicflg = true;
	bool bgdefflg = true;

	// Get a mutable reference to CommonData for camera updates
	CommonData& cd = common_get_state();

	// SSZ: unicode = false (default)
	bool unicode = false;

	// ── First pass: data sections ──
	for (size_t i = 0; i < lines.size(); i++) {
		const std::string& sec = lines[i];
		if (sec.empty() || sec[0] == '[') continue;

		// Extract section name (text before first space/tab)
		std::string secname = sec;
		{
			size_t pos = sec.find_first_of(" \t");
			if (pos != std::string::npos)
				secname = sec.substr(0, pos);
		}

		// Skip section headers and "begin " lines
		if (secname.empty()) continue;
		if (sec[0] == '[') continue;
		if (secname == "begin" || secname.substr(0, 6) == "begin ") continue;

		// Collect section body
		i++;
		std::vector<std::string> secLines;
		while (i < lines.size()) {
			const std::string& l = lines[i];
			if (l.empty()) { i++; continue; }
			if (l[0] == '[') break;
			if (l.find('=') != std::string::npos || l[0] == ';') {
				secLines.push_back(l);
				i++;
				continue;
			}
			// Check if this line starts a new section
			std::string nextName = l;
			{
				size_t pos = l.find_first_of(" \t");
				if (pos != std::string::npos)
					nextName = l.substr(0, pos);
			}
			// Common section names that terminate the current section
			if (nextName == "info" || nextName == "camera" ||
				nextName == "playerinfo" || nextName == "scaling" ||
				nextName == "bound" || nextName == "stageinfo" ||
				nextName == "shadow" || nextName == "reflection" ||
				nextName == "music" || nextName == "bgdef" ||
				nextName == "bgctrldef" || nextName == "bgctrl" ||
				nextName.substr(0, 2) == "bg" ||
				nextName.substr(0, 6) == "begin ") {
				i--;
				break;
			}
			secLines.push_back(l);
			i++;
		}
		i--; // Will be incremented by for loop

		// ── info section ──
		if (secname == "info" && infoflg) {
			infoflg = false;
			for (const auto& line : secLines) {
				size_t eq = line.find('=');
				if (eq == std::string::npos) continue;
				std::string key = line.substr(0, eq);
				std::string val = line.substr(eq + 1);
				// Trim whitespace from key and value
				key = trim_str(key);
				val = trim_str(val);

				if (key == "name") name = val;
				else if (key == "displayname") displayname = val;
				else if (key == "author") author = val;
			}
			if (displayname.empty()) displayname = name;
			// Convert to lowercase variants
			nameLow = to_lower(name);
			displaynameLow = to_lower(displayname);
			authorLow = to_lower(author);
		}

		// ── camera section ──
		else if (secname == "camera" && cameraflg) {
			cameraflg = false;
			for (const auto& line : secLines) {
				size_t eq = line.find('=');
				if (eq == std::string::npos) continue;
				std::string key = trim_str(line.substr(0, eq));
				std::string val = trim_str(line.substr(eq + 1));

				if (key == "startx") cd.cam.stg.startx = common_atoi(val);
				else if (key == "boundleft") cd.cam.stg.boundleft = common_atoi(val);
				else if (key == "boundright") cd.cam.stg.boundright = common_atoi(val);
				else if (key == "boundhigh") cd.cam.stg.boundhigh = common_atoi(val);
				else if (key == "verticalfollow") cd.cam.stg.verticalfollow = static_cast<float>(common_atof(val));
				else if (key == "tension") cd.cam.stg.tension = common_atoi(val);
				else if (key == "floortension") cd.cam.stg.floortension = common_atoi(val);
				else if (key == "overdrawlow") cd.cam.stg.overdrawlow = common_atoi(val);
			}
		}

		// ── playerinfo section ──
		else if (secname == "playerinfo" && playerinfoflg) {
			playerinfoflg = false;
			for (const auto& line : secLines) {
				size_t eq = line.find('=');
				if (eq == std::string::npos) continue;
				std::string key = trim_str(line.substr(0, eq));
				std::string val = trim_str(line.substr(eq + 1));

				int ival = common_atoi(val);
				if (key == "p1startx") p1.startx = ival;
				else if (key == "p1starty") p1.starty = ival;
				else if (key == "p2startx") p2.startx = ival;
				else if (key == "p2starty") p2.starty = ival;
				else if (key == "p3startx") p3.startx = ival;
				else if (key == "p3starty") p3.starty = ival;
				else if (key == "p4startx") p4.startx = ival;
				else if (key == "p4starty") p4.starty = ival;
				else if (key == "p1facing") p1.facing = ival;
				else if (key == "p2facing") p2.facing = ival;
				else if (key == "p3facing") p3.facing = ival;
				else if (key == "p4facing") p4.facing = ival;
				else if (key == "leftbound") leftbound = static_cast<float>(common_atof(val));
				else if (key == "rightbound") rightbound = static_cast<float>(common_atof(val));
			}
		}

		// ── scaling section ──
		else if (secname == "scaling" && scalingflg) {
			scalingflg = false;
			for (const auto& line : secLines) {
				size_t eq = line.find('=');
				if (eq == std::string::npos) continue;
				std::string key = trim_str(line.substr(0, eq));
				std::string val = trim_str(line.substr(eq + 1));
				if (key == "topscale")
					cd.cam.stg.ztopscale = static_cast<float>(common_atof(val));
					SSZ_TRACE_CAT(TRACE_SYS, ("scaling topscale -> cam.stg.ztopscale = " + std::to_string(cd.cam.stg.ztopscale)).c_str());
			}
		}

		// ── bound section ──
		else if (secname == "bound" && boundflg) {
			boundflg = false;
			for (const auto& line : secLines) {
				size_t eq = line.find('=');
				if (eq == std::string::npos) continue;
				std::string key = trim_str(line.substr(0, eq));
				std::string val = trim_str(line.substr(eq + 1));
				if (key == "screenleft") screenleft = common_atoi(val);
				else if (key == "screenright") screenright = common_atoi(val);
			}
		}

		// ── stageinfo section ──
		else if (secname == "stageinfo" && stageinfoflg) {
			stageinfoflg = false;
			for (const auto& line : secLines) {
				size_t eq = line.find('=');
				if (eq == std::string::npos) continue;
				std::string key = trim_str(line.substr(0, eq));
				std::string val = trim_str(line.substr(eq + 1));

				if (key == "zoffset") {
					cd.cam.stg.zoffset = common_atoi(val);
					SSZ_TRACE_CAT(TRACE_SYS, ("stageinfo zoffset -> cam.stg.zoffset = " + std::to_string(cd.cam.stg.zoffset)).c_str());
				}
				else if (key == "zoffsetlink") zoffsetlink = common_atoi(val);
				else if (key == "hires") hires = common_atoi(val) != 0;
				else if (key == "resetbg") resetbg = common_atoi(val) != 0;
				else if (key == "localcoord") {
					// Parse "w h" format
					size_t space = val.find(' ');
					if (space != std::string::npos) {
						cd.cam.stg.localw = common_atoi(val.substr(0, space));
						cd.cam.stg.localh = common_atoi(val.substr(space + 1));
					}
				}
				else if (key == "xscale") {
					xscale = static_cast<float>(common_atof(val));
					cd.cam.stg.xscale = xscale;
					SSZ_TRACE_CAT(TRACE_SYS, ("stageinfo xscale -> cam.stg.xscale = " + std::to_string(xscale)).c_str());
				} else if (key == "yscale") {
					yscale = static_cast<float>(common_atof(val));
					cd.cam.stg.yscale = yscale;
					SSZ_TRACE_CAT(TRACE_SYS, ("stageinfo yscale -> cam.stg.yscale = " + std::to_string(yscale)).c_str());
				}
			}
		}

		// ── shadow section ──
		else if (secname == "shadow" && shadowflg) {
			shadowflg = false;
			for (const auto& line : secLines) {
				size_t eq = line.find('=');
				if (eq == std::string::npos) continue;
				std::string key = trim_str(line.substr(0, eq));
				std::string val = trim_str(line.substr(eq + 1));

				if (key == "intensity") {
					int v = common_atoi(val);
					sdw.intensity = std::max(0, std::min(255, v));
				}
				else if (key == "color") {
					// Parse "r,g,b" format
					int r = 0, g = 0, b = 0;
					size_t c1 = val.find(',');
					if (c1 != std::string::npos) {
						r = std::max(0, std::min(255, common_atoi(val.substr(0, c1))));
						size_t c2 = val.find(',', c1 + 1);
						if (c2 != std::string::npos) {
							g = std::max(0, std::min(255, common_atoi(val.substr(c1 + 1, c2 - c1 - 1))));
							b = std::max(0, std::min(255, common_atoi(val.substr(c2 + 1))));
						} else {
							g = std::max(0, std::min(255, common_atoi(val.substr(c1 + 1))));
						}
					}
					sdw.color = static_cast<uint32_t>((r << 16) | (g << 8) | b);
				}
				else if (key == "yscale") sdw.yscale = static_cast<float>(common_atof(val));
				else if (key == "reflect") reflect = common_atoi(val) != 0;
				else if (key == "fade.range") {
					size_t dot = val.find('.');
					if (dot != std::string::npos) {
						sdw.fadeend = common_atoi(val.substr(0, dot));
						sdw.fadebgn = common_atoi(val.substr(dot + 1));
					}
				}
			}
		}

		// ── reflection section ──
		else if (secname == "reflection" && reflectionflg) {
			reflectionflg = false;
			for (const auto& line : secLines) {
				size_t eq = line.find('=');
				if (eq == std::string::npos) continue;
				std::string key = trim_str(line.substr(0, eq));
				std::string val = trim_str(line.substr(eq + 1));
				if (key == "intensity") {
					reflection = std::max(0, std::min(255, common_atoi(val)));
				}
			}
		}

		// ── music section ──
		else if (secname == "music" && musicflg) {
			musicflg = false;
			for (const auto& line : secLines) {
				size_t eq = line.find('=');
				if (eq == std::string::npos) continue;
				std::string key = trim_str(line.substr(0, eq));
				std::string val = trim_str(line.substr(eq + 1));
				if (key == "bgmusic") {
					bgmusic = common_read_file_name(val, unicode);
				}
			}
		}

		// ── bgdef section ──
		else if (secname == "bgdef" && bgdefflg) {
			bgdefflg = false;
			for (const auto& line : secLines) {
				size_t eq = line.find('=');
				if (eq == std::string::npos) continue;
				std::string key = trim_str(line.substr(0, eq));
				std::string val = trim_str(line.substr(eq + 1));
				if (key == "spr") spr = common_read_file_name(val, unicode);
				else if (key == "debugbg") debugbg = common_atoi(val) != 0;
			}
		}

		// ── bg sections ──
		else if (secname.size() >= 2 && secname.substr(0, 2) == "bg") {
			// Background parsing deferred until bg_service is converted.
			// In SSZ: bg(sc=, link=);
			// Track link for position-linked backgrounds
			static int bg_link = -1;
			(void)bg_link;
		}
	}

	// ── Second pass: bgctrldef / bgctrl sections ──
	// Deferred until bg_service is converted.

	// ── Post-processing ──
	if (!reflect) reflection = 0;

	// ── SFF loading ──
	// Deferred until sff_service is converted.
	// SSZ: sf.new(1); if (.com.loadFile(def, spr=) => error, ...) ret error;
	// For now, check if the sprite file exists
	if (!spr.empty()) {
		std::string loadResult = common_load_file(def, spr, nullptr);
		if (!loadResult.empty()) {
			return loadResult;
		}
	}

	// ── Final setup ──	localscl = static_cast<float>(cd.GameWidth) / static_cast<float>(cd.cam.stg.localw);
	cd.cam.stg.localscl = localscl;
	SSZ_TRACE_CAT(TRACE_SYS, ("StageData::load postproc localscl=" + std::to_string(localscl) + " cam.stg.xscale=" + std::to_string(cd.cam.stg.xscale) + " yscale=" + std::to_string(cd.cam.stg.yscale)).c_str());

	if (std::isnan(leftbound))
		leftbound = 1000.0f / localscl;
	leftbound *= localscl;

	if (std::isnan(rightbound))
		rightbound = 1000.0f / localscl;
	rightbound *= localscl;

	// Background setup deferred until bg_service converts.

	// Camera draw offset calculation deferred (needs bg setup).

	return {}; // Success
}

void StageData::action() {
	// envShake.next() is called each frame
	g_env_shake.next();

	// Background action stepping deferred until bg_service converts.
	// SSZ: bgctl.step!self?(=);
	//      bga.action();
	//      for each bg: bg[i].bga.action(), if active bg[i].anim.action()
}

void StageData::bgDraw(bool t, float x, float y, float scl) {
	SSZ_TRACE_CAT(TRACE_SYS, "StageData::bgDraw");

	// SSZ: float bgscl = `hires ? 0.5 : 1.0;
	float bgscl = hires ? 0.5f : 1.0f;
	float yofs = g_env_shake.getOffset();
	float posx = x, posy = y;
	float scl2 = localscl * scl;

	const CommonData& cd = common_get_state();
	const CameraData& cam = cd.cam;

	// ── Boundhigh clamping ──
	// SSZ: branch { float bhtmp = max(0, boundhigh); cond posy > bhtmp: ... cond posy < boundhigh: ... }
	{
		float bhtmp = static_cast<float>(std::max(0, cam.stg.boundhigh));
		if (posy > bhtmp) {
			yofs += (posy - bhtmp) * scl2;
			posy = bhtmp;
		} else if (posy < static_cast<float>(cam.stg.boundhigh)) {
			yofs += (posy - static_cast<float>(cam.stg.boundhigh)) * scl2;
			posy = static_cast<float>(cam.stg.boundhigh);
		}
	}

	// ── Vertical follow ──
	// SSZ: if(cam.stg.verticalfollow > 0.0) branch { ... }
	if (cam.stg.verticalfollow > 0.0f) {
		if (yofs < 0.0f) {
			// SSZ: branch { float temp = ...; if(temp >= 0.0) break; cond yofs < temp: ... else: ... }
			float temp =
				(static_cast<float>(cam.stg.boundhigh) - posy) * scl2
				+ (
					scl > 1.0f
						? (cam.screenZoff + static_cast<float>(cd.GameHeight - 240))
						: static_cast<float>(cd.GameHeight)
				  ) * (1.0f / scl - 1.0f);
			if (temp >= 0.0f) {
				// break (SSZ: if(temp >= 0.0) break — skip the rest of the inner branch)
			} else if (yofs < temp) {
				yofs -= temp;
				posy += temp / scl2;
			} else {
				posy += yofs / scl2;
				yofs = 0.0f;
			}
		} else {
			// SSZ: else: branch { cond -yofs < posy * scl2: ... else: ... }
			if (-yofs < posy * scl2) {
				yofs += posy * scl2;
				posy = 0.0f;
			} else {
				posy += yofs / scl2;
				yofs = 0.0f;
			}
		}
	}

	// ── Non-zoom → ceil position ──
	// SSZ: if(!cam.zoom) { posx = ceil(posx - 0.5); posy = ceil(posy - 0.5); }
	if (!cam.zoom) {
		posx = std::ceil(posx - 0.5f);
		posy = std::ceil(posy - 0.5f);
	}

	// ── drawOffsetY computation ──
	// SSZ: yofs += (drawOffsetY + (localh - 240.0) * localscl) * scl ** (...)
	{
		double exponent = (
			(360.0 * static_cast<double>(cam.stg.localw)
			 + 160.0 * static_cast<double>(cam.stg.localh))
			/ static_cast<double>(cam.stg.localw)
			+ static_cast<double>(cam.stg.drawOffsetY)
		) / 480.0;

		yofs += (
			cam.stg.drawOffsetY
			+ static_cast<float>(cam.stg.localh - 240) * localscl
		) * static_cast<float>(std::pow(static_cast<double>(scl), exponent));
	}

	// ── Draw background layers ──
	// SSZ: loop index i = 0; while i < #bg; if visible && toplayer == t && #anim.spr > 0 → draw(...)
	{
		auto& bgState = bg_get_state();
		for (size_t i = 0; i < bgState.layers.size(); i++) {
			auto& bg = bgState.layers[i];
			if (bg.visible && bg.toplayer == t && bg.anim && bg.anim->spr) {
				bg.draw(posx, posy, scl, bgscl, localscl, xscale, yscale, yofs);
			}
		}
	}
}

void StageData::clear() {
	SSZ_TRACE_CAT(TRACE_SYS, "StageData::clear");
	def.clear();
	spr.clear();
	bgmusic.clear();
	name.clear();
	displayname.clear();
	author.clear();
	nameLow.clear();
	displaynameLow.clear();
	authorLow.clear();
	// bg.new(0), actionList.new(0), actionTable.clear(), bgctrlList.new(0)
	// deferred until bg_service defines BackGround[], Action[], BGCtrl[] types.
	// StageBgCtrlDef (bgcdef) keeps its defaults — re-parsed on next load().
}

void StageData::reset() {
	SSZ_TRACE_CAT(TRACE_SYS, "StageData::reset");
	// EnvShake is reset between rounds by the caller (stage_reset()),
	// which triggers g_env_shake.clear().
	// bga.clear() deferred — bg_service types not yet defined.
	// bg: [void(i=){i.reset();}] deferred.
	// bgctrlList: [void(i=){i.currenttime = 0;}] deferred.
	// bgctl.clear(), then bgctl.add(bgctrlList[i]) for each in reverse — deferred.
}

// =========================================================================
// Module-level functions
// =========================================================================

void stage_init() {
	g_stage = StageData{};
	g_stage.init();
	g_env_shake = EnvShakeData{};
	g_env_shake.clear();
	g_bgmusic.clear();
}

std::string stage_load(const std::string& defPath) {
	return g_stage.load(defPath);
}

void stage_action() {
	g_stage.action();
}

void stage_bg_draw(bool t, float x, float y, float scl) {
	g_stage.bgDraw(t, x, y, scl);
}

void stage_clear() {
	g_stage.clear();
}

void stage_reset() {
	g_stage.reset();
}

EnvShakeData& stage_get_env_shake() {
	return g_env_shake;
}

std::string& stage_get_bgmusic() {
	return g_bgmusic;
}

} // namespace ikemen::ssz_native
