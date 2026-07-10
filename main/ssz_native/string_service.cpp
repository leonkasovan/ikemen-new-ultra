#include "string_service.hpp"

#if IKEMEN_NATIVE_STRING_LIB

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

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

// ---- Next line ----

int next_line(intptr_t& idx, const std::wstring& str) {
    while (idx < static_cast<intptr_t>(str.size())) {
        if (str[idx] == L'\n') {
            idx++;
            return 1;
        }
        if (str[idx] == L'\r') {
            if (idx + 1 < static_cast<intptr_t>(str.size()) && str[idx + 1] == L'\n') {
                idx += 2;
                return 2;
            }
            idx++;
            return 1;
        }
        idx++;
    }
    return 0;
}

// ---- Character find ----

intptr_t c_find(const std::wstring& cclass, const std::wstring& str) {
    for (size_t i = 0; i < str.size(); i++) {
        if (c_match(cclass, str[i]))
            return static_cast<intptr_t>(i);
    }
    return -1;
}

// ---- String-to-number ----

bool s_to_number(double& out, const std::wstring& s) {
    std::wstring t = trim(s);
    if (t.empty()) return false;
    wchar_t* end = nullptr;
    double val = std::wcstod(t.c_str(), &end);
    if (end == t.c_str()) return false;
    out = val;
    return true;
}

bool s_to_number(float& out, const std::wstring& s) {
    double d;
    if (!s_to_number(d, s)) return false;
    out = static_cast<float>(d);
    return true;
}

bool s_to_number(int32_t& out, const std::wstring& s) {
    std::wstring t = trim(s);
    if (t.empty()) return false;
    wchar_t* end = nullptr;
    long val = std::wcstol(t.c_str(), &end, 10);
    if (end == t.c_str()) return false;
    out = static_cast<int32_t>(val);
    return true;
}

bool s_to_number(int64_t& out, const std::wstring& s) {
    std::wstring t = trim(s);
    if (t.empty()) return false;
    wchar_t* end = nullptr;
    long long val = std::wcstoll(t.c_str(), &end, 10);
    if (end == t.c_str()) return false;
    out = static_cast<int64_t>(val);
    return true;
}

// =====================================================================
// Format object — printf-style formatter (ssz_script/lib/string.ssz &Format)
// =====================================================================

char Format::set(const std::wstring& format) {
    fmt_ = format;
    out.clear();
    next_ = L'\0';
    return setNext();
}

char Format::setNext() {
    if (isError()) return next_;

    // Copy literal text up to the next '%', then parse the specifier
    while (true) {
        {
            size_t pct = fmt_.find(L'%');
            out.append(fmt_, 0, pct);
            if (pct == std::wstring::npos) {
                fmt_.clear();
                return next_ = L'\0';
            }
            fmt_ = fmt_.substr(pct + 1);
        }

        if (fmt_.empty()) return setError();

        // Reset flags
        sign_ = 0;
        zero_ = left_ = sharp_ = false;
        width_ = 0;
        acc_ = -1;

        size_t i = 0;

        // Parse flags
        while (i < fmt_.size()) {
            wchar_t c = fmt_[i];
            if (c == L'0')       { zero_ = true; }
            else if (c == L'-')  { left_ = true; }
            else if (c == L'+')  { sign_ = 1; }
            else if (c == L' ')  { if (sign_ == 0) sign_ = -1; }
            else if (c == L'#')  { sharp_ = true; }
            else break;
            i++;
        }

        // Parse width
        while (i < fmt_.size() && fmt_[i] >= L'0' && fmt_[i] <= L'9') {
            width_ = width_ * 10 + static_cast<int>(fmt_[i] - L'0');
            i++;
        }

        // Parse precision
        if (i < fmt_.size() && fmt_[i] == L'.') {
            i++;
            acc_ = 0;
            while (i < fmt_.size() && fmt_[i] >= L'0' && fmt_[i] <= L'9') {
                acc_ = acc_ * 10 + static_cast<int>(fmt_[i] - L'0');
                i++;
            }
        }

        // Skip length modifier
        if (i < fmt_.size() && (fmt_[i] == L'h' || fmt_[i] == L'l' || fmt_[i] == L'L'))
            i++;

        // Handle %% escape
        if (i < fmt_.size() && fmt_[i] == L'%') {
            out += L'%';
            fmt_ = fmt_.substr(i + 1);
            continue; // restart the loop to look for more literal text
        }

        // Validate conversion specifier
        static const std::wstring valid = L"diuoxXcsfFeEgG";
        if (i >= fmt_.size() || c_match(valid, fmt_[i]) == false)
            return setError();

        next_ = fmt_[i];
        fmt_ = fmt_.substr(i + 1);
        return next_;
    }
}

void Format::putSpace(int n) {
    for (int i = 0; i < n; i++)
        out += L' ';
}

void Format::putStr(const std::wstring& str) {
    int pad = width_ - static_cast<int>(str.size());
    if (!left_ && pad > 0) putSpace(pad);
    out += str;
    if (left_ && pad > 0) putSpace(pad);
}

char Format::d(int64_t i) {
    if (!c_match(std::wstring(L"di"), next_)) {
        if (c_match(std::wstring(L"uoxX"), next_))
            return u(static_cast<uint64_t>(i));
        if (c_match(std::wstring(L"fFeEgG"), next_))
            return f(static_cast<double>(i));
        if (next_ == L'c')  return c(static_cast<wchar_t>(i));
        if (next_ == L's')  return s(std::wstring(1, static_cast<wchar_t>(i)));
        return setError();
    }

    // Absolute value
    uint64_t abs_val;
    bool negative = (i < 0);
    if (negative) {
        abs_val = static_cast<uint64_t>(-(i + 1)) + 1;
    } else {
        abs_val = static_cast<uint64_t>(i);
    }

    // Build sign prefix
    std::wstring prefix;
    if (negative) {
        prefix += L'-';
    } else if (sign_ == 1) {
        prefix += L'+';
    } else if (sign_ == -1) {
        prefix += L' ';
    }

    // Digit string
    std::wstring str = std::to_wstring(abs_val);

    // Determine minimum digit count (precision or zero-pad)
    int min_digits = acc_;
    if (zero_ && min_digits + static_cast<int>(prefix.size()) < width_)
        min_digits = width_ - static_cast<int>(prefix.size());

    // Apply zero-padding to digits
    int pad_count = min_digits - static_cast<int>(str.size());
    if (pad_count > 0)
        str = std::wstring(pad_count, L'0') + str;

    putStr(prefix + str);
    return setNext();
}

char Format::u(uint64_t u) {
    if (!c_match(std::wstring(L"uoxX"), next_)) {
        if (c_match(std::wstring(L"di"), next_))
            return d(static_cast<int64_t>(u));
        if (c_match(std::wstring(L"fFeEgG"), next_))
            return f(static_cast<double>(u));
        if (next_ == L'c')  return c(static_cast<wchar_t>(u));
        if (next_ == L's')  return s(std::wstring(1, static_cast<wchar_t>(u)));
        return setError();
    }

    std::wstring buf;

    // Sign prefix for unsigned (only + or space, never -)
    if (sign_ != 0)
        buf += (sign_ > 0 ? L'+' : L' ');

    // Convert number to string based on format specifier
    std::wstring str;
    switch (next_) {
    case L'u': str = std::to_wstring(u); break;
    case L'o': str = to_octal(u); break;
    case L'x': str = to_hex_lower(u); break;
    case L'X': str = to_hex_upper(u); break;
    }

    // Sharp flag: alternate form prefixes
    if (sharp_) {
        switch (next_) {
        case L'o':
            if (static_cast<int>(str.size()) >= acc_) buf += L'0';
            break;
        case L'x': buf += L"0x"; break;
        case L'X': buf += L"0X"; break;
        }
    }

    // Zero-padding
    int min_digits = acc_;
    if (zero_ && min_digits + static_cast<int>(buf.size()) < width_)
        min_digits = width_ - static_cast<int>(buf.size());
    int pad_count = min_digits - static_cast<int>(str.size());
    if (pad_count > 0)
        str = std::wstring(pad_count, L'0') + str;

    buf += str;
    putStr(buf);
    return setNext();
}

char Format::f(double val) {
    if (!c_match(std::wstring(L"fFeEgG"), next_)) {
        if (c_match(std::wstring(L"di"), next_))
            return d(static_cast<int64_t>(val));
        if (c_match(std::wstring(L"uoxX"), next_))
            return u(static_cast<uint64_t>(val));
        if (next_ == L'c')  return c(static_cast<wchar_t>(val));
        return setError();
    }

    // Build sign prefix manually
    std::wstring prefix;
    bool negative = (val < 0.0);
    if (negative) {
        prefix += L'-';
    } else if (sign_ == 1) {
        prefix += L'+';
    } else if (sign_ == -1) {
        prefix += L' ';
    }

    double abs_val = std::fabs(val);

    // NaN
    if (std::isnan(val)) {
        const wchar_t* nan_str = (next_ >= L'a' && next_ <= L'z') ? L"nan" : L"NAN";
        putStr(prefix + nan_str);
        return setNext();
    }

    // Infinity
    if (!std::isfinite(val)) {
        const wchar_t* inf_str = (next_ >= L'a' && next_ <= L'z') ? L"inf" : L"INF";
        putStr(prefix + inf_str);
        return setNext();
    }

    // Build printf format for the ABSOLUTE value (sign handled separately)
    int precision = (acc_ < 0) ? 6 : acc_;
    char fmt_buf[32];
    int fi = 0;
    fmt_buf[fi++] = '%';
    if (sharp_) fmt_buf[fi++] = '#';
    fmt_buf[fi++] = '.';
    fi += std::sprintf(fmt_buf + fi, "%d", precision);
    fmt_buf[fi++] = static_cast<char>(next_); // f, F, e, E, g, G
    fmt_buf[fi] = '\0';

    char narrow_out[512];
    int written = std::snprintf(narrow_out, sizeof(narrow_out), fmt_buf, abs_val);
    if (written < 0 || written >= static_cast<int>(sizeof(narrow_out)))
        return setError();

    std::wstring str(narrow_out, narrow_out + written);

    // handle zero-padding: if zero_ flag is set, expand to reach width_
    if (zero_ && !left_) {
        int total = static_cast<int>(prefix.size() + str.size());
        int need = width_ - total;
        if (need > 0)
            str = std::wstring(need, L'0') + str;
    }

    putStr(prefix + str);
    return setNext();
}

char Format::c(wchar_t ch) {
    if (next_ != L'c' && next_ != L's') {
        if (c_match(std::wstring(L"di"), next_))
            return d(static_cast<int64_t>(ch));
        if (c_match(std::wstring(L"uoxX"), next_))
            return u(static_cast<uint64_t>(ch));
        if (c_match(std::wstring(L"fFeEgG"), next_))
            return f(static_cast<double>(ch));
        return setError();
    }
    putStr(std::wstring(1, ch));
    return setNext();
}

char Format::s(const std::wstring& str) {
    if (next_ != L's') return setError();
    putStr(str);
    return setNext();
}

} // namespace ikemen::ssz_native::string_util

#endif // IKEMEN_NATIVE_STRING_LIB
