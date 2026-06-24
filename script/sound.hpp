#pragma once

#include <cstdint>
#include <vector>

namespace ikemen {

constexpr int FREQ          = 48000;
constexpr int CHANNELS      = 2;
constexpr int BUFFER_SAMPLES = 2048;

class SoundClient {
public:
	SoundClient();
	~SoundClient();

	bool start();
	bool stop();
	bool bufferReady();
	bool setBuffer(const std::vector<float>& buffer);

private:
	intptr_t m_cl = 0;
};

} // namespace ikemen
