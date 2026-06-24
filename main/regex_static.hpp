#pragma once
//
// regex_static.hpp
//
// Statically register every function exported by regex.cpp
// so that the SSZ runtime resolves them without loading regex.dll.
//

#include "static_plugin_registry.hpp"

#ifdef _WIN32
#include <regex>
#define RNS std
#else
#include <boost/regex.hpp>
#define RNS boost
#endif

struct PluginUtil;
struct Reference;

extern "C"
{
	RNS::wregex* SSZ_STDCALL NewRegex(PluginUtil*, Reference*, bool, Reference);
	void         SSZ_STDCALL DeleteRegex(PluginUtil*, RNS::wregex*);
	void         SSZ_STDCALL RegexSearch(PluginUtil*, Reference*, Reference, RNS::wregex*);
}

inline bool regex_static_register()
{
	static const SSZ_FunctionEntry regex_mapping[] =
	{
		{ "NewRegex",    (void*)NewRegex    },
		{ "DeleteRegex", (void*)DeleteRegex },
		{ "RegexSearch", (void*)RegexSearch },
	};

	return SSZ_RegisterFunction(
		"regex",
		regex_mapping,
		sizeof(regex_mapping) / sizeof(regex_mapping[0]));
}
