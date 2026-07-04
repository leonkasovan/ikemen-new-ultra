// script_static.hpp — Static plugin registration for ssz_script/ssz/script.ssz
//
// When IKEMEN_NATIVE_SCRIPT_LIB=1, replaces the SSZ script.ssz module with
// native C++ entry points registered in the static plugin table.

#pragma once
#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_SCRIPT_LIB

extern "C" {

// script_init(L) — registers all 250+ Lua callbacks (sffNew, sndNew, fontNew,
// commandNew, inputDialog*, key query functions, set/get scores/timers/etc.)
void SSZ_STDCALL ScriptInit(PluginUtil* pu);

} // extern "C"

static inline bool script_static_register()
{
	SSZ_FunctionEntry entries[] = {
		{ "init", (void*)&ScriptInit },
	};
	return SSZ_RegisterFunction("script", entries, 1);
}

#else

static inline bool script_static_register()
{
	// IKEMEN_NATIVE_SCRIPT_LIB=0 — SSZ script.ssz used instead.
	return true;
}

#endif
