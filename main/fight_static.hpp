// fight_static.hpp — Static plugin registration for ssz_script/ssz/fight.ssz
//
// When IKEMEN_NATIVE_FIGHT_LIB=1, replaces the SSZ fight.ssz module
// with native C++ entry points registered in the static plugin table.

#pragma once
#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_FIGHT_LIB

extern "C" {

// init() — initializes the fight engine (lifebars, round, combos, etc).
void SSZ_STDCALL FightInit(PluginUtil* pu);

} // extern "C"

static inline bool fight_static_register()
{
	SSZ_FunctionEntry entries[] = {
		{ "init", (void*)&FightInit },
	};
	return SSZ_RegisterFunction("fight", entries, 1);
}

#else

static inline bool fight_static_register()
{
	// IKEMEN_NATIVE_FIGHT_LIB=0 — SSZ fight.ssz used instead.
	return true;
}

#endif
