#pragma once
//
// ogg_static.hpp
//
// Statically registers every function exported by ogg.cpp
// so that the SSZ runtime resolves them without loading ogg.dll.

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_OGG_LIB

// -----------------------------------------------------------------------
// Forward-declare types needed in function signatures.
// (sszdef.h, typeid.h, and arrayandref.hpp are assumed to be
//  already included by the caller before this header is included.)
// -----------------------------------------------------------------------

struct PluginUtil;  // forward (from pluginutil.hpp)
struct Reference;   // forward (from arrayandref.hpp)
class  OggVorbis;   // forward (from ogg.cpp)

extern "C"
{
	OggVorbis* SSZ_STDCALL NewOggVorbis    (PluginUtil*);
	void       SSZ_STDCALL DeleteOggVorbis (PluginUtil*, OggVorbis*);
	bool       SSZ_STDCALL OggVorbisOpen   (PluginUtil*, Reference, OggVorbis*);
	void       SSZ_STDCALL OggVorbisClear  (PluginUtil*, OggVorbis*);
	int64_t    SSZ_STDCALL OggVorbisPcmTotal(PluginUtil*, OggVorbis*);
	int32_t    SSZ_STDCALL OggVorbisChannels(PluginUtil*, OggVorbis*);
	int32_t    SSZ_STDCALL OggVorbisRate   (PluginUtil*, OggVorbis*);
	intptr_t   SSZ_STDCALL OggVorbisRead   (PluginUtil*, Reference, OggVorbis*);
	int32_t    SSZ_STDCALL OggVorbisSeek   (PluginUtil*, double, OggVorbis*);
}

// -----------------------------------------------------------------------
// Build the mapping table and register it.
// -----------------------------------------------------------------------

/// Call once before the SSZ compiler starts.
/// Returns true on success.
inline bool ogg_static_register()
{
	static const SSZ_FunctionEntry ogg_mapping[] =
	{
		{ "NewOggVorbis",     (void*)NewOggVorbis     },
		{ "DeleteOggVorbis",  (void*)DeleteOggVorbis  },
		{ "OggVorbisOpen",    (void*)OggVorbisOpen    },
		{ "OggVorbisClear",   (void*)OggVorbisClear   },
		{ "OggVorbisPcmTotal",(void*)OggVorbisPcmTotal},
		{ "OggVorbisChannels",(void*)OggVorbisChannels},
		{ "OggVorbisRate",    (void*)OggVorbisRate    },
		{ "OggVorbisRead",    (void*)OggVorbisRead    },
		{ "OggVorbisSeek",    (void*)OggVorbisSeek    },
	};

	return SSZ_RegisterFunction(
		"ogg",
		ogg_mapping,
		sizeof(ogg_mapping) / sizeof(ogg_mapping[0]));
}

#else
static inline bool ogg_static_register()
{
	// IKEMEN_NATIVE_OGG_LIB=0 — SSZ ogg.ssz script used instead.
	return true;
}
#endif

