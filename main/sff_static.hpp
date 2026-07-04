// sff_static.hpp — Static plugin registration for ssz_script/ssz/sff.ssz
//
// When IKEMEN_NATIVE_SFF_LIB=1, replaces the SSZ sff.ssz module
// with native C++ entry points registered in the static plugin table.

#pragma once
#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_SFF_LIB

#include <cstdint>
struct PluginUtil;   // forward (from pluginutil.hpp)
struct Reference;    // forward (from arrayandref.hpp)

extern "C" {

// init() — initializes SFF sprite/animation system.
void SSZ_STDCALL SffInit(PluginUtil* pu);

// loadFile(filename, chr) — load an SFF file. Returns error string.
void SSZ_STDCALL SffLoadFile(PluginUtil* pu, Reference filename, int32_t chr, Reference* out);

// getSprite(group, number) — get a sprite by group/number. Returns sprite ref.
void SSZ_STDCALL SffGetSprite(PluginUtil* pu, int32_t group, int32_t number, Reference* out);

} // extern "C"

static inline bool sff_static_register()
{
	SSZ_FunctionEntry entries[] = {
		{ "init",      (void*)&SffInit      },
		{ "loadFile",  (void*)&SffLoadFile  },
		{ "getSprite", (void*)&SffGetSprite },
	};
	return SSZ_RegisterFunction("sff", entries, 3);
}

#else

static inline bool sff_static_register()
{
	// IKEMEN_NATIVE_SFF_LIB=0 — SSZ sff.ssz used instead.
	return true;
}

#endif
