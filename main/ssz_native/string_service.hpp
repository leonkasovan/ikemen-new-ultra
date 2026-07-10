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
//
// When IKEMEN_NATIVE_STRING_LIB=0, this entire header becomes a no-op
// and ssz_script/lib/string.ssz is used instead.

#if IKEMEN_NATIVE_STRING_LIB

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <type_traits>
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

// Scan str starting at index for a newline. Returns 1 for \n, 2 for \r\n, 0 if none.
// Advances idx past the newline.
int next_line(intptr_t& idx, const std::wstring& str);

// Returns true if item is found in cclass (character-class membership test).
template <typename T>
bool c_match(const std::vector<T>& cclass, const T& item) {
    for (const auto& c : cclass)
        if (c == item) return true;
    return false;
}

inline bool c_match(const std::wstring& cclass, wchar_t item) {
    for (wchar_t c : cclass)
        if (c == item) return true;
    return false;
}

// Returns index of first char in str matching any char in cclass, or -1.
intptr_t c_find(const std::wstring& cclass, const std::wstring& str);

// ---- String-to-number ----

// Parse string into numeric value. Returns true on success.
bool s_to_number(double& out, const std::wstring& s);
bool s_to_number(int32_t& out, const std::wstring& s);
bool s_to_number(int64_t& out, const std::wstring& s);
bool s_to_number(float& out, const std::wstring& s);

// Convenience: parse string to number, returning 0 on failure.
template <typename T>
T s_to_n(const std::wstring& s) {
    T d = static_cast<T>(0);
    s_to_number(d, s);
    return d;
}

// Split string by delimiter, convert each piece to T.
template <typename T>
std::vector<T> sv_to_ary(const std::wstring& delimiter, const std::wstring& v) {
    auto parts = split(delimiter, v);
    std::vector<T> result;
    result.reserve(parts.size());
    for (const auto& p : parts)
        result.push_back(s_to_n<T>(p));
    return result;
}

// ---- Array operations ----

// Copy elements from src into dist, up to the shorter length.
template <typename T>
void copy_array(std::vector<T>& dist, const std::vector<T>& src) {
    size_t len = dist.size() < src.size() ? dist.size() : src.size();
    for (size_t i = 0; i < len; i++)
        dist[i] = src[i];
}

// Clone: return a copy of src.
template <typename T>
std::vector<T> clone_array(const std::vector<T>& src) {
    return src;
}

// Apply callback to each element (by reference, allowing mutation). Returns the array.
template <typename T>
std::vector<T>& each(const std::function<void(T&)>& callback, std::vector<T>& ary) {
    for (auto& elem : ary)
        callback(elem);
    return ary;
}

// ---- Format object (printf-style formatter) ----
//
// Matches the SSZ &Format object from ssz_script/lib/string.ssz.
// Supports %d, %i, %u, %o, %x, %X, %c, %s, %f, %F, %e, %E, %g, %G
// with flags: 0, -, +, space, #  and width/precision.
//
class Format {
public:
    std::wstring out;   // Accumulated output buffer (public, like SSZ)

    char set(const std::wstring& format);
    bool isError() const { return next_ == L'\x7f'; }
    char d(int64_t i);
    char u(uint64_t u);
    char f(double val);
    char c(wchar_t ch);
    char s(const std::wstring& str);
    void putSpace(int n);

private:
    char  setError()    { return next_ = L'\x7f'; }
    char  setNext();
    void  putStr(const std::wstring& str);

    std::wstring fmt_;
    wchar_t next_  = L'\0';
    int     acc_   = -1;
    int     width_ = 0;
    int     sign_  = 0;   // 0=none, 1=+, -1=space
    bool    zero_  = false;
    bool    left_  = false;
    bool    sharp_ = false;
};

// Integer to octal string.
std::wstring to_octal(uint64_t value);

// Integer to lowercase hex string.
std::wstring to_hex_lower(uint64_t value);

// Integer to uppercase hex string.
std::wstring to_hex_upper(uint64_t value);

// Convert array of any trivial type to lowercase hex string (big-endian per element).
template <typename T>
std::wstring to_hex(const std::vector<T>& src) {
    static_assert(std::is_trivially_copyable<T>::value, "to_hex requires trivially copyable type");
    std::wstring result;
    const wchar_t* hex = L"0123456789abcdef";
    for (const auto& elem : src) {
        for (int j = static_cast<int>(sizeof(T)) * 8 - 4; j >= 0; j -= 4) {
            uint64_t v;
            if constexpr (std::is_signed<T>::value) {
                v = static_cast<uint64_t>(static_cast<typename std::make_unsigned<T>::type>(elem)) >> j;
            } else {
                v = static_cast<uint64_t>(elem) >> j;
            }
            result += hex[v & 0xf];
        }
    }
    return result;
}

// Convert array of any trivial type to byte array (little-endian).
template <typename T>
std::vector<uint8_t> to_ubyte(const std::vector<T>& src) {
    static_assert(std::is_trivially_copyable<T>::value, "to_ubyte requires trivially copyable type");
    std::vector<uint8_t> result;
    result.reserve(src.size() * sizeof(T));
    for (const auto& elem : src) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&elem);
        for (size_t j = 0; j < sizeof(T); j++)
            result.push_back(p[j]);
    }
    return result;
}

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

#endif // IKEMEN_NATIVE_STRING_LIB
