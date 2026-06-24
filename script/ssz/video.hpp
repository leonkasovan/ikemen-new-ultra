#pragma once

#include <cstdint>
#include <string>

namespace ikemen {

extern bool g_videoActive;

class Video {
public:
	Video();

	void clear();
	void play(const std::wstring& file, const std::wstring& capturePath,
	          int volume, int audiotrack);

	std::wstring fileName() const { return m_fileName; }

private:
	std::wstring m_fileName;
};

extern Video g_video;

} // namespace ikemen
