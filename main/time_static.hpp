#pragma once
//
// time_static.hpp
//
// Statically register every function exported by time.cpp
// so that the SSZ runtime resolves them without loading time.dll.
//

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_TIME_LIB

struct PluginUtil;

extern "C"
{
	uint32_t SSZ_STDCALL TickCount(PluginUtil*);
	int64_t  SSZ_STDCALL UnixTime(PluginUtil*);
}

inline bool time_static_register()
{
	static const SSZ_FunctionEntry time_mapping[] =
	{
		{ "TickCount", (void*)TickCount },
		{ "UnixTime",  (void*)UnixTime  },
	};

	return SSZ_RegisterFunction(
		"time",
		time_mapping,
		sizeof(time_mapping) / sizeof(time_mapping[0]));
}

#else
inline bool time_static_register() { return true; }
#endif
