#pragma once
//
// debug_script_static.hpp
//
// Statically registers debug script plugin functions so that the SSZ runtime
// resolves them without loading debug_script.dll.
//
// The debug-script module (ssz_script/ssz/debug-script.ssz) implements the
// developer debug Lua API — functions like setLife/setPower/togglePause
// that are registered as Lua callbacks for in-game debugging.
// When IKEMEN_NATIVE_DEBUG_SCRIPT_LIB=1, the native debug_script_service
// implementation is registered with the SSZ runtime, allowing script-level
// debug operations to route to native C++ code.
//
// When IKEMEN_NATIVE_DEBUG_SCRIPT_LIB=0, the registration is a no-op stub
// and the SSZ debug-script.ssz script is used as-is.

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_DEBUG_SCRIPT_LIB

struct PluginUtil;   // forward (from pluginutil.hpp)
struct Reference;    // forward (from arrayandref.hpp)

extern "C"
{
    // loadFile and runFile are the main entry points — they initialize the
    // Lua state, register all debug Lua callbacks, and execute the script.
    // Returns error string via Reference* out (empty = success).
    void SSZ_STDCALL DebugLoadFile(PluginUtil*, Reference file, Reference* out);
    void SSZ_STDCALL DebugRunFile(PluginUtil*, Reference file, Reference* out);
}

/// Call once before the SSZ compiler starts.
/// Returns true on success.
inline bool debug_script_static_register()
{
    static const SSZ_FunctionEntry debug_script_mapping[] =
    {
        { "DebugLoadFile", (void*)DebugLoadFile },
        { "DebugRunFile",  (void*)DebugRunFile  },
    };

    return SSZ_RegisterFunction(
        "debug_script",
        debug_script_mapping,
        sizeof(debug_script_mapping) / sizeof(debug_script_mapping[0]));
}

#else
// Stub: native debug_script service is not active, bridge registration not needed.
inline bool debug_script_static_register() { return true; }
#endif // IKEMEN_NATIVE_DEBUG_SCRIPT_LIB
