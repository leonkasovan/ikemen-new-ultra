#pragma once

// ssz_trace.hpp — Runtime trace mode for SSZ plugin entry points.
//
// When IKEMEN_ENABLE_PLUGIN_TRACE is defined (=1), every bridge wrapper
// logs the function name via printf before calling the native implementation.
//
// Categories allow filtering: set IKEMEN_TRACE_MASK to a bitmask of categories
// you want to trace. Build with:
//   make IKEMEN_ENABLE_PLUGIN_TRACE=1 IKEMEN_TRACE_MASK=64 CONFIG=Debug
//
// Category bit values:
//   1   (FILE)  - File I/O operations
//   2   (NET)   - Socket/network
//   4   (LUA)   - Lua scripting bridge
//   8   (OGG)   - OGG Vorbis audio
//   16  (UTIL)  - Regex, shell, thread, alert, clipboard, INI, compress
//   32  (MATH)  - Math and time functions
//   64  (SDL)   - SDL plugin (render, input, display, BGM)
//   128 (SYS)   - Game state (common, system, loader, char, fight, etc.)
//
// Usage:
//   SSZ_TRACE_CAT(TRACE_SDL, "Flip");

#include <cstdint>

#define TRACE_FILE  UINT32_C(1)
#define TRACE_NET   UINT32_C(2)
#define TRACE_LUA   UINT32_C(4)
#define TRACE_OGG   UINT32_C(8)
#define TRACE_UTIL  UINT32_C(16)
#define TRACE_MATH  UINT32_C(32)
#define TRACE_SDL   UINT32_C(64)
#define TRACE_SYS   UINT32_C(128)

#ifdef IKEMEN_ENABLE_PLUGIN_TRACE
#include <cstdio>
#ifndef IKEMEN_TRACE_MASK
#define IKEMEN_TRACE_MASK UINT32_MAX  // all categories by default
#endif
#define SSZ_TRACE_CAT(cat, msg) do { \
    if ((cat) & IKEMEN_TRACE_MASK) { \
        printf("[TRACE] %s\n", msg); \
        fflush(stdout); \
    } \
} while(0)
#else
#define SSZ_TRACE_CAT(cat, msg) ((void)0)
#endif
