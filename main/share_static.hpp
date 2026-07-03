#pragma once
//
// share_static.hpp
//
// Statically registers share plugin functions so that the SSZ runtime
// resolves them without loading share.dll.
//
// The share module provides &Share objects with copy()/push() methods
// that snapshot and restore game state across SSZ subsystems.
// When IKEMEN_NATIVE_SHARE_LIB=1, the native share_service implementation
// is registered with the SSZ runtime, allowing script-level copy/push
// operations to route to native C++ code instead of executing SSZ code.
//
// When IKEMEN_NATIVE_SHARE_LIB=0, the registration is a no-op stub and
// the SSZ share.ssz script is used as-is.

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_SHARE_LIB

struct PluginUtil;   // forward (from pluginutil.hpp)

extern "C"
{
    void SSZ_STDCALL ShareCopy(PluginUtil* pu);
    void SSZ_STDCALL SharePush(PluginUtil* pu);
}

/// Call once before the SSZ compiler starts.
/// Returns true on success.
inline bool share_static_register()
{
    static const SSZ_FunctionEntry share_mapping[] =
    {
        { "ShareCopy", (void*)ShareCopy },
        { "SharePush", (void*)SharePush },
    };

    return SSZ_RegisterFunction(
        "share",
        share_mapping,
        sizeof(share_mapping) / sizeof(share_mapping[0]));
}

#else
// Stub: native share service is not active, bridge registration not needed.
// share_static_register() still exists so main.cpp can call it unconditionally.
inline bool share_static_register() { return true; }
#endif // IKEMEN_NATIVE_SHARE_LIB
