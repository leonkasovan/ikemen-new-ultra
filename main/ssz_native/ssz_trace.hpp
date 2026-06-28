#pragma once

// ssz_trace.hpp — Runtime trace mode for SSZ plugin entry points.
//
// When IKEMEN_ENABLE_PLUGIN_TRACE is defined (=1), every bridge wrapper
// logs the function name and argument summary via LOG_DEBUG before calling
// the native implementation.
//
// Usage in a bridge wrapper:
//   SSZ_TRACE("Open(md=..., fn=...)");
//   return Open(converted_args);
//
// The trace output goes to stdout prefixed with [TRACE].

#ifdef IKEMEN_ENABLE_PLUGIN_TRACE
#include <cstdio>
#define SSZ_TRACE(msg) do { printf("[TRACE] %s\n", msg); fflush(stdout); } while(0)
#else
#define SSZ_TRACE(msg) ((void)0)
#endif
