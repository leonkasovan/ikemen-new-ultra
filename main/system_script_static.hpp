// system_script_static.hpp — Static plugin registration for ssz_script/ssz/system-script.ssz
//
// When IKEMEN_NATIVE_SYSTEM_SCRIPT_LIB=1, replaces the SSZ system-script.ssz
// module with native C++ entry points registered in the static plugin table.

#pragma once
#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_SYSTEM_SCRIPT_LIB

extern "C" {

// init(L) — initializes the system script module with a Lua state.
// Registers all 200+ system-level Lua callbacks (game loop, rendering,
// audio, select screen, pause menu, results screen, etc.).
void SSZ_STDCALL SystemScriptInit(PluginUtil* pu);

} // extern "C"

static inline bool system_script_static_register()
{
	SSZ_FunctionEntry entries[] = {
		{ "init", (void*)&SystemScriptInit },
	};
	return SSZ_RegisterFunction("system_script", entries, 1);
}

#else

static inline bool system_script_static_register()
{
	// IKEMEN_NATIVE_SYSTEM_SCRIPT_LIB=0 — SSZ system-script.ssz used instead.
	return true;
}

#endif
