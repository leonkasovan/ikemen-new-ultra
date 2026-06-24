#pragma once
//
// ssz_static.hpp
//
// Statically register every function exported by ssz.cpp
// so that the SSZ runtime resolves them without loading ssz.dll.

#include "static_plugin_registry.hpp"

struct PluginUtil;
struct Reference;
class CompilerState;	extern "C"
{
	bool           SSZ_STDCALL Run(PluginUtil*, Reference);
	CompilerState* SSZ_STDCALL NewCompiler(PluginUtil*);
	void           SSZ_STDCALL DeleteCompiler(PluginUtil*, CompilerState*);
	void           SSZ_STDCALL CompilerCompile(PluginUtil*, Reference*, Reference, CompilerState*);
	void           SSZ_STDCALL CompilerCompileString(PluginUtil*, Reference*, Reference, Reference, CompilerState*);
	bool           SSZ_STDCALL CompilerRun(PluginUtil*, CompilerState*);
	void           SSZ_STDCALL MemMarkBefore(PluginUtil*, Reference);
	void           SSZ_STDCALL MemMarkAfter(PluginUtil*, Reference);
}

inline bool ssz_static_register()
{
	static const SSZ_FunctionEntry ssz_mapping[] =
	{
		{ "Run",                 (void*)Run                 },
		{ "NewCompiler",         (void*)NewCompiler         },
		{ "DeleteCompiler",      (void*)DeleteCompiler      },
		{ "CompilerCompile",     (void*)CompilerCompile     },
		{ "CompilerCompileString", (void*)CompilerCompileString },
		{ "CompilerRun",         (void*)CompilerRun         },
		{ "MemMarkBefore",       (void*)MemMarkBefore       },
		{ "MemMarkAfter",        (void*)MemMarkAfter        },
	};

	return SSZ_RegisterFunction(
		"ssz",
		ssz_mapping,
		sizeof(ssz_mapping) / sizeof(ssz_mapping[0]));
}