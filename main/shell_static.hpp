#pragma once
//
// shell_static.hpp
//
// Statically register every function exported by shell.cpp
// so that the SSZ runtime resolves them without loading shell.dll.
//

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_SHELL_LIB

struct PluginUtil;
struct Reference;

extern "C"
{
	bool SSZ_STDCALL ShellOpen(PluginUtil*, bool, bool, Reference, Reference, Reference);
	bool SSZ_STDCALL MoveTrash(PluginUtil*, Reference);
}

inline bool shell_static_register()
{
	static const SSZ_FunctionEntry shell_mapping[] =
	{
		{ "ShellOpen",  (void*)ShellOpen  },
		{ "MoveTrash",  (void*)MoveTrash  },
	};

	return SSZ_RegisterFunction(
		"shell",
		shell_mapping,
		sizeof(shell_mapping) / sizeof(shell_mapping[0]));
}

#else
inline bool shell_static_register() { return true; }
#endif
