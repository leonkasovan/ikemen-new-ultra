#pragma once
//
// sdlevent_static.hpp
//
// Statically registers sdlevent plugin functions so that the SSZ runtime
// resolves them without loading sdlevent.dll.
//
// The sdlevent module (ssz_script/lib/alpha/sdlevent.ssz) provides SDL
// event polling, key state tracking, and frame timing. When
// IKEMEN_NATIVE_SDLEVENT_LIB=1, the native sdlevent_service implementation
// is registered with the SSZ runtime.
//
// When IKEMEN_NATIVE_SDLEVENT_LIB=0, the registration is a no-op stub and
// the SSZ sdlevent.ssz script is used as-is.

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_SDLEVENT_LIB

struct PluginUtil;   // forward (from pluginutil.hpp)

extern "C"
{
    bool SSZ_STDCALL SdleventEventUpdate(PluginUtil*);
    bool SSZ_STDCALL SdleventEvent(PluginUtil*, int32_t fps);
}

/// Call once before the SSZ compiler starts.
/// Returns true on success.
inline bool sdlevent_static_register()
{
    static const SSZ_FunctionEntry sdlevent_mapping[] =
    {
        { "eventUpdate", (void*)SdleventEventUpdate },
        { "event",       (void*)SdleventEvent       },
    };

    return SSZ_RegisterFunction(
        "sdlevent",
        sdlevent_mapping,
        sizeof(sdlevent_mapping) / sizeof(sdlevent_mapping[0]));
}

#else

inline bool sdlevent_static_register() { return true; }

#endif
