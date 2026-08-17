// ============================================================================
// arcfour.cpp — native (C++) implementation of the SSZ `arcfour` library's
// module function.
//
// Consumed from ssz_script/lib/arcfour.ssz, which keeps the stateful
// `&Arcfour` struct in SSZ and delegates the one-shot function here:
//
//     lib arc = <arcfour>;
//     ...
//     public bool arcfourEnc(^ubyte dest=, ^/ubyte key, ^/ubyte src)
//     {
//       ret .arc.arcfourEnc(dest=, key, src);
//     }
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
// `^/ubyte` byte-array params arrive as `Reference` values (null = empty);
// an out-parameter (`^ubyte dest=`) arrives as a pointer to the caller's
// Reference slot, which the function fills in place — the same convention as
// the plugin bridges in main/ssz/bridge.cpp (e.g. CompilerCompile).
//
// The algorithm is RC4 (arcfour), matching the SSZ implementation: 256-byte
// KSA from the key, then a byte-wise PRGA XOR against the source.
// ============================================================================

#include <cstdint>
#include <cstring>
#include <vector>

#include "sszdef.h"
#include "native_lib.hpp"
#include "arrayandref.hpp"

struct PluginUtil;

// SSZ: public bool arcfourEnc(^ubyte dest=, ^/ubyte key, ^/ubyte src)
// Args arrive reversed: (pu, src, key, dest*).
static bool SSZ_STDCALL ArcfourLibEnc(
	PluginUtil*, Reference src, Reference key, Reference* dest)
{
	// if(#key == 0) ret false;
	if(key.null() || key.len() == 0) return false;

	const uint8_t* kb = (const uint8_t*)key.atpos();
	size_t kn = (size_t)key.len();

	// Key-scheduling algorithm
	uint8_t S[256];
	for(int i = 0; i < 256; i++) S[i] = (uint8_t)i;
	size_t j = 0;
	for(int i = 0; i < 256; i++){
		j = (j + S[i] + kb[i % kn]) & 0xff;
		uint8_t t = S[i]; S[i] = S[j]; S[j] = t;
	}

	// Pseudo-random generation: dest[i] = src[i] ^ keystream[i]
	const uint8_t* sp = src.null() ? nullptr : (const uint8_t*)src.atpos();
	size_t n = src.null() ? 0 : (size_t)src.len();
	std::vector<uint8_t> out(n);
	size_t x = 0, y = 0;
	for(size_t i = 0; i < n; i++){
		x = (x + 1) & 0xff;
		y = (y + S[x]) & 0xff;
		uint8_t t = S[x]; S[x] = S[y]; S[y] = t;
		out[i] = sp[i] ^ S[(S[x] + S[y]) & 0xff];
	}

	// dest = out  (fill the caller's Reference slot)
	dest->releaseanddelete();
	dest->refnew((intptr_t)out.size(), 1);
	if(!dest->null() && !out.empty()){
		memcpy(dest->atpos(), out.data(), out.size());
	}
	return true;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool arcfour_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "arcfourEnc", "bool (^ubyte=, ^/ubyte, ^/ubyte)", (void*)ArcfourLibEnc },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "arcfour";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
