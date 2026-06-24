#include "video.hpp"
#include "common.hpp"

#include "../file.hpp"
#include "../alpha/sdlplugin.hpp"

namespace ikemen {

bool g_videoActive = false;
Video g_video;

Video::Video() { clear(); }

void Video::clear()
{
	m_fileName.clear();
}

void Video::play(const std::wstring& file, const std::wstring& capturePath,
                 int volume, int audiotrack)
{
	File f;
	if (f.open(file, L"rb")) {
		m_fileName = file;
		g_videoActive = true;
		ikemen::playVideo(audiotrack, volume, capturePath, m_fileName);
	}
}

} // namespace ikemen
