#pragma once
//
// system_static.hpp
//
// Statically registers system plugin functions so that the SSZ runtime
// resolves them without loading system.dll.
//
// The system module (ssz_script/ssz/system.ssz) implements character/stage
// select screen logic — &Select (character/stage list management, portrait
// loading), &SelectInfo (player selection state), and &System (top-level
// wrapper).  When IKEMEN_NATIVE_SYSTEM_LIB=1, the native system_service
// implementation is registered with the SSZ runtime, allowing script-level
// system operations to route to native C++ code instead of executing SSZ code.
//
// When IKEMEN_NATIVE_SYSTEM_LIB=0, the registration is a no-op stub and
// the SSZ system.ssz script is used as-is.

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_SYSTEM_LIB

struct PluginUtil;   // forward (from pluginutil.hpp)
struct Reference;    // forward (from arrayandref.hpp)

extern "C"
{
    bool   SSZ_STDCALL SystemAddChar(PluginUtil*, Reference def);
    void   SSZ_STDCALL SystemAddStage(PluginUtil*, Reference def, Reference* out);
    void   SSZ_STDCALL SystemGetStageName(PluginUtil*, int32_t i, Reference* out);
    int    SSZ_STDCALL SystemSetStageNo(PluginUtil*, int32_t i);
    void   SSZ_STDCALL SystemSelectStage(PluginUtil*, int32_t no);
    bool   SSZ_STDCALL SystemAddSelchr(PluginUtil*, int32_t pn, int32_t cn, int32_t pl);
    void   SSZ_STDCALL SystemSelReset(PluginUtil*);
}

/// Call once before the SSZ compiler starts.
/// Returns true on success.
inline bool system_static_register()
{
    static const SSZ_FunctionEntry system_mapping[] =
    {
        { "SystemAddChar",      (void*)SystemAddChar      },
        { "SystemAddStage",     (void*)SystemAddStage     },
        { "SystemGetStageName", (void*)SystemGetStageName },
        { "SystemSetStageNo",   (void*)SystemSetStageNo   },
        { "SystemSelectStage",  (void*)SystemSelectStage  },
        { "SystemAddSelchr",    (void*)SystemAddSelchr    },
        { "SystemSelReset",     (void*)SystemSelReset     },
    };

    return SSZ_RegisterFunction(
        "system",
        system_mapping,
        sizeof(system_mapping) / sizeof(system_mapping[0]));
}

#else
// Stub: native system service is not active, bridge registration not needed.
// system_static_register() still exists so main.cpp can call it unconditionally.
inline bool system_static_register() { return true; }
#endif // IKEMEN_NATIVE_SYSTEM_LIB
