#pragma once

// crypto_service.hpp — Native equivalents of ssz_script/lib/base64.ssz and arcfour.ssz
//
// Provides:
// - Base64 encoding/decoding (standard alphabet)
// - Arcfour (RC4) stream cipher with RAII context
//
// Design note: Both functions are pure algorithm implementations with no SSZ
// plugin dependency. They are reimplemented natively via standard C++.

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen::ssz_native::crypto {

// ---- Base64 ----

// Encode binary data to base64 string.
std::string base64_encode(const std::vector<uint8_t>& data);

// Decode base64 string to binary data. Returns empty vector on invalid input.
std::vector<uint8_t> base64_decode(const std::string& b64);

// ---- Arcfour (RC4) ----

// RAII context for Arcfour stream cipher operations.
// Mirrors the &Arcfour object from ssz_script/lib/arcfour.ssz.
class Arcfour {
public:
    Arcfour() = default;

    // Initialize the cipher with a key.
    void init(const std::vector<uint8_t>& key);

    // Encrypt (or decrypt) a buffer in-place.
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& src);

    // Non-copyable, movable.
    Arcfour(const Arcfour&) = delete;
    Arcfour& operator=(const Arcfour&) = delete;

private:
    uint8_t state_[256] = {};
    uint8_t x_ = 0, y_ = 0;
};

// Convenience function: arcfourEnc(dest, key, src)
bool arcfour_encrypt(std::vector<uint8_t>& dest, const std::vector<uint8_t>& key, const std::vector<uint8_t>& src);

} // namespace ikemen::ssz_native::crypto
