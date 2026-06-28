#include "string_service.hpp"

#include <algorithm>
#include <cassert>

namespace ikemen::ssz_native::string_util {

// ---- Character classification ----

// ---- String operations ----

bool equ(const std::wstring& a, const std::wstring& b) {
    return a == b;
}

std::wstring trim(const std::wstring& str) {
    const std::wstring blanks = L" \t\r\n";
    size_t start = str.find_first_not_of(blanks);
    if (start == std::wstring::npos) return std::wstring();
    size_t end = str.find_last_not_of(blanks);
    return str.substr(start, end - start + 1);
}

intptr_t find(const std::wstring& pattern, const std::wstring& str) {
    size_t pos = str.find(pattern);
    return pos != std::wstring::npos ? static_cast<intptr_t>(pos) : -1;
}

std::vector<std::wstring> split(const std::wstring& delimiter, const std::wstring& src) {
    std::vector<std::wstring> parts;
    if (src.empty()) return parts;

    size_t delim_len = delimiter.size();
    size_t start = 0;
    while (start < src.size()) {
        size_t pos = delim_len > 0 ? src.find(delimiter, start) : start;
        if (pos == std::wstring::npos) {
            parts.push_back(src.substr(start));
            break;
        }
        parts.push_back(src.substr(start, pos - start));
        start = pos + delim_len + (delim_len == 0 ? 1 : 0);
    }
    return parts;
}

std::vector<std::wstring> split_lines(const std::wstring& str) {
    std::vector<std::wstring> lines;
    size_t i = 0;
    while (i < str.size()) {
        size_t start = i;
        while (i < str.size() && str[i] != L'\n' && str[i] != L'\r') i++;
        lines.push_back(str.substr(start, i - start));
        if (i < str.size() && str[i] == L'\r') i++;
        if (i < str.size() && str[i] == L'\n') i++;
    }
    return lines;
}

std::wstring join(const std::wstring& delimiter, const std::vector<std::wstring>& parts) {
    std::wstring result;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0) result += delimiter;
        result += parts[i];
    }
    return result;
}

std::wstring to_lower(const std::wstring& str) {
    // ASCII-only case conversion, matching SSZ behavior.
    // Non-ASCII characters are truncated to 7-bit. No Unicode case folding.
    std::wstring result;
    result.reserve(str.size());
    for (wchar_t c : str) {
        result += static_cast<wchar_t>(to_lower_char(static_cast<char>(c & 0x7f)));
    }
    return result;
}

// ---- Number-to-string ----

std::wstring to_octal(uint64_t value) {
    if (value == 0) return L"0";
    std::wstring result;
    while (value > 0) {
        result.insert(result.begin(), L'0' + static_cast<wchar_t>(value & 0x7));
        value >>= 3;
    }
    return result;
}

static std::wstring to_hex_impl(uint64_t value, bool upper) {
    if (value == 0) return L"0";
    const wchar_t* digits = upper ? L"0123456789ABCDEF" : L"0123456789abcdef";
    std::wstring result;
    while (value > 0) {
        result.insert(result.begin(), digits[value & 0xf]);
        value >>= 4;
    }
    return result;
}

std::wstring to_hex_lower(uint64_t value) { return to_hex_impl(value, false); }
std::wstring to_hex_upper(uint64_t value) { return to_hex_impl(value, true); }

// ---- Encoding ----

std::vector<uint8_t> to_utf8(const std::wstring& str) {
    std::vector<uint8_t> result;
    for (size_t i = 0; i < str.size(); i++) {
        uint32_t c = static_cast<uint32_t>(str[i]);

        // Handle surrogate pairs (UTF-16)
        if ((c >> 10) == 0x36 && i + 1 < str.size() && (static_cast<uint32_t>(str[i + 1]) >> 10) == 0x37) {
            c = ((c & 0x3ff) << 10 | (static_cast<uint32_t>(str[++i]) & 0x3ff)) + 0x10000;
        }

        if (c < 0x80) {
            result.push_back(static_cast<uint8_t>(c));
        } else if (c < 0x800) {
            result.push_back(static_cast<uint8_t>((c >> 6) | 0xc0));
            result.push_back(static_cast<uint8_t>((c & 0x3f) | 0x80));
        } else if (c < 0x10000) {
            result.push_back(static_cast<uint8_t>((c >> 12) | 0xe0));
            result.push_back(static_cast<uint8_t>(((c >> 6) & 0x3f) | 0x80));
            result.push_back(static_cast<uint8_t>((c & 0x3f) | 0x80));
        } else {
            result.push_back(static_cast<uint8_t>((c >> 18) | 0xf0));
            result.push_back(static_cast<uint8_t>(((c >> 12) & 0x3f) | 0x80));
            result.push_back(static_cast<uint8_t>(((c >> 6) & 0x3f) | 0x80));
            result.push_back(static_cast<uint8_t>((c & 0x3f) | 0x80));
        }
    }
    return result;
}

std::wstring from_utf8(const std::vector<uint8_t>& utf8) {
    std::wstring result;
    for (size_t i = 0; i < utf8.size(); i++) {
        uint32_t c = utf8[i];
        size_t extra = 0;

        if (c < 0xc0) {
            // 1-byte: 0xxxxxxx
        } else if (c < 0xe0) {
            c &= 0x1f;
            extra = 1;
        } else if (c < 0xf0) {
            c &= 0x0f;
            extra = 2;
        } else if (c < 0xf8) {
            c &= 0x07;
            extra = 3;
        }

        for (size_t j = 0; j < extra && i + 1 < utf8.size(); j++) {
            c = (c << 6) | (utf8[++i] & 0x3f);
        }

        if (c < 0x10000) {
            result.push_back(static_cast<wchar_t>(c));
        } else {
            // Encode as surrogate pair
            c -= 0x10000;
            result.push_back(static_cast<wchar_t>(((c >> 10) & 0x3ff) | 0xd800));
            result.push_back(static_cast<wchar_t>((c & 0x3ff) | 0xdc00));
        }
    }
    return result;
}

static bool is_unreserved(wchar_t c) {
    return (c >= L'A' && c <= L'Z') ||
           (c >= L'a' && c <= L'z') ||
           (c >= L'0' && c <= L'9') ||
           c == L'-' || c == L'.' || c == L'_' || c == L'~';
}

std::wstring percent_encode(const std::wstring& str) {
    auto utf8 = to_utf8(str);
    std::wstring result;
    const wchar_t* hex = L"0123456789ABCDEF";

    for (uint8_t byte : utf8) {
        wchar_t c = static_cast<wchar_t>(byte);
        if (is_unreserved(c)) {
            result += c;
        } else {
            result += L'%';
            result += hex[byte >> 4];
            result += hex[byte & 0xf];
        }
    }
    return result;
}

std::wstring percent_decode(const std::wstring& str) {
    std::vector<uint8_t> utf8;
    bool has_encoded = false;

    for (size_t i = 0; i < str.size(); i++) {
        if (str[i] == L'%' && i + 2 < str.size()) {
            has_encoded = true;
            auto hex_digit = [](wchar_t c) -> uint8_t {
                if (c >= L'0' && c <= L'9') return static_cast<uint8_t>(c - L'0');
                if (c >= L'A' && c <= L'F') return static_cast<uint8_t>(c - L'A' + 10);
                if (c >= L'a' && c <= L'f') return static_cast<uint8_t>(c - L'a' + 10);
                return 0;
            };
            const wchar_t high = str[++i];
            const wchar_t low = str[++i];
            uint8_t byte = (hex_digit(high) << 4) | hex_digit(low);
            utf8.push_back(byte);
        } else {
            if (has_encoded && !utf8.empty()) {
                // Continue filling the UTF-8 buffer
            }
            // Direct character (not percent-encoded) — collect as UTF-8 byte
            utf8.push_back(static_cast<uint8_t>(str[i] & 0xff));
        }
    }

    if (has_encoded || !utf8.empty()) {
        return from_utf8(utf8);
    }
    return str;
}

} // namespace ikemen::ssz_native::string_util
