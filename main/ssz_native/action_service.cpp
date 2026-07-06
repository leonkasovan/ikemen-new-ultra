// action_service.cpp — Real implementations for ssz_script/ssz/action.ssz
//
// Implements ActionData::read() (.air file parser), ActionData::copy(),
// DrawnClsnData::set() and DrawnClsnData::draw() for debug collision display.

#include "action_service.hpp"
#include "common_service.hpp"
#include "sdlplugin_service.hpp"
#include "ssz_trace.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace ikemen::ssz_native {

// =========================================================================
// Helper: trim whitespace from a string
// =========================================================================
namespace {

std::string trim(const std::string& s) {
	size_t start = s.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) return "";
	size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end - start + 1);
}

// Strip inline comment (everything after ';')
std::string strip_comment(const std::string& line) {
	size_t pos = line.find(';');
	if (pos == std::string::npos) return line;
	return line.substr(0, pos);
}

// Convert to lowercase
std::string to_lower(const std::string& s) {
	std::string r = s;
	for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return r;
}

// Split string by delimiter
std::vector<std::string> split(const std::string& s, char delim) {
	std::vector<std::string> parts;
	size_t start = 0;
	while (start < s.size()) {
		size_t end = s.find(delim, start);
		if (end == std::string::npos) {
			parts.push_back(s.substr(start));
			break;
		}
		parts.push_back(s.substr(start, end - start));
		start = end + 1;
	}
	return parts;
}

// Parse comma/tab-separated ints (SSZ ctaOF equivalent)
std::vector<int> parse_ints(const std::string& line) {
	std::vector<int> result;
	std::string current;
	for (size_t i = 0; i < line.size(); i++) {
		char c = line[i];
		if (c == ',' || c == '\t' || c == ' ') {
			if (!current.empty()) {
				result.push_back(std::atoi(current.c_str()));
				current.clear();
			}
			if (c == ' ' || c == '\t') {
				// Tab/space can be a delimiter too, but skip consecutive spaces
				while (i + 1 < line.size() && (line[i + 1] == ' ' || line[i + 1] == '\t'))
					i++;
			}
		} else {
			current += c;
		}
	}
	if (!current.empty())
		result.push_back(std::atoi(current.c_str()));
	return result;
}

} // anonymous namespace

// =========================================================================
// ActionData::copy
// =========================================================================

void ActionData::copy(const ActionData& a) {
	no = a.no;
	ani.copy(a.ani);
}

// =========================================================================
// ActionData::read — .air file action parser
// =========================================================================
// SSZ equivalent (action.ssz):
//
// public void read(^^/char lines, index i=)
// {
//   ani.frames.new(0);
//   index ols = 0;
//   loop{
//     if(#lines[i] > 0 && lines[i][0] == '[') break;
//     line = toLower(trim(strip_comment(lines[i])));
//     if numeric line:  -> frame data
//       ary = ctaOF(line);
//       readData(frame, ary, line);
//       handle default clsn1/clsn2
//     if "loopstart": set loopstart
//     if "clsn": parse clsn block
//   }
//   if(loopstart >= frames.size()) restore ols
//   setFrames(frames, loopstart)
// }

int ActionData::read(const std::vector<std::string>& lines, int& i) {
	SSZ_TRACE_CAT(TRACE_SYS, "ActionData::read");

	ani.frames.clear();
	ani.mask = 0;  // SSZ new(): `ani.mask = 0
	int ols = ani.loopstart;

	// Working clsn arrays and defaults (persist across frames)
	std::vector<Rect> clsn1, clsn1d, clsn2, clsn2d;
	bool defaul1 = true, defaul2 = true;

	while (i < static_cast<int>(lines.size())) {
		const std::string& raw = lines[i];

		// Stop at section header
		if (!raw.empty() && raw[0] == '[') {
			i--;  // Leave it for the caller
			break;
		}

		std::string line = to_lower(trim(strip_comment(raw)));
		if (line.empty()) {
			i++;
			continue;
		}

		// ── Frame data line (starts with digit or '-') ──
		if ((line[0] >= '0' && line[0] <= '9') || line[0] == '-') {
			std::vector<int> ary = parse_ints(line);
			if (ary.size() < 5) {
				i++;
				continue;
			}

			// Save current loopstart before adding frame
			ols = ani.loopstart;

			// Create new frame
			FrameData frame;
			FrameMethods::readData(frame, ary, line);

			// Apply default clsn if not overridden
			if (defaul1 && !clsn1d.empty()) clsn1 = clsn1d;
			if (defaul2 && !clsn2d.empty()) clsn2 = clsn2d;

			// Copy active clsn boxes to frame
			if (!clsn1.empty() || !clsn2.empty()) {
				frame.clsn.resize(2);
				if (!clsn1.empty()) frame.clsn[0] = clsn1;
				if (!clsn2.empty()) frame.clsn[1] = clsn2;
			}

			ani.frames.push_back(std::move(frame));

			// Reset defaults for next frame
			defaul1 = defaul2 = true;
			clsn1.clear();
			clsn2.clear();
			i++;
			continue;
		}

		// ── loopstart ──
		if (line.size() >= 9 && line.substr(0, 9) == "loopstart") {
			ani.loopstart = static_cast<int>(ani.frames.size());
			i++;
			continue;
		}

		// ── clsn1 / clsn2 block ──
		if (line.size() >= 4 && line.substr(0, 4) == "clsn") {
			// Find the colon separating "clsn1:N" or "clsn2:N"
			size_t colon = line.find(':');
			if (colon == std::string::npos) {
				i++;
				continue;
			}

			int size = std::atoi(line.substr(colon + 1).c_str());
			if (size < 0) {
				i++;
				continue;
			}

			if (line.size() > 4 && line[4] == '1') {
				// clsn1
				clsn1.clear();
				clsn1.resize(static_cast<size_t>(size));

				// Check for "default"
				if (line.size() > 5 && line.substr(5, 7) == "default") {
					clsn1d = clsn1;
				}
				defaul1 = false;
			} else if (line.size() > 4 && line[4] == '2') {
				// clsn2
				clsn2.clear();
				clsn2.resize(static_cast<size_t>(size));

				// Check for "default"
				if (line.size() > 5 && line.substr(5, 7) == "default") {
					clsn2d = clsn2;
				}
				defaul2 = false;
			} else {
				// Neither clsn1 nor clsn2 — skip
				i++;
				continue;
			}

			if (size <= 0) {
				i++;
				continue;
			}

			// Parse subsequent clsn lines
			i++;
			int n = 0;
			while (i < static_cast<int>(lines.size()) && n < size) {
				std::string sub = to_lower(trim(strip_comment(lines[i])));
				if (sub.empty()) {
					i++;
					continue;
				}
				// Stop if not a clsn line
				if (sub.size() < 4 || sub.substr(0, 4) != "clsn") {
					break;
				}

				// Find '=' to get the value part
				size_t eq = sub.find('=');
				if (eq == std::string::npos) {
					i++;
					continue;
				}

				std::vector<int> vals = parse_ints(sub.substr(eq + 1));
				if (vals.size() < 4) {
					i++;
					continue;
				}

				// SSZ: ordered l <= r, t <= b
				Rect& r = (line[4] == '1') ? clsn1[n] : clsn2[n];
				if (vals[0] <= vals[2]) {
					r.l = vals[0];
					r.r = vals[2];
				} else {
					r.l = vals[2];
					r.r = vals[0];
				}
				if (vals[1] <= vals[3]) {
					r.t = vals[1];
					r.b = vals[3];
				} else {
					r.t = vals[3];
					r.b = vals[1];
				}
				n++;
				i++;
			}
			i--;  // Adjust for outer loop increment
			i++;
			continue;
		}

		i++;
	}

	// Restore loopstart if it ended up out of bounds
	if (ani.loopstart >= static_cast<int>(ani.frames.size()))
		ani.loopstart = ols;

	// Finalize animation data
	ani.setFrames(ani.frames, ani.loopstart);

	return no;
}

// =========================================================================
// DrawnClsnData::set
// =========================================================================
// SSZ equivalent:
//   public void set(^/&.Rect cl, float x, float y, float xs, float ys)
//   {
//     clsn = cl;
//     x = (x - .com.cam.x) * .com.cam.scale;
//     y = (y - .com.cam.y) * .com.cam.scale + .com.cam.groundLevel();
//     xscale = xs * .com.cam.scale;
//     yscale = ys * .com.cam.scale;
//   }

void DrawnClsnData::set(const std::vector<Rect>* cl, float x_, float y_, float xs, float ys) {
	clsn = cl;
	const auto& cam = common_get_state().cam;
	x = (x_ - cam.x) * cam.scale;
	y = (y_ - cam.y) * cam.scale + camera_ground_level(cam);
	xscale = xs * cam.scale;
	yscale = ys * cam.scale;
}

// =========================================================================
// DrawnClsnData::draw
// =========================================================================
// Draws each collision box rectangle using the sprite rendering pipeline.
// SSZ equivalent (action.ssz):
//
//   til.set(0, 0, 0, 0);
//   loop {
//     x = `x + `xscale*(float)`clsn[i].l + (float).com.GameWidth/2.0;
//     y = `y + `yscale*(float)`clsn[i].t + (float)(.com.GameHeight-240);
//     w = (float)`clsn[i].r - (float)`clsn[i].l;
//     h = (float)`clsn[i].b - (float)`clsn[i].t;
//     if(`xscale < 0.0) x *= -1.0;
//     if(`yscale < 0.0) y *= -1.0;
//     /?/*.cfg.Renderer != 0:
//       .sdl.RenderMugenGl(:
//         spr.pxl<>, spr.colorPallet<>=, -1, spr.rct=,
//         -x*.com.WidthScale, -y*.com.HeightScale, til=,
//         `xscale*.com.WidthScale*w, `xscale*.com.WidthScale*w,
//         `yscale*.com.WidthScale*h, 1.0, 0.0, 0.0, alpha, .com.scrrect=,
//         0.0, 0.0:);
//     /*true:
//       .sdl.renderMugenZoom(
//         .com.scrrect=, 0.0, 0.0, spr.pxl, spr.colorPallet,
//         -1, spr.rct=, -x*.com.WidthScale, -y*.com.HeightScale, til=,
//         `xscale*.com.WidthScale*w, `xscale*.com.WidthScale*w,
//         `yscale*.com.WidthScale*h, 0.0, 0d0, alpha, spr.rle, spr.pluginbuf=);
//     /*?*/
//     i++;
//   }

void DrawnClsnData::draw(const SpriteData* spr, int alpha) {
	if (!clsn || clsn->empty() || !spr) return;
	if (spr->pxl.empty() && spr->colorPallet.empty()) return;

	const auto& com = common_get_state();

	// Build the tile rect (SSZ: til.set(0, 0, 0, 0))
	SdlRect tile;
	tile.set(0, 0, 0, 0);

	// Build source rect from sprite dimensions (SSZ: spr.rct=)
	SdlRect src_rect;
	src_rect.set(spr->rct_x, spr->rct_y, spr->rct_w, spr->rct_h);

	// Build destination rect from screen dimensions (SSZ: .com.scrrect=)
	// TODO: Replace with CommonData::scrrect when that field is added to the
	// native struct — the full-screen approximation may be incorrect for
	// letterboxed or sub-rectangle rendering contexts (menu overlays, etc.).
	SdlRect dst_rect;
	dst_rect.set(0, 0, com.GameWidth, com.GameHeight);

	// Local plugin buffer for software renderer (SSZ: spr.pluginbuf=)
	std::vector<int8_t> pluginbuf;
	pluginbuf.reserve(1024);

	for (size_t i = 0; i < clsn->size(); i++) {
		const Rect& r = (*clsn)[i];

		// Compute screen-space position (SSZ equivalent)
		float dx = x + xscale * static_cast<float>(r.l) + static_cast<float>(com.GameWidth) / 2.0f;
		float dy = y + yscale * static_cast<float>(r.t) + static_cast<float>(com.GameHeight - 240);
		float w = static_cast<float>(r.r - r.l);
		float h = static_cast<float>(r.b - r.t);

		if (xscale < 0.0f) dx *= -1.0f;
		if (yscale < 0.0f) dy *= -1.0f;

		// Call renderMugenZoom (software renderer path)
		// This matches the SSZ's /*true: renderMugenZoom(...) path.
		// The GL-only RenderMugenGl path is not exposed in the native
		// sdlplugin_service — renderMugenZoom serves as the unified path.
		renderMugenZoom(
			dst_rect,                                          // dr = .com.scrrect=
			0.0f, 0.0f,                                        // rcx, rcy
			spr->pxl,                                           // pxl = spr.pxl
			spr->colorPallet,                                   // pal = spr.colorPallet
			-1,                                                  // ckey = -1
			src_rect,                                           // sr = spr.rct=
			-dx * com.WidthScale,                                // cx = -x*.com.WidthScale
			-dy * com.HeightScale,                               // ty = -y*.com.HeightScale
			tile,                                                // tile = til=
			xscale * com.WidthScale * w,                         // xtopscl = `xscale*.com.WidthScale*w
			xscale * com.WidthScale * w,                         // xbotscl = same (SSZ passes same value)
			yscale * com.WidthScale * h,                         // yscl = `yscale*.com.WidthScale*h
			0.0f,                                                // rasterxadd
			0u,                                                  // roto
			alpha,                                               // alpha
			spr->rle,                                            // rle = spr.rle
			pluginbuf                                            // pluginbuf = spr.pluginbuf=
		);
	}
}

// =========================================================================
// action_init
// =========================================================================

void action_init() {
	SSZ_TRACE_CAT(TRACE_SYS, "action_init");
	// Action module initialization.
	// The data structures (Rect, FrameData, ActionData) are plain structs
	// used by other native modules (char, sff, bg) — no runtime state
	// needs to be initialized at the module level.
}

} // namespace ikemen::ssz_native
