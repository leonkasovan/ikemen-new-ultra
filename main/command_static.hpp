// command_static.hpp — Static plugin registration for ssz_script/ssz/command.ssz
//
// When IKEMEN_NATIVE_COMMAND_LIB=1, replaces the SSZ command.ssz module
// with native C++ entry points registered in the static plugin table.

#pragma once
#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_COMMAND_LIB

extern "C" {

// init() — initializes command input processing system.
void SSZ_STDCALL CommandInit(PluginUtil* pu);

} // extern "C"

static inline bool command_static_register()
{
	SSZ_FunctionEntry entries[] = {
		{ "init", (void*)&CommandInit },
	};
	return SSZ_RegisterFunction("command", entries, 1);
}

#else

static inline bool command_static_register()
{
	// IKEMEN_NATIVE_COMMAND_LIB=0 — SSZ command.ssz used instead.
	return true;
}

#endif
