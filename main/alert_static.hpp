#pragma once
//
// alert_static.hpp
//
// Statically register every function exported by alert.cpp
// so that the SSZ runtime resolves them without loading alert.dll.
//

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_ALERT_LIB

struct PluginUtil;
struct Reference;

extern "C"
{
	void SSZ_STDCALL Alert(PluginUtil*, Reference, Reference);
}

inline bool alert_static_register()
{
	static const SSZ_FunctionEntry alert_mapping[] =
	{
		{ "Alert", (void*)Alert },
	};

	return SSZ_RegisterFunction(
		"alert",
		alert_mapping,
		sizeof(alert_mapping) / sizeof(alert_mapping[0]));
}

#else
inline bool alert_static_register() { return true; }
#endif
