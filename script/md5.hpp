#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ikemen {

class Md5 {
public:
	Md5();

	void init();
	void append(const std::vector<uint8_t>& data);
	std::vector<uint8_t> finish();

private:
	void process(const uint8_t block[64]);

	std::array<uint32_t, 2>  m_count{};
	std::array<uint32_t, 4>  m_abcd{};
	std::array<uint8_t, 64>  m_buf{};
};

std::vector<uint8_t> md5(const std::vector<uint8_t>& data);
std::wstring         md5str(const std::vector<uint8_t>& data);

} // namespace ikemen
