#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ikemen {

class Arcfour {
public:
	Arcfour();

	void init(const std::vector<uint8_t>& key);
	uint8_t getByte();
	std::vector<uint8_t> encrypt(const std::vector<uint8_t>& src);

private:
	std::array<uint8_t, 256> m_state;
	uint32_t m_x;
	uint32_t m_y;
};

inline bool arcfourEnc(std::vector<uint8_t>& dest, const std::vector<uint8_t>& key, const std::vector<uint8_t>& src)
{
	if (key.empty()) return false;
	Arcfour rc4;
	rc4.init(key);
	dest = rc4.encrypt(src);
	return true;
}

} // namespace ikemen
