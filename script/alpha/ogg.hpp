#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen {

class OggVorbis {
public:
	OggVorbis();
	~OggVorbis();

	bool    open(const std::wstring& file);
	void    clear();
	int64_t pcmTotal();
	int32_t channels();
	int32_t rate();
	intptr_t read(std::vector<int16_t>& buffer);
	int32_t seek(double time);

private:
	intptr_t m_ptr = 0;
};

} // namespace ikemen
