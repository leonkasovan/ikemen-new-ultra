// bg_static.hpp — Static plugin registration for ssz_script/ssz/bg.ssz
//
// When IKEMEN_NATIVE_BG_LIB=1, replaces the SSZ bg.ssz module
// with native C++ entry points registered in the static plugin table.

#pragma once
#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_BG_LIB

#include <cstdint>
struct PluginUtil;   // forward (from pluginutil.hpp)
struct Reference;    // forward (from arrayandref.hpp)

extern "C" {

// init() — initializes background rendering system.
void SSZ_STDCALL BgInit(PluginUtil* pu);

// splitParams(paramStr) — split a parameter string into an array.
void SSZ_STDCALL BgSplitParams(PluginUtil* pu, Reference paramStr, Reference* out);

} // extern "C"

static inline bool bg_static_register()
{
	SSZ_FunctionEntry entries[] = {
		{ "init",        (void*)&BgInit        },
		{ "splitParams", (void*)&BgSplitParams },
	};
	return SSZ_RegisterFunction("bg", entries, 2);
}

#else

static inline bool bg_static_register()
{
	// IKEMEN_NATIVE_BG_LIB=0 — SSZ bg.ssz used instead.
	return true;
}

#endif
