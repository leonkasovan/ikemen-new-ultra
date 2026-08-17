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
// &Arcfour struct methods — the struct stays in SSZ (fields `uint x, y` and
// `^ubyte state`), the method bodies delegate here with the fields passed as
// out-params (pointers to the caller's slots, read-write).
// ---------------------------------------------------------------------------

// SSZ: &Arcfour.init(^/ubyte key) — KSA into the caller's state slot.
// Args arrive reversed: (pu, key, state*, y*, x*).
static void SSZ_STDCALL ArcfourLibInit(
	PluginUtil*, Reference key, Reference* state, uint32_t* y, uint32_t* x)
{
	*x = 0;
	*y = 0;
	if(state->null() || state->len() < 256) return;  // defensive: SSZ new() allocates 256
	uint8_t* st = (uint8_t*)state->atpos();
	for(int i = 0; i < 256; i++) st[i] = (uint8_t)i;

	const uint8_t* kb = (const uint8_t*)key.atpos();
	size_t kn = (size_t)key.len();
	if(kn == 0) return;
	// KSA: state accumulator j advances by key[counter % kn] + S[counter];
	// the SSZ tracks the key position separately (wraps at #key), which is
	// exactly counter % kn.
	size_t j = 0;
	for(int counter = 0; counter < 256; counter++){
		uint8_t t = st[counter];
		j = (j + kb[counter % kn] + t) & 0xff;
		uint8_t u = st[j];
		st[j] = t;
		st[counter] = u;
	}
}

// SSZ: &Arcfour.getByte() — one PRGA keystream byte, advancing x/y.
// Args arrive reversed: (pu, state*, y*, x*).
static uint8_t SSZ_STDCALL ArcfourLibGetByte(
	PluginUtil*, Reference* state, uint32_t* y, uint32_t* x)
{
	uint8_t* st = (uint8_t*)state->atpos();
	uint32_t nx = (*x + 1) & 0xff;
	uint8_t sx = st[nx];
	uint32_t ny = (sx + *y) & 0xff;
	uint8_t sy = st[ny];
	*x = nx;
	*y = ny;
	st[ny] = sx;
	st[nx] = sy;
	return st[(sx + sy) & 0xff];
}

// SSZ: &Arcfour.encrypt(^/ubyte src) — XOR each source byte with keystream.
// Args arrive reversed: (pu, src, state*, y*, x*).  Returns a heap ^ubyte.
static intptr_t SSZ_STDCALL ArcfourLibEncrypt(
	PluginUtil*, Reference src, Reference* state, uint32_t* y, uint32_t* x)
{
	const uint8_t* sp = src.null() ? nullptr : (const uint8_t*)src.atpos();
	size_t n = src.null() ? 0 : (size_t)src.len();
	std::vector<uint8_t> out(n);
	uint8_t* st = (uint8_t*)state->atpos();
	uint32_t xx = *x, yy = *y;
	for(size_t i = 0; i < n; i++){
		xx = (xx + 1) & 0xff;
		yy = (yy + st[xx]) & 0xff;
		uint8_t t = st[xx]; st[xx] = st[yy]; st[yy] = t;
		out[i] = sp[i] ^ st[(st[xx] + st[yy]) & 0xff];
	}
	*x = xx;
	*y = yy;

	Reference* r = (Reference*)sszrefnewfunc(sizeof(Reference));
	if(r != nullptr){
		r->init();
		r->refnew((intptr_t)out.size(), 1);
		if(!r->null() && !out.empty()){
			memcpy(r->atpos(), out.data(), out.size());
		}
	}
	return (intptr_t)r;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool arcfour_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "arcfourEnc", "bool (^ubyte=, ^/ubyte, ^/ubyte)", (void*)ArcfourLibEnc },
		{ "init",       "void (uint=, uint=, ^ubyte=, ^/ubyte)", (void*)ArcfourLibInit },
		{ "getByte",    "ubyte (uint=, uint=, ^ubyte=)", (void*)ArcfourLibGetByte },
		{ "encrypt",    "^ubyte (uint=, uint=, ^ubyte=, ^/ubyte)", (void*)ArcfourLibEncrypt },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "arcfour";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
