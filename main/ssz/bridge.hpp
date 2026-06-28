#pragma once

#include <string>

#include "sszdef.h"
#include "typeid.h"
#include "arrayandref.hpp"
#include "pluginutil.hpp"

struct PluginUtil;
struct Reference;

namespace ikemen::ssz_bridge {

// Describes a single regex match by its position and length within the
// original input string, measured in WCHR (UTF-16 code unit) positions.
// Used to pass match results from the native RegexSearch back to the
// bridge wrapper, which creates SSZ Reference slices from them.
struct RegexMatchInfo
{
    intptr_t pos;   // match start position in WCHR units
    intptr_t len;   // match length in WCHR units
};

// Convert an SSZ Reference string into a native std::wstring for bridge
// wrappers. Prefer the PluginUtil instance when available because it is the
// old ABI's conversion/context object; fall back to PluginUtil's static helper
// for wrapper tests or future call sites that do not need VM context.
inline std::wstring refToWstring(PluginUtil* pu, Reference r)
{
    return pu != nullptr ? pu->refToWstr(r) : PluginUtil::refToWstr(r);
}

// Convert an SSZ Reference string into a native narrow std::string (UTF-8 on
// Windows, locale-dependent multibyte on Linux) for bridge wrappers that need
// to pass strings to C APIs such as getaddrinfo / socket functions.
inline std::string refToNarrowUtf8(PluginUtil* pu, Reference r)
{
#ifdef _WIN32
    return pu->refToAstr(CP_UTF8, r);
#else
    return PluginUtil::wToA(PluginUtil::refToWstr(r));
#endif
}

} // namespace ikemen::ssz_bridge
