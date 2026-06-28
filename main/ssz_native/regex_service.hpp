#pragma once

// regex_service.hpp — Native equivalent of ssz_script/lib/regex.ssz
//
// Provides:
// - RAII wrapper for std::wregex (boost::wregex on Linux)
// - pattern compilation with error reporting
// - search returning matched substrings or raw position/length pairs
//
// Design note: regex_service creates std::wregex/boost::wregex directly
// rather than calling the SSZ native plugin (main/regex/regex.cpp's NewRegex).
// The SSZ native plugin uses the same ECMAScript+optimize flags and identical
// error-handling patterns. This module compiles and tests without linking
// against regex.o, making it independent of the SSZ runtime.
// If main/regex/regex.cpp ever adds SSZ-specific behavior, the Regex::compile
// implementation must be updated to call through plugin_native_api.hpp instead.

#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#include <regex>
#define SSZ_REGEX_NS std
#else
#include <boost/regex.hpp>
#define SSZ_REGEX_NS boost
#endif

namespace ikemen::ssz_native::regex {

// A single match result: position and length in WCHR (UTF-16 code unit) units.
// Semantically identical to ikemen::ssz_bridge::RegexMatchInfo (bridge.hpp).
// TODO: Consolidate with RegexMatchInfo when the SSZ bridge layer is retired.
struct Match {
    intptr_t pos = 0;
    intptr_t len = 0;
};

// RAII wrapper around a compiled regular expression.
// Mirrors the &Regex object from ssz_script/lib/regex.ssz.
class Regex {
public:
    Regex() = default;

    // Compile a pattern. Returns an error string on failure (empty on success).
    // case_insensitive: if true, enables case-insensitive matching (SSZ flag I=1).
    std::wstring compile(const std::wstring& pattern, bool case_insensitive = false);

    // Search str. Returns ALL capture groups from the first match as substrings.
    // Group 0 is the full match; groups 1..N are captured subexpressions.
    // Returns empty vector if no match. Matches SSZ RegexSearch behavior.
    std::vector<std::wstring> search(const std::wstring& str) const;

    // Search str and return raw position/length pairs for all capture groups.
    // Extension: not provided by the SSZ native plugin (RegexSearch returns one
    // match only). Added at the service layer for convenience.
    std::vector<Match> search_matches(const std::wstring& str) const;

    // Search str iteratively for all non-overlapping matches.
    // Returns the full match text (group 0) for each match found.
    // Extension: not provided by the SSZ native plugin. Added at the service
    // layer for convenience.
    std::vector<std::wstring> search_all(const std::wstring& str) const;

    // Returns true if the regex is compiled and ready.
    bool is_compiled() const { return re_ != nullptr; }

    ~Regex() { clear(); }

    // Non-copyable, movable.
    Regex(const Regex&) = delete;
    Regex& operator=(const Regex&) = delete;
    Regex(Regex&& other) noexcept : re_(other.re_) { other.re_ = nullptr; }
    Regex& operator=(Regex&& other) noexcept {
        if (this != &other) { clear(); re_ = other.re_; other.re_ = nullptr; }
        return *this;
    }

private:
    void clear();

    SSZ_REGEX_NS::wregex* re_ = nullptr;
};

// Free function: compile a regex and return {regex, error_string}.
// Convenience wrapper when RAII ownership isn't needed for one-off searches.
std::pair<Regex, std::wstring> compile(const std::wstring& pattern, bool case_insensitive = false);

} // namespace ikemen::ssz_native::regex

#undef SSZ_REGEX_NS
