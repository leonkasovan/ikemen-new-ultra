// trigger_script_static.hpp — Static plugin registration for ssz_script/ssz/trigger-script.ssz
//
// When IKEMEN_NATIVE_TRIGGER_SCRIPT_LIB=1, replaces the SSZ trigger-script.ssz
// module with native C++ entry points registered in the static plugin table.

#pragma once
#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_TRIGGER_SCRIPT_LIB

extern "C" {

// register_function(L) — registers all 170+ trigger Lua callbacks (player,
// parent, root, helper, target, alive, anim, life, power, pos, vel, fvar,
// sysfvar, tvar, var, stateno, statetype, movetype, and many more).
void SSZ_STDCALL TriggerScriptRegisterFunction(PluginUtil* pu);

} // extern "C"

static inline bool trigger_script_static_register()
{
	SSZ_FunctionEntry entries[] = {
		{ "registerFunction", (void*)&TriggerScriptRegisterFunction },
	};
	return SSZ_RegisterFunction("trigger_script", entries, 1);
}

#else

static inline bool trigger_script_static_register()
{
	// IKEMEN_NATIVE_TRIGGER_SCRIPT_LIB=0 — SSZ trigger-script.ssz used instead.
	return true;
}

#endif
