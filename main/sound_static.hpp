#pragma once
//
// sound_static.hpp
//
// Statically register every function exported by sound.cpp
// so that the SSZ runtime resolves them without loading sound.dll.
//

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_SOUND_LIB

struct PluginUtil;
struct Reference;

// Client is defined inside sound.cpp; only used as an opaque pointer here.
class Client;

extern "C"
{
	Client* SSZ_STDCALL NewClient(PluginUtil*);
	void    SSZ_STDCALL DeleteClient(PluginUtil*, Client*);
	bool    SSZ_STDCALL ClientStart(PluginUtil*, Client*);
	bool    SSZ_STDCALL ClientStop(PluginUtil*, Client*);
	bool    SSZ_STDCALL ClientBufferReady(PluginUtil*, Client*);
	bool    SSZ_STDCALL ClientSetBuffer(PluginUtil*, Reference, Client*);
}

inline bool sound_static_register()
{
	static const SSZ_FunctionEntry sound_mapping[] =
	{
		{ "NewClient",         (void*)NewClient         },
		{ "DeleteClient",      (void*)DeleteClient      },
		{ "ClientStart",       (void*)ClientStart       },
		{ "ClientStop",        (void*)ClientStop        },
		{ "ClientBufferReady", (void*)ClientBufferReady  },
		{ "ClientSetBuffer",   (void*)ClientSetBuffer    },
	};

	return SSZ_RegisterFunction(
		"sound",
		sound_mapping,
		sizeof(sound_mapping) / sizeof(sound_mapping[0]));
}

#else
inline bool sound_static_register() { return true; }
#endif
