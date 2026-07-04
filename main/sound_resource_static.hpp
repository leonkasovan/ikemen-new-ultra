#pragma once
//
// sound_resource_static.hpp
//
// Statically registers sound resource plugin functions so that the SSZ runtime
// resolves them without loading sound.dll.
//
// The sound module (ssz_script/ssz/sound.ssz) manages game sound effects and
// BGM resources: sound effect containers (Snd), per-channel playback state
// (Sound), BGM (Bgm), buffer management, and mixing.
//
// When IKEMEN_NATIVE_SOUND_RES_LIB=1, the native sound_resource_service
// implementation is registered with the SSZ runtime, allowing script-level
// sound operations to route to native C++ code instead of executing SSZ code.
//
// When IKEMEN_NATIVE_SOUND_RES_LIB=0, the registration is a no-op stub and
// the SSZ sound.ssz script is used as-is.

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_SOUND_RES_LIB

struct PluginUtil;   // forward (from pluginutil.hpp)
struct Reference;    // forward (from arrayandref.hpp)

extern "C"
{
	// init() — initializes sound resource system.
	void SSZ_STDCALL SoundResourceInit(PluginUtil* pu);

	// Module-level functions from sound.ssz
	void SSZ_STDCALL SoundSndbufClear(PluginUtil* pu);
	// getChannel(ch) — returns a channel reference (handled via SSZ objects)
	// void SSZ_STDCALL SoundGetChannel(PluginUtil* pu, int32_t ch);
	void SSZ_STDCALL SoundMixSounds(PluginUtil* pu);
	void SSZ_STDCALL SoundPlaySound(PluginUtil* pu);
	void SSZ_STDCALL SoundStopSound(PluginUtil* pu);
}

static inline bool sound_resource_static_register()
{
	SSZ_FunctionEntry entries[] = {
		{ "init",          (void*)&SoundResourceInit },
		{ "sndbufClear",   (void*)&SoundSndbufClear  },
		{ "mixSounds",     (void*)&SoundMixSounds    },
		{ "playSound",     (void*)&SoundPlaySound    },
		{ "stopSound",     (void*)&SoundStopSound    },
	};
	return SSZ_RegisterFunction("sound", entries, 5);
}

#else

static inline bool sound_resource_static_register()
{
	// IKEMEN_NATIVE_SOUND_RES_LIB=0 — SSZ sound.ssz used instead.
	return true;
}

#endif
