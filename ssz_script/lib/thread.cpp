// ============================================================================
// thread.cpp — native (C++) implementation of the SSZ `thread` library.
//
// Converted from ssz_script/lib/thread.ssz.  Declared in SSZ scripts as:
//
//     lib th = <thread>;
//
// The module is resolved by the native-lib registry (main/ssz/native_lib.hpp)
// — no thread.ssz file is required.  Every public function from thread.ssz is
// implemented below with the plugin ABI:
//
//     T name(PluginUtil*, ...)   (first arg is the SSZ runtime util; the JIT
//                                 passes g_gpsf.sf and reads the result from
//                                 the return slot)
//
// ABI note: the JIT pushes arguments in SSZ declaration order, so a C++
// function receives them reversed — the last SSZ parameter arrives first.
// This mirrors the plugin bridge in main/ssz/bridge.cpp (ThreadDelay).
// ============================================================================

#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#else
#include <thread>
#include <chrono>
#endif

#include "sszdef.h"
#include "native_lib.hpp"

struct PluginUtil;

// ---------------------------------------------------------------------------
// Functions (plugin ABI — arguments arrive reversed vs. the SSZ declaration)
// ---------------------------------------------------------------------------

// SSZ: public void sleep(uint milliseconds)
static void SSZ_STDCALL ThreadLibSleep(PluginUtil*, uint32_t ms)
{
#ifdef _WIN32
	Sleep(ms);
#else
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool thread_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "sleep", "void (uint)", (void*)ThreadLibSleep },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "thread";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
