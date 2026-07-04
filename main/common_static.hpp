#pragma once
//
// common_static.hpp
//
// Statically registers common plugin functions so that the SSZ runtime
// resolves them without loading common.dll.
//
// The common module (ssz_script/ssz/common.ssz) provides game-wide state
// (life, power, timer, score, camera, display flags) and utility functions
// (tickFrame, atoi, atof, loadText, etc.). When IKEMEN_NATIVE_COMMON_LIB=1,
// the native common_service implementation is registered with the SSZ
// runtime, allowing script-level common operations to route to native C++
// code instead of executing SSZ common.ssz code.
//
// When IKEMEN_NATIVE_COMMON_LIB=0, the registration is a no-op stub and
// the SSZ common.ssz script is used as-is.

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_COMMON_LIB

struct PluginUtil;   // forward (from pluginutil.hpp)
struct Reference;    // forward (from arrayandref.hpp)

extern "C"
{
    void   SSZ_STDCALL CommonFlagInit(PluginUtil*);
    void   SSZ_STDCALL CommonResetRemapInput(PluginUtil*);
    void   SSZ_STDCALL CommonSetSize(PluginUtil*, int32_t w, int32_t h);
    bool   SSZ_STDCALL CommonTickFrame(PluginUtil*);
    bool   SSZ_STDCALL CommonTickNextFrame(PluginUtil*);
    float  SSZ_STDCALL CommonTickInterpola(PluginUtil*);
    bool   SSZ_STDCALL CommonAddFrameTime(PluginUtil*, float t);
    void   SSZ_STDCALL CommonResetFrameTime(PluginUtil*);
    bool   SSZ_STDCALL CommonMatchOver(PluginUtil*);
    int    SSZ_STDCALL CommonAtoi(PluginUtil*, Reference str);
    double SSZ_STDCALL CommonAtof(PluginUtil*, Reference str);
    void   SSZ_STDCALL CommonLoadText(PluginUtil*, Reference* out, Reference filename, bool unicode);
}

/// Call once before the SSZ compiler starts.
/// Returns true on success.
inline bool common_static_register()
{
    static const SSZ_FunctionEntry common_mapping[] =
    {
        { "CommonFlagInit",      (void*)CommonFlagInit      },
        { "CommonResetRemapInput", (void*)CommonResetRemapInput },
        { "CommonSetSize",       (void*)CommonSetSize       },
        { "CommonTickFrame",     (void*)CommonTickFrame     },
        { "CommonTickNextFrame", (void*)CommonTickNextFrame },
        { "CommonTickInterpola", (void*)CommonTickInterpola },
        { "CommonAddFrameTime",  (void*)CommonAddFrameTime  },
        { "CommonResetFrameTime",(void*)CommonResetFrameTime},
        { "CommonMatchOver",     (void*)CommonMatchOver     },
        { "CommonAtoi",          (void*)CommonAtoi          },
        { "CommonAtof",          (void*)CommonAtof          },
        { "CommonLoadText",      (void*)CommonLoadText      },
    };

    return SSZ_RegisterFunction(
        "common",
        common_mapping,
        sizeof(common_mapping) / sizeof(common_mapping[0]));
}

#else
// Stub: native common service is not active, bridge registration not needed.
// common_static_register() still exists so main.cpp can call it unconditionally.
inline bool common_static_register() { return true; }
#endif // IKEMEN_NATIVE_COMMON_LIB
