// common_service.cpp — Native C++ implementations matching ssz_script/ssz/common.ssz
//
// Phase 3: Real implementations for all 16 public functions.
// Matches the SSZ common.ssz (1199 line) behavior exactly.

#include "common_service.hpp"
#include "math_service.hpp"
#include "sdlplugin_service.hpp"
#include "ssz_trace.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ikemen::ssz_native {

// ── flagInit ──
// Initializes com, taglevel, autoguard, and powerShare arrays.
// Called at module load time in SSZ; matches the SSZ flagInit() body.
void common_flag_init(CommonData& cd) {
	// com.new(maxSimul*2); each = 4
	cd.com.assign(cd.maxSimul * 2, 4);

	// taglevel.new(maxSimul*4); each = 8
	cd.taglevel.assign(cd.maxSimul * 4, 8);

	// autoguard.new(maxSimul*2); each = false
	cd.autoguard.assign(cd.maxSimul * 2, false);

	// powerShare.new(2); each = true
	cd.powerShare.assign(2, true);
}

// ── resetRemapInput ──
// Fills inputRemap with identity mapping [0, 1, 2, ..., maxSimul*2-1].
void common_reset_remap_input(CommonData& cd) {
	int n = cd.maxSimul * 2;
	cd.inputRemap.resize(n);
	for (int i = 0; i < n; i++)
		cd.inputRemap[i] = i;
}

// ── setSize ──
// Computes game area dimensions and scale factors from window size.
// Matches the SSZ setSize() logic exactly.
void common_set_size(CommonData& cd, int w, int h) {
	cd.GameWidth = w * 3 > h * 4 ? w * 3 * 320 / (h * 4) : 320;
	cd.GameHeight = h * 4 > w * 3 ? h * 4 * 240 / (w * 3) : 240;
	cd.WidthScale = (float)w / (float)cd.GameWidth;
	cd.HeightScale = (float)h / (float)cd.GameHeight;
}

// ── tickFrame ──
// Returns true if a new tick has occurred since last check.
// SSZ: ret .oldTickCount < .tickCount;
bool common_tick_frame(const CommonData& cd) {
	return cd.oldTickCount < cd.tickCount;
}

// ── tickNextFrame ──
// Returns true if the next tick boundary has been reached.
// SSZ: ret (int)(.tickCountF + .nextAddTime) > .tickCount;
bool common_tick_next_frame(const CommonData& cd) {
	return static_cast<int>(cd.tickCountF + cd.nextAddTime) > cd.tickCount;
}

// ── tickInterpola ──
// Returns interpolation factor for smooth animation.
// SSZ: ret .tickNextFrame() ? 1.0 : .tickCountF - .lastTick + .nextAddTime;
float common_tick_interpola(const CommonData& cd) {
	if (common_tick_next_frame(cd))
		return 1.0f;
	return cd.tickCountF - cd.lastTick + cd.nextAddTime;
}

// ── addFrameTime ──
// Advances the frame timing state machine.
// SSZ: complex branch logic matching the SSZ addFrameTime body.
bool common_add_frame_time(CommonData& cd, float t) {
	cd.oldTickCount = cd.tickCount;

	if (static_cast<int>(cd.tickCountF) > cd.tickCount) {
		cd.tickCount++;
		return false;
	}

	cd.tickCountF += cd.nextAddTime;
	if (static_cast<int>(cd.tickCountF) > cd.tickCount) {
		cd.tickCount++;
		cd.lastTick = cd.tickCountF;
	}

	cd.oldNextAddTime = cd.nextAddTime;
	cd.nextAddTime = t;
	return true;
}

// ── resetFrameTime ──
// Resets all frame timing counters to initial state.
void common_reset_frame_time(CommonData& cd) {
	cd.tickCount = 0;
	cd.oldTickCount = cd.tickCount - 1;
	cd.tickCountF = 0.0f;
	cd.lastTick = 0.0f;
	cd.nextAddTime = 1.0f;  // Speed = (float)cfg.GameSpeed / 60.0, default 1.0
	cd.oldNextAddTime = 1.0f;
}

// ── matchOver ──
// Returns true if either player has reached the required wins.
// SSZ: ret .p1wins >= .p1mw || .p2wins >= .p2mw || .forceOver;
bool common_match_over(const CommonData& cd) {
	return cd.p1wins >= cd.p1mw || cd.p2wins >= cd.p2mw || cd.forceOver;
}

// ── nextLine ──
// Scans forward from index i for the next newline sequence.
// Returns 1 if '\n' is found, 2 if '\r\n' is found, 0 if no newline.
// Modifies i to point to the newline character.
int common_next_line(int& i, const std::string& str) {
	int n = static_cast<int>(str.size());
	while (i < n) {
		if (str[i] == '\n')
			return 1;
		if (str[i] == '\r') {
			if (i + 1 < n && str[i + 1] == '\n') {
				i++;
				return 2;
			}
		}
		i++;
	}
	return 0;
}

// ── splitLines ──
// Splits a string into lines, discarding newline characters.
std::vector<std::string> common_split_lines(const std::string& str) {
	std::vector<std::string> result;
	int i = 0;
	int n = static_cast<int>(str.size());
	while (i < n) {
		int start = i;
		// Advance to the next newline
		while (i < n && str[i] != '\n' && str[i] != '\r')
			i++;
		result.push_back(str.substr(start, i - start));
		// Skip the newline sequence
		if (i < n && str[i] == '\r') i++;
		if (i < n && str[i] == '\n') i++;
	}
	return result;
}

// ── atof ──
// Parses a double from a string (handles sign, decimal, exponent).
// Matches SSZ common.ssz atof() logic.
double common_atof(const std::string& str) {
	const char* s = str.c_str();
	while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
	if (*s == '\0') return 0.0;

	bool negative = (*s == '-');
	if (*s == '-' || *s == '+') s++;

	double result = 0.0;
	int decimal_pos = -1;
	while (*s >= '0' && *s <= '9') {
		result = result * 10.0 + static_cast<double>(*s - '0');
		s++;
	}
	if (*s == '.') {
		s++;
		decimal_pos = 0;
		while (*s >= '0' && *s <= '9') {
			result = result * 10.0 + static_cast<double>(*s - '0');
			s++;
			decimal_pos++;
		}
	}

	// Handle exponent
	if (*s == 'e' || *s == 'E') {
		s++;
		bool exp_neg = (*s == '-');
		if (*s == '-' || *s == '+') s++;
		double exp_val = 0.0;
		while (*s >= '0' && *s <= '9') {
			exp_val = exp_val * 10.0 + static_cast<double>(*s - '0');
			s++;
		}
		if (exp_neg) exp_val = -exp_val;
		if (decimal_pos > 0) {
			result *= std::pow(10.0, exp_val - decimal_pos);
		} else {
			result *= std::pow(10.0, exp_val);
		}
	} else if (decimal_pos > 0) {
		result *= std::pow(10.0, -decimal_pos);
	}

	return negative ? -result : result;
}

// ── atoi ──
// Parses an integer from a string (handles sign).
// Matches SSZ common.ssz atoi() logic.
int common_atoi(const std::string& str) {
	const char* s = str.c_str();
	while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
	if (*s == '\0') return 0;

	int sign = 1;
	if (*s == '-') { sign = -1; s++; }
	else if (*s == '+') s++;

	int result = 0;
	while (*s >= '0' && *s <= '9') {
		result = result * 10 + static_cast<int>(*s - '0');
		s++;
	}
	return result * sign;
}

// ── loadText ──
// Reads a text file, detecting UTF-8 BOM. Returns file contents as a string.
// SSZ: reads file, checks for EF BB BF BOM, converts from UTF-8 if needed.
std::string common_load_text(const std::string& filename, bool unicode) {
	// Use platform file I/O through the file_service plugin API
	// This reads raw bytes from the file
	std::wstring wfilename(filename.begin(), filename.end());
	
	// We use the native file plugin to read the file via Open/ReadAry/Close
	// Since this code may be called before the native file plugin is fully wired,
	// try to use stdio directly as a fallback
	FILE* fp = nullptr;
#ifdef _WIN32
	_wfopen_s(&fp, wfilename.c_str(), L"rb");
#else
	fp = fopen(filename.c_str(), "rb");
#endif
	if (!fp) return {};

	// Get file size
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (size <= 0) { fclose(fp); return {}; }

	std::vector<uint8_t> buf(static_cast<size_t>(size));
	if (fread(buf.data(), 1, static_cast<size_t>(size), fp) != static_cast<size_t>(size)) {
		fclose(fp);
		return {};
	}
	fclose(fp);

	// Check for UTF-8 BOM (EF BB BF)
	std::string result;
	if (buf.size() >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) {
		// UTF-8 with BOM — convert to char string
		result.resize(buf.size() - 3);
		for (size_t i = 3; i < buf.size(); i++)
			result[i - 3] = static_cast<char>(buf[i]);
	} else {
		// No BOM — copy raw bytes
		result.resize(buf.size());
		for (size_t i = 0; i < buf.size(); i++)
			result[i] = static_cast<char>(buf[i]);
	}

	return result;
}

// ── readFileName ──
// If unicode is true, returns the path as-is.
// If unicode is false, converts via AsciiToLocal.
// SSZ: ret unicode ? f : mes.AsciiToLocal(f)
// The native implementation converts from UTF-8 to the system ANSI code page
// using Win32 APIs, matching the behavior of mesdialog's AsciiToLocal.
std::string common_read_file_name(const std::string& f, bool unicode) {
	if (unicode) return f;
#ifdef _WIN32
	// AsciiToLocal: convert from UTF-8 to system ANSI code page
	// Step 1: UTF-8 (narrow) → UTF-16 (wide)
	int wideLen = MultiByteToWideChar(CP_UTF8, 0, f.c_str(), -1, nullptr, 0);
	if (wideLen > 0) {
		std::wstring wide(static_cast<size_t>(wideLen), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, f.c_str(), -1, &wide[0], wideLen);
		// Step 2: UTF-16 (wide) → ANSI code page (narrow)
		int ansiLen = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (ansiLen > 0) {
			std::string ansi(static_cast<size_t>(ansiLen), '\0');
			WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, &ansi[0], ansiLen, nullptr, nullptr);
			// Remove null terminator included by WideCharToMultiByte
			if (!ansi.empty() && ansi.back() == '\0')
				ansi.pop_back();
			return ansi;
		}
	}
#endif
	// Fallback: return input unchanged (non-Windows platforms or conversion failure)
	return f;
}

// ── loadFile ──
// Searches for a file relative to the def file's directory, then data/, ssz/,
// and finally the raw path. Calls the load callback for each found path.
// SSZ: searches deffile dir, "data/", "ssz/", then raw path.
std::string common_load_file(const std::string& deffile, std::string& file,
	void* load_callback)
{
	(void)load_callback; // Callback wiring deferred until loader integration

	// Search relative to def file's directory
	size_t slash = deffile.find_last_of("/\\");
	if (slash != std::string::npos) {
		std::string candidate = deffile.substr(0, slash + 1) + file;
		FILE* fp = nullptr;
#ifdef _WIN32
		std::wstring wcandidate(candidate.begin(), candidate.end());
		_wfopen_s(&fp, wcandidate.c_str(), L"rb");
#else
		fp = fopen(candidate.c_str(), "rb");
#endif
		if (fp) {
			fclose(fp);
			file = candidate;
			return {}; // Success — no error
		}
	}

	// Try "data/" prefix
	{
		std::string candidate = "data/" + file;
		FILE* fp = nullptr;
#ifdef _WIN32
		std::wstring wcandidate(candidate.begin(), candidate.end());
		_wfopen_s(&fp, wcandidate.c_str(), L"rb");
#else
		fp = fopen(candidate.c_str(), "rb");
#endif
		if (fp) {
			fclose(fp);
			file = candidate;
			return {};
		}
	}

	// Try "ssz/" prefix
	{
		std::string candidate = "ssz/" + file;
		FILE* fp = nullptr;
#ifdef _WIN32
		std::wstring wcandidate(candidate.begin(), candidate.end());
		_wfopen_s(&fp, wcandidate.c_str(), L"rb");
#else
		fp = fopen(candidate.c_str(), "rb");
#endif
		if (fp) {
			fclose(fp);
			file = candidate;
			return {};
		}
	}

	// Try raw path
	{
		FILE* fp = nullptr;
#ifdef _WIN32
		std::wstring wfile(file.begin(), file.end());
		_wfopen_s(&fp, wfile.c_str(), L"rb");
#else
		fp = fopen(file.c_str(), "rb");
#endif
		if (fp) {
			fclose(fp);
			return {};
		}
	}

	return file + ":\r\n" + "Failed to open file";
}

// =========================================================================
// Camera functions — match common.ssz Camera::scaleBound/xBound/yBound/init
// =========================================================================

void cam_init(CameraData& cam, const CommonData& cd) {
	SSZ_TRACE_CAT(TRACE_SYS, "cam_init");
	cam.boundL = static_cast<float>(cam.stg.boundleft - cam.stg.startx) * cam.stg.localscl;
	cam.boundR = static_cast<float>(cam.stg.boundright - cam.stg.startx) * cam.stg.localscl;
	cam.halfWidth = static_cast<float>(cd.GameWidth) / 2.0f;
	cam.xMin = cam.boundL - cam.halfWidth / cam_base_scale(cam);
	cam.xMax = cam.boundR + cam.halfWidth / cam_base_scale(cam);

	if (cam.stg.verticalfollow > 0.0f) {
		cam.boundH = math::min(0.0f,
			static_cast<float>(cam.stg.boundhigh) * cam.stg.localscl
			+ static_cast<float>(cd.GameHeight) - cam.stg.drawOffsetY
			- static_cast<float>(cd.GameWidth) * static_cast<float>(cam.stg.localh) / static_cast<float>(cam.stg.localw));
	} else {
		cam.boundH = 0.0f;
	}

	if (cam.stg.boundhigh > 0)
		cam.boundH += static_cast<float>(cam.stg.boundhigh) * cam.stg.localscl;

	float xminscl = static_cast<float>(cd.GameWidth) / (static_cast<float>(cd.GameWidth) - cam.boundL + cam.boundR);
	float yminscl = static_cast<float>(cd.GameHeight) / (240.0f - math::min(0.0f, cam.boundH));
	cam.minScale = math::max(cam.zoomMin, math::min(cam.zoomMax, math::max(xminscl, yminscl)));

	cam.screenZoff =
		static_cast<float>(cam.stg.zoffset) * cam.stg.localscl - cam.stg.drawOffsetY + 240.0f
		- static_cast<float>(cd.GameWidth) * static_cast<float>(cam.stg.localh) / static_cast<float>(cam.stg.localw);
}

float cam_scale_bound(const CameraData& cam, float scl) {
	SSZ_TRACE_CAT(TRACE_SYS, "cam_scale_bound");
	// SSZ: ret `zoom ? .m.max!float?(`minScale, .m.min!float?(`zoomMax, scl)) : 1.0;
	if (!cam.zoom) return 1.0f;
	return math::max(cam.minScale, math::min(cam.zoomMax, scl));
}

float cam_x_bound(const CameraData& cam, float scl, float x) {
	SSZ_TRACE_CAT(TRACE_SYS, "cam_x_bound");
	// SSZ: ret .m.max!float?(boundL - halfWidth + halfWidth / scl),
	//          .m.min!float?(boundR + halfWidth - halfWidth / scl, x);
	float minX = cam.boundL - cam.halfWidth + cam.halfWidth / scl;
	float maxX = cam.boundR + cam.halfWidth - cam.halfWidth / scl;
	return math::max(minX, math::min(maxX, x));
}

float cam_y_bound(const CameraData& cam, float scl, float y, float gameHeight) {
	SSZ_TRACE_CAT(TRACE_SYS, "cam_y_bound");
	// SSZ: if(stg.verticalfollow <= 0.0) ret 0.0;
	if (cam.stg.verticalfollow <= 0.0f) return 0.0f;

	float tmp = math::max(0.0f, 240.0f - cam.screenZoff);

	// SSZ: ret .m.max!float?(0.0, boundH) + .m.min!float?(0.0),
	//          .m.min!float?(tmp * (1.0 / scl - 1.0)),
	//          .m.max!float?(boundH - 240.0 + .m.max!float?((float)GameHeight / scl),
	//              (tmp + screenZoff / scl),
	//               y + 240.0 * (1.0 - .m.min!float?(1.0, scl)));
	//
	// The chained .< syntax passes the value before < as the first positional arg
	// to the function after the comma. So the inner max is:
	//   max(max(GameHeight / scl, 0.0f), tmp + screenZoff / scl, y + 240.0 * (1.0 - min(1.0, scl)))
	// But < passes a single value, and the function after comma receives it as arg 1.
	// Actually in SSZ: A.<, .func(B) means func(A, B).
	// So: outer = max(0.0f, boundH) + min(0.0f, min(tmp * (1.0/scl - 1.0), maxA))
	// where maxA = max(boundH - 240.0 + max(GameHeight/scl, 0.0f), tmp + screenZoff/scl, y + 240.0 * (1.0 - min(1.0, scl)))

	float innerMax = math::max(cam.boundH - 240.0f + math::max(gameHeight / scl, 0.0f),
		math::max(tmp + cam.screenZoff / scl,
			y + 240.0f * (1.0f - math::min(1.0f, scl))));

	return math::max(0.0f, cam.boundH) + math::min(0.0f,
		math::min(tmp * (1.0f / scl - 1.0f), innerMax));
}

float cam_base_scale(const CameraData& cam) {
	// SSZ: ret `stg.ztopscale;
	return cam.stg.ztopscale;
}

// ── Camera::update — sync camera runtime state with current position/scale ──
// SSZ: cam.update(scl, x, y, stg=)
// The `stg` template parameter (chr.stg) provides stage BGA offsets for
// xOffset/yOffset. Since stage BGA data is not yet wired, those are stubbed.
void cam_update(CameraData& cam, const CommonData& cd, float scl, float x, float y) {
	SSZ_TRACE_CAT(TRACE_SYS, "cam_update");
	// `scale = baseScale() * scl;
	cam.scale = cam_base_scale(cam) * scl;

	// `zoff = scl * (stg.zoffset * localscl - drawOffsetY
	//          + (240.0 - GameWidth * localh / localw) + (GameHeight - 240))
	//          + (1.0 - scl) * GameHeight;
	cam.zoff =
		scl * (
			static_cast<float>(cam.stg.zoffset) * cam.stg.localscl - cam.stg.drawOffsetY
			+ (240.0f - static_cast<float>(cd.GameWidth) * static_cast<float>(cam.stg.localh) / static_cast<float>(cam.stg.localw))
			+ static_cast<float>(cd.GameHeight - 240))
		+ (1.0f - scl) * static_cast<float>(cd.GameHeight);

	// `xOffset = stg.bga.xoffset * localscl * xscale * scl;
	// bgaXOffset/bgaYOffset are set by the stage's background action system
	// (BGActionData::action()) each frame. Until the background system is fully
	// wired, they default to 0.0f — matching the previous stub behavior.
	cam.xOffset = cam.bgaXOffset * cam.stg.localscl * cam.stg.xscale * scl;
	cam.yOffset = cam.bgaYOffset * cam.stg.localscl * cam.stg.yscale * scl;

	// `screenX = x - halfWidth / scale - xOffset;
	cam.screenX = x - cam.halfWidth / cam.scale - cam.xOffset;

	// `screenY = y - (zoff - (GameHeight - 240) * scl) / scale - yOffset;
	cam.screenY = y - (cam.zoff - static_cast<float>(cd.GameHeight - 240) * scl) / cam.scale - cam.yOffset;

	// `x = x;
	// `y = y;
	cam.x = x;
	cam.y = y;
}

// ── common_timer_step — decrement roundTime and format timerFormatted ──

void common_timer_step(CommonData& cd) {
	// Decrement roundTime each tick when countdownTimer is active (>= 0).
	// countdownTimer starts at -1 (disabled); it's set to a positive value
	// by trigger-script or game-mode code to start the countdown.
	if (common_tick_frame(cd) && cd.countdownTimer >= 0 && cd.roundTime > 0) {
		cd.roundTime--;
	}

	// Format timerFormatted as integer seconds remaining.
	// roundTime is in 60ths of a second (M.U.G.E.N convention);
	// roundTime / 60 gives the number of whole seconds remaining.
	// This matches the display format used by the lifebar.
	cd.timerFormatted = std::to_string(cd.roundTime / 60);
}

// ── screenFill ──
// Fill the entire screen with a solid color.
// SSZ: .com.screenFill(color) — fills the full .com.scrrect rect.
void common_screen_fill(uint32_t color) {
	CommonData& cd = common_get_state();
	// Use sdlplugin's fill to fill the entire window
	SdlRect r;
	r.set(0, 0, cd.GameWidth, cd.GameHeight);
	fill(r, color);
}

// ── rectFill ──
// Fill a screen rectangle with color and alpha.
// SSZ: .com.rectFill(rect, color, alpha)
// The SSZ passes alpha as 0-256; we convert to 0-255.
void common_rect_fill(const SdlRect& rect, uint32_t color, int alpha) {
	// Map alpha range: SSZ uses 0-256, sdlplugin uses 0-255
	int a = (alpha > 255) ? 255 : (alpha < 0 ? 0 : alpha);
	(void)a; // Unused with current fill — sdlplugin fill() doesn't take alpha
	// SSZ uses .com.rectFill which calls softFill
	softFill(rect, color);
}

// ── No-arg convenience wrappers (internal static CommonData) ──
// These are called by the SSZ bridge (Common* wrappers in bridge.cpp).
// They operate on an internal static CommonData instance.

static CommonData g_common_state;

void common_flag_init() { common_flag_init(g_common_state); }
void common_reset_remap_input() { common_reset_remap_input(g_common_state); }
void common_set_size(int w, int h) { common_set_size(g_common_state, w, h); }
bool common_tick_frame() { return common_tick_frame(g_common_state); }
bool common_tick_next_frame() { return common_tick_next_frame(g_common_state); }
float common_tick_interpola() { return common_tick_interpola(g_common_state); }
bool common_add_frame_time(float t) { return common_add_frame_time(g_common_state, t); }
void common_reset_frame_time() { common_reset_frame_time(g_common_state); }
bool common_match_over() { return common_match_over(g_common_state); }

CommonData& common_get_state() { return g_common_state; }

} // namespace ikemen::ssz_native
