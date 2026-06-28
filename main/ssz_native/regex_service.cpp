#include "regex_service.hpp"

#ifdef _WIN32
#include <windows.h>   // MultiByteToWideChar, CP_THREAD_ACP
#define SSZ_REGEX_NS std
#else
#define SSZ_REGEX_NS boost
#endif

// SSZ_REGEX_NS must be redefined here: regex_service.hpp defines it for the
// class definition but #undef's it at EOF to prevent macro leakage.

namespace ikemen::ssz_native::regex {

std::wstring Regex::compile(const std::wstring& pattern, bool case_insensitive) {
    clear();
    try {
        auto flags = SSZ_REGEX_NS::wregex::ECMAScript | SSZ_REGEX_NS::wregex::optimize;
        if (case_insensitive)
            flags |= SSZ_REGEX_NS::wregex::icase;
        re_ = new SSZ_REGEX_NS::wregex(pattern, flags);
        return std::wstring();
    } catch (const SSZ_REGEX_NS::regex_error& e) {
#ifdef _WIN32
        int len = MultiByteToWideChar(CP_THREAD_ACP, 0, e.what(), -1, nullptr, 0);
        if (len > 0) {
            std::wstring error;
            error.resize(len - 1);
            MultiByteToWideChar(CP_THREAD_ACP, 0, e.what(), -1, &error[0], len);
            return error;
        }
        return L"regex error";
#else
    // Known limitation: regex_service is independent of the SSZ runtime
    // and cannot use PluginUtil for charset conversion. The SSZ native
    // plugin (main/regex/regex.cpp) produces a localized error message via
    // PluginUtil::wToGw(PluginUtil::aToW(e.what())). Returning a generic
    // message here is a deliberate trade-off for runtime independence.
    return L"regex error";
#endif
    }
}

void Regex::clear() {
    delete re_;
    re_ = nullptr;
}

std::vector<std::wstring> Regex::search(const std::wstring& str) const {
    // Returns ALL capture groups from the FIRST match.
    // Matches SSZ RegexSearch behavior (group 0 = full match, 1..N = captures).
    std::vector<std::wstring> results;
    if (!re_) return results;

    SSZ_REGEX_NS::wcmatch match;
    auto first = str.c_str();
    auto last = first + str.size();
    if (SSZ_REGEX_NS::regex_search(first, last, match, *re_)) {
        results.reserve(match.size());
        for (size_t i = 0; i < match.size(); ++i) {
            results.push_back(std::wstring(match[i].first, match[i].second));
        }
    }
    return results;
}

std::vector<Match> Regex::search_matches(const std::wstring& str) const {
    // Iteratively finds all non-overlapping matches and returns absolute
    // position/length for the full match (group 0) of each.
    std::vector<Match> results;
    if (!re_) return results;

    const wchar_t* base = str.c_str();
    SSZ_REGEX_NS::wcmatch match;
    auto first = base;
    auto last = base + str.size();
    while (SSZ_REGEX_NS::regex_search(first, last, match, *re_)) {
        Match m;
        m.pos = static_cast<intptr_t>(match[0].first - base);
        m.len = static_cast<intptr_t>(match[0].second - match[0].first);
        results.push_back(m);
        first = match[0].second;
    }
    return results;
}

std::vector<std::wstring> Regex::search_all(const std::wstring& str) const {
    // Iteratively finds all non-overlapping matches (full match text only).
    std::vector<std::wstring> results;
    if (!re_) return results;

    SSZ_REGEX_NS::wcmatch match;
    auto first = str.c_str();
    auto last = first + str.size();
    auto it = first;
    while (SSZ_REGEX_NS::regex_search(it, last, match, *re_)) {
        results.push_back(std::wstring(match[0].first, match[0].second));
        it = match[0].second;
    }
    return results;
}

std::pair<Regex, std::wstring> compile(const std::wstring& pattern, bool case_insensitive) {
    Regex r;
    std::wstring error = r.compile(pattern, case_insensitive);
    return {std::move(r), std::move(error)};
}

} // namespace ikemen::ssz_native::regex

#undef SSZ_REGEX_NS
