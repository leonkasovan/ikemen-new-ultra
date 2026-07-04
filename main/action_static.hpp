// action_static.hpp — Static plugin registration for ssz_script/ssz/action.ssz
//
// When IKEMEN_NATIVE_ACTION_LIB=1, replaces the SSZ action.ssz module
// with native C++ entry points registered in the static plugin table.

#pragma once
#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_ACTION_LIB

extern "C" {

// init() — initializes the action animation engine.
void SSZ_STDCALL ActionInit(PluginUtil* pu);

} // extern "C"

static inline bool action_static_register()
{
	SSZ_FunctionEntry entries[] = {
		{ "init", (void*)&ActionInit },
	};
	return SSZ_RegisterFunction("action", entries, 1);
}

#else

static inline bool action_static_register()
{
	// IKEMEN_NATIVE_ACTION_LIB=0 — SSZ action.ssz used instead.
	return true;
}

#endif
