#include "base64.hpp"

namespace ikemen {
namespace {

char uintToB64Char(uint32_t n)
{
	if (n < 26)  return static_cast<char>('A' + n);
	if (n < 52)  return static_cast<char>('a' + (n - 26));
	if (n < 62)  return static_cast<char>('0' + (n - 52));
	if (n == 62) return '+';
	if (n == 63) return '/';
	return '=';
}

uint32_t b64CharToUint(char c)
{
	if (c >= 'A' && c <= 'Z') return static_cast<uint32_t>(c - 'A');
	if (c >= 'a' && c <= 'z') return static_cast<uint32_t>(c - 'a') + 26;
	if (c >= '0' && c <= '9') return static_cast<uint32_t>(c - '0') + 52;
	if (c == '+') return 62;
	if (c == '/') return 63;
	return 64;
}

} // namespace

std::string encBase64(const std::vector<uint8_t>& data)
{
	std::string buf;
	size_t i = 0;
	uint32_t bits = 0;
	int bitCount = 0;

	while (i < data.size()) {
		bits = (bits << 8) | data[i++];
		bitCount += 8;

		while (bitCount >= 6) {
			bitCount -= 6;
			buf.push_back(uintToB64Char((bits >> bitCount) & 0x3F));
		}
	}

	if (bitCount > 0) {
		buf.push_back(uintToB64Char((bits << (6 - bitCount)) & 0x3F));
	}

	while (buf.size() % 4 != 0) {
		buf.push_back('=');
	}

	return buf;
}

std::vector<uint8_t> decBase64(const std::string& b64)
{
	std::vector<uint8_t> buf;
	uint32_t bits = 0;
	int bitCount = 0;

	for (size_t i = 0; i < b64.size(); i++) {
		uint32_t tmp = b64CharToUint(b64[i]);
		if (tmp > 63) {
			// padding or invalid character — stop decoding
			break;
		}

		bits = (bits << 6) | tmp;
		bitCount += 6;

		if (bitCount >= 8) {
			bitCount -= 8;
			buf.push_back(static_cast<uint8_t>((bits >> bitCount) & 0xFF));
		}
	}

	return buf;
}

} // namespace ikemen
