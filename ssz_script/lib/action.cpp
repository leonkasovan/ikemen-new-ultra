// ============================================================================
// action.cpp — native C++ library for action.ssz.
//
// Proves that &.Rect type resolution works from child modules.  The key
// test: action.ssz defines &Rect and imports this native lib, which uses
// &.Rect in its signature strings.  NativeTypeID must resolve .Rect from
// the importing module's scope (not just root).
// ============================================================================

#include "sszdef.h"
#include "arrayandref.hpp"
#include "native_lib.hpp"
#include "pluginutil.hpp"

// ---------------------------------------------------------------------------
// fillRect — takes a &.Rect out-param (pointer to struct memory) and fills
// it with default values.  Matches the ABI of sdlplugin's Fill: the JIT
// passes a pointer to the caller's struct slot.
// ---------------------------------------------------------------------------

struct ActionRect { int32_t l, t, r, b; };

static void SSZ_STDCALL fillRect(PluginUtil* pu, ActionRect* rect)
{
    if(rect) {
        rect->l = 0;
        rect->t = 0;
        rect->r = 320;
        rect->b = 240;
    }
}

extern "C" bool action_lib_register()
{
    NativeLib::NativeFunction funcs[] = {
        // &.Rect= is encoded as AND_TOKEN + class_id of Rect, plus
        // ~DAINYUU_TOKEN.  The JIT passes a pointer to the struct memory.
        { "fillRect", "void (&.Rect=)", (void*)fillRect },
    };
    NativeLib::NativeLibrary lib;
    lib.name = "action";
    for(auto& f : funcs) lib.functions.push_back(f);
    return NativeLib::RegisterLibrary(lib);
}
