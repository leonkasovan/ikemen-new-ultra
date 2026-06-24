#include "arcfour.hpp"

#include <utility>

namespace ikemen {

Arcfour::Arcfour()
	: m_x(0)
	, m_y(0)
{
	for (int i = 0; i < 256; i++) {
		m_state[i] = static_cast<uint8_t>(i);
	}
}

void Arcfour::init(const std::vector<uint8_t>& key)
{
	if (key.empty()) return;

	m_x = 0;
	m_y = 0;

	for (int i = 0; i < 256; i++) {
		m_state[i] = static_cast<uint8_t>(i);
	}

	uint32_t j = 0;
	for (uint32_t i = 0; i < 256; i++) {
		j = (j + key[i % key.size()] + m_state[i]) & 0xFF;
		std::swap(m_state[i], m_state[j]);
	}
}

uint8_t Arcfour::getByte()
{
	m_x = (m_x + 1) & 0xFF;
	uint8_t sx = m_state[m_x];
	m_y = (sx + m_y) & 0xFF;
	uint8_t sy = m_state[m_y];
	m_state[m_y] = sx;
	m_state[m_x] = sy;
	return m_state[(sx + sy) & 0xFF];
}

std::vector<uint8_t> Arcfour::encrypt(const std::vector<uint8_t>& src)
{
	std::vector<uint8_t> dest;
	dest.reserve(src.size());
	for (size_t i = 0; i < src.size(); i++) {
		dest.push_back(src[i] ^ getByte());
	}
	return dest;
}

} // namespace ikemen
