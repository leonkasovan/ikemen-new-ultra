// char_static.hpp — Static plugin registration for ssz_script/ssz/char.ssz
//
// When IKEMEN_NATIVE_CHAR_LIB=1, replaces the SSZ char.ssz module with native
// C++ entry points registered in the static plugin table.

#pragma once
#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_CHAR_LIB

extern "C" {

// init(L) — initializes the character engine with a Lua state.
// Registers character state machine, animation, collision detection, etc.
void SSZ_STDCALL CharInit(PluginUtil* pu);

} // extern "C"

static inline bool char_static_register()
{
	SSZ_FunctionEntry entries[] = {
		{ "init", (void*)&CharInit },
	};
	return SSZ_RegisterFunction("char", entries, 1);
}

#else

static inline bool char_static_register()
{
	// IKEMEN_NATIVE_CHAR_LIB=0 — SSZ char.ssz used instead.
	return true;
}

#endif
