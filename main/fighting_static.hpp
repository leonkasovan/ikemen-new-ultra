// fighting_static.hpp — Static plugin registration for ssz_script/ssz/fighting.ssz
//
// When IKEMEN_NATIVE_FIGHTING_LIB=1, replaces the SSZ fighting.ssz module
// with native C++ entry points registered in the static plugin table.

#pragma once
#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_FIGHTING_LIB

extern "C" {

// init() — initializes the fight/orchestration engine, placing the main()
// public entry point that drives match flow.
void SSZ_STDCALL FightingInit(PluginUtil* pu);

} // extern "C"

static inline bool fighting_static_register()
{
	SSZ_FunctionEntry entries[] = {
		{ "init", (void*)&FightingInit },
	};
	return SSZ_RegisterFunction("fighting", entries, 1);
}

#else

static inline bool fighting_static_register()
{
	// IKEMEN_NATIVE_FIGHTING_LIB=0 — SSZ fighting.ssz used instead.
	return true;
}

#endif
