// video_service.cpp — Real implementation for ssz_script/ssz/video.ssz
//
// Delegates to sdlplugin::PlayVideo() for actual video playback.
// SSZ equivalent:
//   public void play(^/char file, ^/char capturepath, int volume, int audiotrack)
//   {
//     &.file.File f;
//     f.open(file, "rb");
//     if(f.open(file, "rb")) {
//       `fileName = file;
//       .videoActive = true;
//       int result = .sdl.playVideo(audiotrack, volume, capturepath, `fileName);
//       if (result == 0) { /* ALT+F4 or close — engine handles shutdown */ }
//     }
//   }

#include "video_service.hpp"
#include "ssz_native/plugin_native_api.hpp"
#include "ssz_trace.hpp"

#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace ikemen::ssz_native {

// =========================================================================
// Static state
// =========================================================================

static VideoState s_state;

VideoState& video_get_state() { return s_state; }

// =========================================================================
// video_play
// =========================================================================

int video_play(const std::string& file, const std::string& capturePath, int volume, int audioTrack) {
	SSZ_TRACE_CAT(TRACE_SYS, "video_play");

	// SSZ equivalent: f.open(file, "rb") to check file existence.
	// Use _wfopen on Windows for Unicode path support, else plain fopen.
	if (!file.empty()) {
		FILE* f = nullptr;
#ifdef _WIN32
		// Convert UTF-8 to wide string for _wfopen
		int wlen = MultiByteToWideChar(CP_UTF8, 0, file.c_str(), -1, nullptr, 0);
		if (wlen > 0) {
			std::wstring wfile(wlen, L'\0');
			MultiByteToWideChar(CP_UTF8, 0, file.c_str(), -1, &wfile[0], wlen);
			f = _wfopen(wfile.c_str(), L"rb");
		}
#else
		f = std::fopen(file.c_str(), "rb");
#endif
		if (!f) {
			SSZ_TRACE_CAT(TRACE_SYS, "video_play: file not found");
			return -1;
		}
		std::fclose(f);
	}

	// SSZ: store fileName, set videoActive
	s_state.videoActive = true;

	// SSZ: .sdl.playVideo(audiotrack, volume, capturepath, fileName)
	// The PlayVideo function is declared in plugin_native_api.hpp:
	//   int PlayVideo(const std::wstring& fn, const std::wstring& screenshotPath,
	//                 int volume, int audioTrack);
	std::wstring wfn(file.begin(), file.end());
	std::wstring wcapture(capturePath.begin(), capturePath.end());
	int result = PlayVideo(wfn, wcapture, volume, audioTrack);

	// SSZ: if (result == 0) — ALT+F4 or close window event
	// Engine handles shutdown; no explicit action needed here.

	return result;
}

// ── No-arg convenience wrapper ──
// Matches SSZ: video.new(1) → play("", "", 100, 1)
void video_play() {
	SSZ_TRACE_CAT(TRACE_SYS, "video_play (no-arg)");
	video_play("", "", 100, 1);
}

} // namespace ikemen::ssz_native
