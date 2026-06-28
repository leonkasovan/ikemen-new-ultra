#include "crypto_service.hpp"

#include <array>
#include <cstring>

namespace ikemen::ssz_native::crypto {

// ---- Base64 ----

static const char kBase64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_char_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;  // invalid or padding
}

std::string base64_encode(const std::vector<uint8_t>& data) {
    if (data.empty()) return {};
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t trio = 0;
        int remaining = static_cast<int>(data.size() - i);
        for (int j = 0; j < 3; j++) {
            trio = (trio << 8) | (j < remaining ? data[i + j] : 0);
        }
        result += kBase64Alphabet[(trio >> 18) & 0x3f];
        result += kBase64Alphabet[(trio >> 12) & 0x3f];
        result += (remaining >= 2) ? kBase64Alphabet[(trio >> 6) & 0x3f] : '=';
        result += (remaining >= 3) ? kBase64Alphabet[trio & 0x3f] : '=';
    }
    return result;
}

std::vector<uint8_t> base64_decode(const std::string& b64) {
    if (b64.empty()) return {};
    std::vector<uint8_t> result;
    result.reserve((b64.size() / 4) * 3);

    for (size_t i = 0; i + 3 < b64.size(); i += 4) {
        int v[4];
        for (int j = 0; j < 4; j++) {
            v[j] = base64_char_value(b64[i + j]);
        }
        uint32_t quad = 0;
        for (int j = 0; j < 4; j++) {
            quad = (quad << 6) | (v[j] >= 0 ? static_cast<uint32_t>(v[j]) : 0);
        }
        result.push_back(static_cast<uint8_t>((quad >> 16) & 0xff));
        if (v[2] >= 0) result.push_back(static_cast<uint8_t>((quad >> 8) & 0xff));
        if (v[3] >= 0) result.push_back(static_cast<uint8_t>(quad & 0xff));
    }
    return result;
}

// ---- Arcfour (RC4) ----

void Arcfour::init(const std::vector<uint8_t>& key) {
    x_ = 0;
    y_ = 0;
    for (int i = 0; i < 256; i++) state_[i] = static_cast<uint8_t>(i);

    size_t key_len = key.size();
    if (key_len == 0) return;

    size_t key_index = 0;
    uint8_t t;
    for (int i = 0; i < 256; i++) {
        key_index = (key_index + key[i % key_len] + state_[i]) & 0xff;
        t = state_[i];
        state_[i] = state_[key_index];
        state_[key_index] = t;
    }
}

std::vector<uint8_t> Arcfour::encrypt(const std::vector<uint8_t>& src) {
    std::vector<uint8_t> dest;
    dest.reserve(src.size());

    uint8_t sx, sy, t;
    for (size_t i = 0; i < src.size(); i++) {
        x_ = static_cast<uint8_t>(x_ + 1);
        sx = state_[x_];
        y_ = static_cast<uint8_t>(y_ + sx);
        sy = state_[y_];

        state_[x_] = sy;
        state_[y_] = sx;

        dest.push_back(static_cast<uint8_t>(src[i] ^ state_[static_cast<uint8_t>(sx + sy)]));
    }
    return dest;
}

bool arcfour_encrypt(std::vector<uint8_t>& dest, const std::vector<uint8_t>& key, const std::vector<uint8_t>& src) {
    if (key.empty()) return false;
    Arcfour rc4;
    rc4.init(key);
    dest = rc4.encrypt(src);
    return true;
}

// ---- MD5 implementation (RFC 1321) ----

namespace {

struct MD5Context {
    uint32_t state[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    uint32_t count[2] = {};
    uint8_t buffer[64] = {};

    static uint32_t F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
    static uint32_t G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
    static uint32_t H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
    static uint32_t I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }

    static uint32_t rot(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }

    static void FF(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rot(a + F(b, c, d) + x + ac, s) + b;
    }
    static void GG(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rot(a + G(b, c, d) + x + ac, s) + b;
    }
    static void HH(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rot(a + H(b, c, d) + x + ac, s) + b;
    }
    static void II(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rot(a + I(b, c, d) + x + ac, s) + b;
    }

    void transform(const uint8_t block[64]) {
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint32_t x[16];
        for (int i = 0; i < 16; i++)
            x[i] = static_cast<uint32_t>(block[i*4]) | (static_cast<uint32_t>(block[i*4+1]) << 8)
                 | (static_cast<uint32_t>(block[i*4+2]) << 16) | (static_cast<uint32_t>(block[i*4+3]) << 24);

        FF(a, b, c, d, x[ 0],  7, 0xd76aa478); FF(d, a, b, c, x[ 1], 12, 0xe8c7b756);
        FF(c, d, a, b, x[ 2], 17, 0x242070db); FF(b, c, d, a, x[ 3], 22, 0xc1bdceee);
        FF(a, b, c, d, x[ 4],  7, 0xf57c0faf); FF(d, a, b, c, x[ 5], 12, 0x4787c62a);
        FF(c, d, a, b, x[ 6], 17, 0xa8304613); FF(b, c, d, a, x[ 7], 22, 0xfd469501);
        FF(a, b, c, d, x[ 8],  7, 0x698098d8); FF(d, a, b, c, x[ 9], 12, 0x8b44f7af);
        FF(c, d, a, b, x[10], 17, 0xffff5bb1); FF(b, c, d, a, x[11], 22, 0x895cd7be);
        FF(a, b, c, d, x[12],  7, 0x6b901122); FF(d, a, b, c, x[13], 12, 0xfd987193);
        FF(c, d, a, b, x[14], 17, 0xa679438e); FF(b, c, d, a, x[15], 22, 0x49b40821);

        GG(a, b, c, d, x[ 1],  5, 0xf61e2562); GG(d, a, b, c, x[ 6],  9, 0xc040b340);
        GG(c, d, a, b, x[11], 14, 0x265e5a51); GG(b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
        GG(a, b, c, d, x[ 5],  5, 0xd62f105d); GG(d, a, b, c, x[10],  9, 0x02441453);
        GG(c, d, a, b, x[15], 14, 0xd8a1e681); GG(b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
        GG(a, b, c, d, x[ 9],  5, 0x21e1cde6); GG(d, a, b, c, x[14],  9, 0xc33707d6);
        GG(c, d, a, b, x[ 3], 14, 0xf4d50d87); GG(b, c, d, a, x[ 8], 20, 0x455a14ed);
        GG(a, b, c, d, x[13],  5, 0xa9e3e905); GG(d, a, b, c, x[ 2],  9, 0xfcefa3f8);
        GG(c, d, a, b, x[ 7], 14, 0x676f02d9); GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

        HH(a, b, c, d, x[ 5],  4, 0xfffa3942); HH(d, a, b, c, x[ 8], 11, 0x8771f681);
        HH(c, d, a, b, x[11], 16, 0x6d9d6122); HH(b, c, d, a, x[14], 23, 0xfde5380c);
        HH(a, b, c, d, x[ 1],  4, 0xa4beea44); HH(d, a, b, c, x[ 4], 11, 0x4bdecfa9);
        HH(c, d, a, b, x[ 7], 16, 0xf6bb4b60); HH(b, c, d, a, x[10], 23, 0xbebfbc70);
        HH(a, b, c, d, x[13],  4, 0x289b7ec6); HH(d, a, b, c, x[ 0], 11, 0xeaa127fa);
        HH(c, d, a, b, x[ 3], 16, 0xd4ef3085); HH(b, c, d, a, x[ 6], 23, 0x04881d05);
        HH(a, b, c, d, x[ 9],  4, 0xd9d4d039); HH(d, a, b, c, x[12], 11, 0xe6db99e5);
        HH(c, d, a, b, x[15], 16, 0x1fa27cf8); HH(b, c, d, a, x[ 2], 23, 0xc4ac5665);

        II(a, b, c, d, x[ 0],  6, 0xf4292244); II(d, a, b, c, x[ 7], 10, 0x432aff97);
        II(c, d, a, b, x[14], 15, 0xab9423a7); II(b, c, d, a, x[ 5], 21, 0xfc93a039);
        II(a, b, c, d, x[12],  6, 0x655b59c3); II(d, a, b, c, x[ 3], 10, 0x8f0ccc92);
        II(c, d, a, b, x[10], 15, 0xffeff47d); II(b, c, d, a, x[ 1], 21, 0x85845dd1);
        II(a, b, c, d, x[ 8],  6, 0x6fa87e4f); II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
        II(c, d, a, b, x[ 6], 15, 0xa3014314); II(b, c, d, a, x[13], 21, 0x4e0811a1);
        II(a, b, c, d, x[ 4],  6, 0xf7537e82); II(d, a, b, c, x[11], 10, 0xbd3af235);
        II(c, d, a, b, x[ 2], 15, 0x2ad7d2bb); II(b, c, d, a, x[ 9], 21, 0xeb86d391);

        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    }

    void update(const uint8_t* data, size_t len) {
        size_t idx = (count[0] >> 3) & 0x3f;
        uint32_t add = static_cast<uint32_t>(len) << 3;
        count[0] += add;
        if (count[0] < add) count[1]++;
        count[1] += static_cast<uint32_t>(len >> 29);
        for (size_t i = 0; i < len; i++) {
            buffer[idx++] = data[i];
            if (idx == 64) { transform(buffer); idx = 0; }
        }
    }

    std::vector<uint8_t> finalize() {
        static const uint8_t padding[64] = {0x80};
        uint8_t bits[8];
        for (int i = 0; i < 8; i++)
            bits[i] = static_cast<uint8_t>((count[i >> 2] >> ((i & 3) << 3)) & 0xff);

        size_t idx = (count[0] >> 3) & 0x3f;
        size_t padlen = (idx < 56) ? (56 - idx) : (120 - idx);
        update(padding, padlen);
        update(bits, 8);

        std::vector<uint8_t> digest(16);
        for (int i = 0; i < 16; i++)
            digest[i] = static_cast<uint8_t>((state[i >> 2] >> ((i & 3) << 3)) & 0xff);
        return digest;
    }
};

} // anonymous namespace

std::vector<uint8_t> md5_hash(const std::vector<uint8_t>& data) {
    MD5Context ctx;
    ctx.update(data.data(), data.size());
    return ctx.finalize();
}

std::string md5_hex(const std::vector<uint8_t>& data) {
    auto hash = md5_hash(data);
    const char* hex = "0123456789abcdef";
    std::string result(32, '0');
    for (int i = 0; i < 16; i++) {
        result[i*2]   = hex[(hash[i] >> 4) & 0xf];
        result[i*2+1] = hex[hash[i] & 0xf];
    }
    return result;
}

} // namespace ikemen::ssz_native::crypto
