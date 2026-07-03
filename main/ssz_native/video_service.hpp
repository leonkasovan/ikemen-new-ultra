// video_service.hpp — Native C++ scaffolding for ssz_script/ssz/video.ssz
//
// video.ssz (57 lines) implements video playback via sdlplugin::playVideo.

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

void video_play(const std::string& file, const std::string& capturePath, int volume, int audioTrack);

} // namespace ikemen::ssz_native
