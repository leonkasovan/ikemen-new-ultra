// video_service.hpp — Native C++ implementation for ssz_script/ssz/video.ssz
//
// video.ssz (57 lines) implements video playback via sdlplugin::playVideo.
// The &Video SSZ class has a single global instance (video.new(1)).
// Native equivalent is a static VideoState + free function video_play().

#pragma once

#include <string>

namespace ikemen::ssz_native {

struct VideoData {
	std::string fileName;
	int volume{100};
	int audioTrack{1};
	int subtitleTrack{};
};

struct VideoState {
	bool videoActive{};
};

// Play a video file. Returns 0 on ALT+F4/close, non-zero on success.
// Calls sdlplugin::PlayVideo() internally.
int video_play(const std::string& file, const std::string& capturePath, int volume, int audioTrack);
void video_play();

// State accessor
VideoState& video_get_state();

} // namespace ikemen::ssz_native
