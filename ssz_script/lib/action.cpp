// ============================================================================
// action.cpp — native C++ library for action.ssz.
//
// Provides &.Rect utility functions called from the SSZ wrapper.
// Exercises &.Rect in multiple parameter positions:
//   - &.Rect=  (out-param, struct pointer)  — fillRect
//   - &.Rect   (input param, struct pointer) — rectOverlap, rectMerge
// ============================================================================

#include "sszdef.h"
#include "arrayandref.hpp"
#include "native_lib.hpp"
#include "pluginutil.hpp"

// ---------------------------------------------------------------------------
// ActionRect — matches &Rect layout in action.ssz: 4 x int32_t = 16 bytes.
// ---------------------------------------------------------------------------

struct ActionRect { int32_t l, t, r, b; };

// ---------------------------------------------------------------------------
// fillRect — fills a &.Rect with default values.
// Signature: void (&.Rect=)
// ---------------------------------------------------------------------------

static void SSZ_STDCALL fillRect(PluginUtil* pu, ActionRect* rect)
{
    if(rect) {
        rect->l = 0;
        rect->t = 0;
        rect->r = 320;
        rect->b = 240;
    }
}

// ---------------------------------------------------------------------------
// rectOverlap — AABB overlap test between two &.Rect.
// Signature: bool (&.Rect, &.Rect)
// ---------------------------------------------------------------------------

static bool SSZ_STDCALL rectOverlap(PluginUtil* pu, ActionRect* a, ActionRect* b)
{
    if(!a || !b) return false;
    return a->l < b->r && b->l < a->r
        && a->t < b->b && b->t < a->b;
}

// ---------------------------------------------------------------------------
// rectMerge — merge (union) two &.Rect into the first.
// Signature: void (&.Rect=, &.Rect)
// ---------------------------------------------------------------------------

static void SSZ_STDCALL rectMerge(PluginUtil* pu, ActionRect* out, ActionRect* b)
{
    if(!out || !b) return;
    if(out->l > b->l) out->l = b->l;
    if(out->t > b->t) out->t = b->t;
    if(out->r < b->r) out->r = b->r;
    if(out->b < b->b) out->b = b->b;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool action_lib_register()
{
    NativeLib::NativeFunction funcs[] = {
        { "fillRect",    "void (&.Rect=)",              (void*)fillRect    },
        { "rectOverlap", "bool (&.Rect, &.Rect)",       (void*)rectOverlap },
        { "rectMerge",   "void (&.Rect=, &.Rect)",      (void*)rectMerge   },
    };
    NativeLib::NativeLibrary lib;
    lib.name = "action";
    for(auto& f : funcs) lib.functions.push_back(f);
    return NativeLib::RegisterLibrary(lib);
}
