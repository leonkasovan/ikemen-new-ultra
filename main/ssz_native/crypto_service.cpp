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

} // namespace ikemen::ssz_native::crypto
