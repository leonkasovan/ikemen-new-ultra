// video_static.hpp — Static plugin registration for ssz_script/ssz/video.ssz
//
// When IKEMEN_NATIVE_VIDEO_LIB=1, replaces the SSZ video.ssz module
// with native C++ entry points registered in the static plugin table.

#pragma once
#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_VIDEO_LIB

extern "C" {

// play() — initializes video playback via sdlplugin::playVideo.
void SSZ_STDCALL VideoPlay(PluginUtil* pu);

} // extern "C"

static inline bool video_static_register()
{
	SSZ_FunctionEntry entries[] = {
		{ "play", (void*)&VideoPlay },
	};
	return SSZ_RegisterFunction("video", entries, 1);
}

#else

static inline bool video_static_register()
{
	// IKEMEN_NATIVE_VIDEO_LIB=0 — SSZ video.ssz used instead.
	return true;
}

#endif
