// font_static.hpp — Static plugin registration for ssz_script/ssz/font.ssz
//
// When IKEMEN_NATIVE_FONT_LIB=1, replaces the SSZ font.ssz module
// with native C++ entry points registered in the static plugin table.

#pragma once
#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_FONT_LIB

extern "C" {

// init() — initializes font rendering engine.
void SSZ_STDCALL FontInit(PluginUtil* pu);

// renderText() — renders text at the given position.
void SSZ_STDCALL FontRenderText(PluginUtil* pu);

} // extern "C"

static inline bool font_static_register()
{
	SSZ_FunctionEntry entries[] = {
		{ "init",        (void*)&FontInit },
		{ "renderText",  (void*)&FontRenderText },
	};
	return SSZ_RegisterFunction("font", entries, 2);
}

#else

static inline bool font_static_register()
{
	// IKEMEN_NATIVE_FONT_LIB=0 — SSZ font.ssz used instead.
	return true;
}

#endif
