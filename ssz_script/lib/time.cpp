// ============================================================================
// time.cpp — native (C++) implementation of the SSZ `time` library.
//
// Converted from ssz_script/lib/time.ssz.  Declared in SSZ scripts as:
//
//     lib time = <time>;
//
// The module is resolved by the native-lib registry (main/ssz/native_lib.hpp)
// — no time.ssz file is required.  Every public function from time.ssz is
// implemented below with the plugin ABI:
//
//     T name(PluginUtil*, ...)   (first arg is the SSZ runtime util; the JIT
//                                 passes g_gpsf.sf and reads the result from
//                                 the return slot)
//
// The signature strings describe the SSZ view of each function and are used
// by the compiler to type-check calls like `time.tickCount()`.
//
// Note: time.ssz also exposes div/mod/days/ymdhms/ymdhmsToUnixTime.  The
// first three are pure integer helpers and can be ported trivially; ymdhms
// returns a `^int` array that must be allocated through the SSZ Reference
// runtime, which is deferred.  math.ssz only uses tickCount() and unixTime().
// ============================================================================

#include <ctime>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#else
#include <time.h>
#endif

#include "sszdef.h"

// ---------------------------------------------------------------------------
// Native library registry
// ---------------------------------------------------------------------------
#include "native_lib.hpp"

struct PluginUtil;

// ---------------------------------------------------------------------------
// Functions (plugin ABI — first argument is the SSZ runtime PluginUtil*)
// ---------------------------------------------------------------------------

static uint32_t SSZ_STDCALL TickCount(PluginUtil*)
{
#ifdef _WIN32
	return timeGetTime();
#else
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (uint32_t)(now.tv_sec * 1000 + now.tv_nsec / 1000000);
#endif
}

static int64_t SSZ_STDCALL UnixTime(PluginUtil*)
{
	return (int64_t)time(nullptr);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool time_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "tickCount", "uint ()", (void*)TickCount },
		{ "unixTime",  "long ()", (void*)UnixTime  },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "time";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
