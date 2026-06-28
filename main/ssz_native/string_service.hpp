#pragma once

// string_service.hpp — Native equivalent of ssz_script/lib/string.ssz
//
// Provides:
// - String utilities: toLower, trim, split, join, find, equ
// - UTF-8 encode/decode (sToU8 / u8ToS)
// - Percent encoding/decoding
// - Number-to-string (hex, octal, decimal)
//
// Design note: string_service implements string operations via the C++
// standard library rather than the SSZ native plugin. The SSZ string library
// is pure SSZ script with no native plugin calls. Bypassing the SSZ runtime
// is therefore natural — there is no plugin layer to call through.

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen::ssz_native::string_util {

// ---- Character classification ----

// Convert a single character to lowercase (ASCII only, matching SSZ behavior).
inline char to_lower_char(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

// Convert all ASCII characters in a wide string to lowercase.
std::wstring to_lower(const std::wstring& str);

// ---- String operations ----

// Compare two strings for equality (length + content).
bool equ(const std::wstring& a, const std::wstring& b);

// Trim leading and trailing blank characters (space, tab, \r, \n).
std::wstring trim(const std::wstring& str);

// Find first occurrence of pattern in str. Returns position or -1.
intptr_t find(const std::wstring& pattern, const std::wstring& str);

// Split str by delimiter. Returns vector of substrings.
std::vector<std::wstring> split(const std::wstring& delimiter, const std::wstring& src);

// Split str by newline boundaries (\n, \r\n).
std::vector<std::wstring> split_lines(const std::wstring& str);

// Join vector of strings with delimiter between each pair.
std::wstring join(const std::wstring& delimiter, const std::vector<std::wstring>& parts);

// ---- Number-to-string ----

// Integer to octal string.
std::wstring to_octal(uint64_t value);

// Integer to lowercase hex string.
std::wstring to_hex_lower(uint64_t value);

// Integer to uppercase hex string.
std::wstring to_hex_upper(uint64_t value);

// ---- Encoding ----

// Encode a wide string to UTF-8 byte vector.
std::vector<uint8_t> to_utf8(const std::wstring& str);

// Decode UTF-8 byte vector to wide string.
std::wstring from_utf8(const std::vector<uint8_t>& utf8);

// Percent-encode a wide string (UTF-8 based).
std::wstring percent_encode(const std::wstring& str);

// Percent-decode to wide string.
std::wstring percent_decode(const std::wstring& str);

} // namespace ikemen::ssz_native::string_util
