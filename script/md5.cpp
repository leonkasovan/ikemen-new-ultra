#include "md5.hpp"

#include <cstring>

namespace ikemen {

static constexpr uint32_t T(int i) {
	constexpr uint32_t table[64] = {
		0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
		0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
		0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
		0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
		0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
		0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
		0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
		0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
		0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
		0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
		0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
		0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
		0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
		0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
		0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
		0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
	};
	return table[i];
}

static inline uint32_t rotl(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }

namespace {
	inline uint32_t F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
	inline uint32_t G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
	inline uint32_t H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z;       }
	inline uint32_t I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z);    }

	static constexpr uint32_t S11 =  7, S12 = 12, S13 = 17, S14 = 22;
	static constexpr uint32_t S21 =  5, S22 =  9, S23 = 14, S24 = 20;
	static constexpr uint32_t S31 =  4, S32 = 11, S33 = 16, S34 = 23;
	static constexpr uint32_t S41 =  6, S42 = 10, S43 = 15, S44 = 21;

	static constexpr uint8_t K1[] = {  0, 1, 2,3,4,5,6,7,8,9,10,11,12,13,14,15};
	static constexpr uint8_t K2[] = {  1, 6,11,0,5,10,15,4,9,14, 3, 8,13, 2, 7,12};
	static constexpr uint8_t K3[] = {  5, 8,11,14,1,4,7,10,13,0,3,6,9,12,15,2};
	static constexpr uint8_t K4[] = {  0, 7,14,5,12,3,10,1,8,15,6,13,4,11,2,9};
} // namespace

Md5::Md5() { init(); }

void Md5::init()
{
	m_count[0] = m_count[1] = 0;
	m_abcd[0] = 0x67452301;
	m_abcd[1] = 0xefcdab89;
	m_abcd[2] = 0x98badcfe;
	m_abcd[3] = 0x10325476;
}

void Md5::process(const uint8_t block[64])
{
	uint32_t a = m_abcd[0], b = m_abcd[1], c = m_abcd[2], d = m_abcd[3];
	uint32_t X[16];
	for (int i = 0; i < 16; i++)
		X[i] = static_cast<uint32_t>(block[i*4]) | static_cast<uint32_t>(block[i*4+1])<<8
		     | static_cast<uint32_t>(block[i*4+2])<<16 | static_cast<uint32_t>(block[i*4+3])<<24;

	auto FF = [&](uint32_t& a, uint32_t b, uint32_t c, uint32_t d, int k, int s, int t) {
		a = b + rotl(a + F(b,c,d) + X[k] + T(t), s);
	};
	auto GG = [&](uint32_t& a, uint32_t b, uint32_t c, uint32_t d, int k, int s, int t) {
		a = b + rotl(a + G(b,c,d) + X[k] + T(t), s);
	};
	auto HH = [&](uint32_t& a, uint32_t b, uint32_t c, uint32_t d, int k, int s, int t) {
		a = b + rotl(a + H(b,c,d) + X[k] + T(t), s);
	};
	auto II = [&](uint32_t& a, uint32_t b, uint32_t c, uint32_t d, int k, int s, int t) {
		a = b + rotl(a + I(b,c,d) + X[k] + T(t), s);
	};

	FF(a,b,c,d,K1[ 0],S11, 0); FF(d,a,b,c,K1[ 1],S12, 1); FF(c,d,a,b,K1[ 2],S13, 2); FF(b,c,d,a,K1[ 3],S14, 3);
	FF(a,b,c,d,K1[ 4],S11, 4); FF(d,a,b,c,K1[ 5],S12, 5); FF(c,d,a,b,K1[ 6],S13, 6); FF(b,c,d,a,K1[ 7],S14, 7);
	FF(a,b,c,d,K1[ 8],S11, 8); FF(d,a,b,c,K1[ 9],S12, 9); FF(c,d,a,b,K1[10],S13,10); FF(b,c,d,a,K1[11],S14,11);
	FF(a,b,c,d,K1[12],S11,12); FF(d,a,b,c,K1[13],S12,13); FF(c,d,a,b,K1[14],S13,14); FF(b,c,d,a,K1[15],S14,15);

	GG(a,b,c,d,K2[ 0],S21,16); GG(d,a,b,c,K2[ 1],S22,17); GG(c,d,a,b,K2[ 2],S23,18); GG(b,c,d,a,K2[ 3],S24,19);
	GG(a,b,c,d,K2[ 4],S21,20); GG(d,a,b,c,K2[ 5],S22,21); GG(c,d,a,b,K2[ 6],S23,22); GG(b,c,d,a,K2[ 7],S24,23);
	GG(a,b,c,d,K2[ 8],S21,24); GG(d,a,b,c,K2[ 9],S22,25); GG(c,d,a,b,K2[10],S23,26); GG(b,c,d,a,K2[11],S24,27);
	GG(a,b,c,d,K2[12],S21,28); GG(d,a,b,c,K2[13],S22,29); GG(c,d,a,b,K2[14],S23,30); GG(b,c,d,a,K2[15],S24,31);

	HH(a,b,c,d,K3[ 0],S31,32); HH(d,a,b,c,K3[ 1],S32,33); HH(c,d,a,b,K3[ 2],S33,34); HH(b,c,d,a,K3[ 3],S34,35);
	HH(a,b,c,d,K3[ 4],S31,36); HH(d,a,b,c,K3[ 5],S32,37); HH(c,d,a,b,K3[ 6],S33,38); HH(b,c,d,a,K3[ 7],S34,39);
	HH(a,b,c,d,K3[ 8],S31,40); HH(d,a,b,c,K3[ 9],S32,41); HH(c,d,a,b,K3[10],S33,42); HH(b,c,d,a,K3[11],S34,43);
	HH(a,b,c,d,K3[12],S31,44); HH(d,a,b,c,K3[13],S32,45); HH(c,d,a,b,K3[14],S33,46); HH(b,c,d,a,K3[15],S34,47);

	II(a,b,c,d,K4[ 0],S41,48); II(d,a,b,c,K4[ 1],S42,49); II(c,d,a,b,K4[ 2],S43,50); II(b,c,d,a,K4[ 3],S44,51);
	II(a,b,c,d,K4[ 4],S41,52); II(d,a,b,c,K4[ 5],S42,53); II(c,d,a,b,K4[ 6],S43,54); II(b,c,d,a,K4[ 7],S44,55);
	II(a,b,c,d,K4[ 8],S41,56); II(d,a,b,c,K4[ 9],S42,57); II(c,d,a,b,K4[10],S43,58); II(b,c,d,a,K4[11],S44,59);
	II(a,b,c,d,K4[12],S41,60); II(d,a,b,c,K4[13],S42,61); II(c,d,a,b,K4[14],S43,62); II(b,c,d,a,K4[15],S44,63);

	m_abcd[0] += a; m_abcd[1] += b; m_abcd[2] += c; m_abcd[3] += d;
}

void Md5::append(const std::vector<uint8_t>& data)
{
	uint32_t nbits = static_cast<uint32_t>(data.size() << 3);
	if (data.empty()) return;
	m_count[1] += static_cast<uint32_t>(data.size() >> 29);
	m_count[0] += nbits;
	if (m_count[0] < nbits) m_count[1]++;

	size_t offset = (m_count[0] >> 3) & 63;
	size_t p = 0;

	if (offset) {
		size_t copy = offset + data.size() > 64 ? 64 - offset : data.size();
		std::memcpy(m_buf.data() + offset, data.data(), copy);
		if (offset + copy < 64) return;
		p += copy - offset;
		process(m_buf.data());
	}

	while (p + 64 <= data.size()) {
		process(data.data() + p);
		p += 64;
	}

	std::memcpy(m_buf.data(), data.data() + p, data.size() - p);
}

std::vector<uint8_t> Md5::finish()
{
	constexpr uint8_t padding[64] = {
		0x80,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	};

	uint8_t count[8];
	for (int i = 0; i < 8; i++)
		count[i] = static_cast<uint8_t>(m_count[i >> 2] >> ((i & 3) << 3));

	append({padding, padding + ((55 - (m_count[0] >> 3)) & 63) + 1});
	append({count, count + 8});

	std::vector<uint8_t> digest(16);
	for (int i = 0; i < 16; i++)
		digest[i] = static_cast<uint8_t>(m_abcd[i >> 2] >> ((i & 3) << 3));
	return digest;
}

std::vector<uint8_t> md5(const std::vector<uint8_t>& data)
{
	Md5 m;
	m.append(data);
	return m.finish();
}

std::wstring md5str(const std::vector<uint8_t>& data)
{
	auto d = md5(data);
	std::wstring s;
	for (auto b : d) {
		wchar_t hi = L"0123456789abcdef"[b >> 4];
		wchar_t lo = L"0123456789abcdef"[b & 0xF];
		s += hi; s += lo;
	}
	return s;
}

} // namespace ikemen
