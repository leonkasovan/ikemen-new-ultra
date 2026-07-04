// statebuilder_static.hpp — Static plugin registration for ssz_script/ssz/statebuilder.ssz
//
// When IKEMEN_NATIVE_STATEBUILDER_LIB=1, replaces the SSZ statebuilder.ssz module
// with native C++ entry points registered in the static plugin table.

#pragma once
#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_STATEBUILDER_LIB

extern "C" {

// init() — initializes the state machine builder engine.
void SSZ_STDCALL StateBuilderInit(PluginUtil* pu);

} // extern "C"

static inline bool statebuilder_static_register()
{
	SSZ_FunctionEntry entries[] = {
		{ "init", (void*)&StateBuilderInit },
	};
	return SSZ_RegisterFunction("statebuilder", entries, 1);
}

#else

static inline bool statebuilder_static_register()
{
	// IKEMEN_NATIVE_STATEBUILDER_LIB=0 — SSZ statebuilder.ssz used instead.
	return true;
}

#endif
