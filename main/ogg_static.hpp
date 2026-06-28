#pragma once
//
// ogg_static.hpp
//
// Statically registers every function exported by ogg.cpp
// so that the SSZ runtime resolves them without loading ogg.dll.
//
// HOW TO USE
// ----------
// 1.  #include this header in the translation unit that owns main()
//     (or wherever the SSZ compiler is initialised).
//
// 2.  Call  ogg_static_register()  BEFORE the SSZ compiler runs.
//
//     #include "ogg_static.hpp"
//
//     int main() {
//         if (!ogg_static_register()) {
//             printf("Failed to register ogg functions.\n");
//             return 1;
//         }
//         // ... start SSZ compiler / runtime ...
//     }
//
// 3.  The SSZ scripts continue to use:
//         plugin void Close(:index:) = "dll/ogg.dll";
//     but NO ogg.dll file is needed — the function pointer is
//     resolved from the static registry.
//
// PREREQUISITES
// -------------
//   - Link ogg.cpp (and its dependencies) into the same executable.
//   - #include "static_plugin_registry.hpp" (pulled in automatically).
//

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
inline bool ogg_static_register() { return true; }
#endif
