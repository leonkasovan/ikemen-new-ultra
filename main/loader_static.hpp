#pragma once
//
// loader_static.hpp
//
// Statically registers loader plugin functions so that the SSZ runtime
// resolves them without loading loader.dll.
//
// The loader module (ssz_script/ssz/loader.ssz) implements the loading
// screen logic — character/stage loading, state compilation, and threading.
// When IKEMEN_NATIVE_LOADER_LIB=1, the native loader_service implementation
// is registered with the SSZ runtime, allowing script-level loader
// operations to route to native C++ code instead of executing SSZ code.
//
// When IKEMEN_NATIVE_LOADER_LIB=0, the registration is a no-op stub and
// the SSZ loader.ssz script is used as-is.

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_LOADER_LIB

struct PluginUtil;   // forward (from pluginutil.hpp)
struct Reference;    // forward (from arrayandref.hpp)

extern "C"
{
    void SSZ_STDCALL LoaderError(PluginUtil*, Reference msg);
    bool SSZ_STDCALL LoaderStage(PluginUtil*);
    int  SSZ_STDCALL LoaderChara(PluginUtil*, int32_t pn);
    bool SSZ_STDCALL LoaderStateCompile(PluginUtil*);
    void SSZ_STDCALL LoaderLoad(PluginUtil*);
    void SSZ_STDCALL LoaderReset(PluginUtil*);
    bool SSZ_STDCALL LoaderRunTread(PluginUtil*);
}

/// Call once before the SSZ compiler starts.
/// Returns true on success.
inline bool loader_static_register()
{
    static const SSZ_FunctionEntry loader_mapping[] =
    {
        { "LoaderError",        (void*)LoaderError        },
        { "LoaderStage",        (void*)LoaderStage        },
        { "LoaderChara",        (void*)LoaderChara        },
        { "LoaderStateCompile", (void*)LoaderStateCompile },
        { "LoaderLoad",         (void*)LoaderLoad         },
        { "LoaderReset",        (void*)LoaderReset        },
        { "LoaderRunTread",     (void*)LoaderRunTread     },
    };

    return SSZ_RegisterFunction(
        "loader",
        loader_mapping,
        sizeof(loader_mapping) / sizeof(loader_mapping[0]));
}

#else
// Stub: native loader service is not active, bridge registration not needed.
// loader_static_register() still exists so main.cpp can call it unconditionally.
inline bool loader_static_register() { return true; }
#endif // IKEMEN_NATIVE_LOADER_LIB
