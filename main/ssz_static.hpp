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
	void           SSZ_STDCALL ProcMemMark(PluginUtil*, Reference);
	void           SSZ_STDCALL SetProfilerLog(PluginUtil*, bool);
	bool           SSZ_STDCALL SffV2CacheGet(PluginUtil*, Reference*, int32_t, int32_t, Reference);
	void           SSZ_STDCALL SffV2CachePut(PluginUtil*, Reference, int32_t, int32_t, Reference);
	bool           SSZ_STDCALL SndCacheGet(PluginUtil*, Reference*, int32_t, int32_t, Reference);
	void           SSZ_STDCALL SndCachePut(PluginUtil*, Reference, int32_t, int32_t, Reference);
	void           SSZ_STDCALL ProfBegin(PluginUtil*, Reference);
	void           SSZ_STDCALL ProfEnd(PluginUtil*, Reference);
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
		{ "ProcMemMark",         (void*)ProcMemMark         },
		{ "SetProfilerLog",      (void*)SetProfilerLog      },
		{ "SffV2CacheGet",       (void*)SffV2CacheGet       },
		{ "SffV2CachePut",       (void*)SffV2CachePut       },
		{ "SndCacheGet",         (void*)SndCacheGet         },
		{ "SndCachePut",         (void*)SndCachePut         },
		{ "ProfBegin",           (void*)ProfBegin           },
		{ "ProfEnd",             (void*)ProfEnd             },
	};

	return SSZ_RegisterFunction(
		"ssz",
		ssz_mapping,
		sizeof(ssz_mapping) / sizeof(ssz_mapping[0]));
}