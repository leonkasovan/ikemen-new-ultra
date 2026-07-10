#pragma once
//
// thread_static.hpp
//
// Statically register every function exported by ssz_script/lib/thread.ssz
// so that the SSZ runtime resolves them without loading thread.dll.
//
// When IKEMEN_NATIVE_THREAD_LIB=1, native thread delay replaces the native
// plugin call (ThreadDelay). When 0, the SSZ script is used instead.
//
// NOTE: thread.cpp's "Delay" was renamed to "ThreadDelay" in the C symbol
//       to avoid a linker clash with sdlplugin.cpp's "Delay".
//       The SSZ mapping still maps "Delay" -> ThreadDelay.

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_THREAD_LIB

struct PluginUtil;

extern "C"
{
	void SSZ_STDCALL ThreadDelay(PluginUtil*, uint32_t);
}

inline bool thread_static_register()
{
	static const SSZ_FunctionEntry thread_mapping[] =
	{
		{ "Delay", (void*)ThreadDelay },
	};

	return SSZ_RegisterFunction(
		"thread",
		thread_mapping,
		sizeof(thread_mapping) / sizeof(thread_mapping[0]));
}

#else

static inline bool thread_static_register()
{
	// IKEMEN_NATIVE_THREAD_LIB=0 — SSZ thread.ssz script used instead.
	return true;
}

#endif
