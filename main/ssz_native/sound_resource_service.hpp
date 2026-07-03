// sound_resource_service.hpp — Native C++ scaffolding for ssz_script/ssz/sound.ssz
//
// sound.ssz (408 lines) manages game sound effects and BGM resources:
//   &SndNnm — sound group/number identifier
//   &Wave — raw PCM wave data with metadata
//   &Bgm — background music state

#pragma once

#include <string>
#include <vector>

namespace ikemen::ssz_native {

struct SndNnm { int group{-1}, number{}; };

struct WaveData {
	unsigned samplesPerSec{44100};
	unsigned short channels{0x1}, bytesPerSample{0x1};
	std::vector<unsigned char> wav;
	SndNnm num;
};

struct BgmData {
	std::string fileName;
	int volume{100};
	bool loop{true};
};

} // namespace ikemen::ssz_native
