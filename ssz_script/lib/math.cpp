// ============================================================================
// math.cpp — native (C++) implementation of the SSZ `math` PRNG core.
//
// Consumed from ssz_script/lib/math.ssz, which keeps the template functions
// (min/max/inRange/limMax/limMin/limRange/swap) and the trig plugin wrappers
// in SSZ and delegates its PRNG core here:
//
//     lib mn = <math>;
//     ...
//     public int random() { ret .mn.random(); }
//
// The module is resolved by the native-lib registry (main/ssz/native_lib.hpp).
// Every function below follows the plugin ABI:
//
//     T name(PluginUtil*, ...)   (first arg is the SSZ runtime util; the JIT
//                                 passes g_gpsf.sf and reads the result from
//                                 the return slot)
//
// ABI note: the JIT pushes arguments in SSZ declaration order, so a C++
// function receives them reversed — the last SSZ parameter arrives first.
//
// The PRNG is the Park–Miller minimal standard generator, ported verbatim
// from the original math.ssz (same constants and update rule, seed derived
// from the current time).  `randseed` is also registered as a module variable
// for interface parity with the original module.
// ============================================================================

#include <cstdint>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#else
#include <time.h>
#endif

#include "sszdef.h"
#include "native_lib.hpp"

struct PluginUtil;

// RANDMAX = .consts.int_t::MAX — the maximum Park–Miller seed / result.
static const int64_t RANDMAX = (int64_t)0x7FFFFFFF;

// Module state (the original math.ssz's `int randseed`).
static int64_t s_randseed = 0;

static void SeedFromTime()
{
	// Mirror math.ssz: (time.unixTime() ^ (long)time.tickCount() << 16) & RANDMAX
	int64_t t = (int64_t)time(nullptr);
#ifdef _WIN32
	int64_t ms = (int64_t)timeGetTime();
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	int64_t ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
	s_randseed = (t ^ (ms << 16)) & RANDMAX;
}

// ---------------------------------------------------------------------------
// Functions (plugin ABI — arguments arrive reversed vs. the SSZ declaration)
// ---------------------------------------------------------------------------

// SSZ: public int random()
static int64_t SSZ_STDCALL MathLibRandom(PluginUtil*)
{
	int64_t w = s_randseed / 127773;
	s_randseed = (s_randseed - w * 127773) * 16807 - w * 2836;
	if(s_randseed <= 0){
		s_randseed += RANDMAX - (int64_t)(s_randseed == 0);
	}
	return s_randseed;
}

// SSZ: public void srand(int s)
// (SSZ ints arrive as 32-bit values in 8-byte slots — read only the low 32
// bits, exactly like the plugin bridges in main/ssz/bridge.cpp.)
static void SSZ_STDCALL MathLibSrand(PluginUtil*, int32_t s)
{
	s_randseed = s;
}

// SSZ: public int rand(int min, int max)
static int64_t SSZ_STDCALL MathLibRand(PluginUtil*, int32_t max, int32_t min)
{
	return (int64_t)min + MathLibRandom(nullptr)
		/ ((int64_t)RANDMAX / ((int64_t)max - min + 1) + 1);
}

// SSZ: public int randI(int x, int y)
static int64_t SSZ_STDCALL MathLibRandI(PluginUtil* pu, int32_t y, int32_t x)
{
	if(y < x){
		if((int64_t)x - y < 0) return (int64_t)y + MathLibRandom(pu) * ((int64_t)x - y) / RANDMAX;
		return MathLibRand(pu, x, y);   // rand(y, x)
	}
	if((int64_t)y - x < 0) return (int64_t)x + MathLibRandom(pu) * ((int64_t)y - x) / RANDMAX;
	return MathLibRand(pu, y, x);       // rand(x, y)
}

// SSZ: public float randF(float x, float y)
static float SSZ_STDCALL MathLibRandF(PluginUtil*, float y, float x)
{
	return x + (float)MathLibRandom(nullptr) * (y - x) / (float)RANDMAX;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool math_lib_register()
{
	SeedFromTime();

	static const NativeLib::NativeFunction funcs[] = {
		{ "random", "int ()",             (void*)MathLibRandom },
		{ "srand",  "void (int)",         (void*)MathLibSrand  },
		{ "rand",   "int (int, int)",     (void*)MathLibRand   },
		{ "randI",  "int (int, int)",     (void*)MathLibRandI  },
		{ "randF",  "float (float, float)", (void*)MathLibRandF },
	};
	static const NativeLib::NativeVariable vars[] = {
		// Private, like the original math.ssz declaration.  The PRNG state
		// itself lives in s_randseed above; the frame slot is interface parity.
		{ "randseed", "int" },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "math";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	for(size_t i = 0; i < sizeof(vars) / sizeof(vars[0]); i++){
		lib.variables.push_back(vars[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
