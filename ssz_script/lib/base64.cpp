// ============================================================================
// base64.cpp — native (C++) implementation of the SSZ `base64` library's
// plain helpers and byte-level core.
//
// Consumed from ssz_script/lib/base64.ssz, which keeps the generic
// bit-packing templates in SSZ and delegates here:
//
//     lib b64 = <base64>;
//     ...
//     public char uintToB64Char(uint n) { ret .b64.uintToB64Char(n); }
//
//     // encBase64<_t> / decBase64<_t>: when the element type is byte-sized
//     // (typesize(_t) == 1), the template builds a %ubyte byte-view of the
//     // data and delegates to the native byte-level core; for wider element
//     // types it falls back to the original SSZ bit-packing, which treats
//     // each element as its own big-endian bit container (not the raw
//     // memory bytes).
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
// string/byte returns build a heap Reference and return its address.
//
// The byte-level core is RFC 4648 standard base64, which for byte/ubyte
// element types is bit-identical to the SSZ template (the SSZ bit-packer
// emits the same 6-bit groups and '=' padding).
// ============================================================================

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "sszdef.h"
#include "native_lib.hpp"
#include "arrayandref.hpp"
#include "pluginutil.hpp"

struct PluginUtil;

// ---------------------------------------------------------------------------
// Helpers: build heap References for string / byte-array returns
// ---------------------------------------------------------------------------

static intptr_t MakeStr(const std::WSTR& w)
{
	Reference* r = (Reference*)sszrefnewfunc(sizeof(Reference));
	if(r != nullptr){
		r->init();
		PluginUtil::wstrToRef(*r, w);
	}
	return (intptr_t)r;
}

static intptr_t MakeBytes(const std::vector<uint8_t>& bytes)
{
	Reference* r = (Reference*)sszrefnewfunc(sizeof(Reference));
	if(r != nullptr){
		r->init();
		r->refnew((intptr_t)bytes.size(), 1);
		if(!r->null() && !bytes.empty()){
			memcpy(r->atpos(), bytes.data(), bytes.size());
		}
	}
	return (intptr_t)r;
}

// ---------------------------------------------------------------------------
// Functions (plugin ABI — arguments arrive reversed vs. the SSZ declaration)
// ---------------------------------------------------------------------------

// SSZ: public char uintToB64Char(uint n)
// Maps 0..63 to the base64 alphabet; returns '=' for anything out of range.
static int16_t SSZ_STDCALL Base64LibUIntToB64Char(PluginUtil*, uint32_t n)
{
	static const WCHR table[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	if(n < 64) return (int16_t)table[n];
	return (int16_t)L'=';
}

// SSZ: public uint b64CharToUint(char c)
// Inverse of uintToB64Char; returns 64 for characters outside the alphabet.
static uint32_t SSZ_STDCALL Base64LibB64CharToUint(PluginUtil*, int16_t c)
{
	if(L'A' <= c && c <= L'Z') return (uint32_t)(c - L'A');
	if(L'a' <= c && c <= L'z') return (uint32_t)(c - L'a') + 26;
	if(L'0' <= c && c <= L'9') return (uint32_t)(c - L'0') + 52;
	if(c == L'+') return 62;
	if(c == L'/') return 63;
	return 64;
}

// SSZ: public ^char encB64(^/ubyte data) — standard base64 encode of a byte
// array (RFC 4648): 3 bytes -> 4 chars, '=' padding to a multiple of 4.
static intptr_t SSZ_STDCALL Base64LibEncB64(PluginUtil*, Reference data)
{
	const uint8_t* p = data.null() ? nullptr : (const uint8_t*)data.atpos();
	size_t n = data.null() ? 0 : (size_t)data.len();
	static const WCHR table[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	std::WSTR out;
	size_t i = 0;
	for(; i + 3 <= n; i += 3){
		uint32_t v = ((uint32_t)p[i] << 16) | ((uint32_t)p[i+1] << 8) | p[i+2];
		out += table[(v >> 18) & 0x3f];
		out += table[(v >> 12) & 0x3f];
		out += table[(v >> 6) & 0x3f];
		out += table[v & 0x3f];
	}
	size_t rem = n - i;
	if(rem == 1){
		uint32_t v = (uint32_t)p[i] << 16;
		out += table[(v >> 18) & 0x3f];
		out += table[(v >> 12) & 0x3f];
		out += L'=';
		out += L'=';
	}else if(rem == 2){
		uint32_t v = ((uint32_t)p[i] << 16) | ((uint32_t)p[i+1] << 8);
		out += table[(v >> 18) & 0x3f];
		out += table[(v >> 12) & 0x3f];
		out += table[(v >> 6) & 0x3f];
		out += L'=';
	}
	return MakeStr(out);
}

// SSZ: public ^ubyte decB64(^/char b64) — standard base64 decode to bytes.
// Stops at the first character outside the alphabet (including '='), exactly
// like the SSZ template (`if(tmp > 0d63) while;`).
static intptr_t SSZ_STDCALL Base64LibDecB64(PluginUtil*, Reference b64)
{
	std::WSTR s = PluginUtil::refToWstr(b64);
	std::vector<uint8_t> out;
	uint32_t bii = 0, x = 0;
	for(size_t i = 0; i < s.size(); i++){
		uint32_t tmp;
		WCHR c = s[i];
		if(L'A' <= c && c <= L'Z') tmp = (uint32_t)(c - L'A');
		else if(L'a' <= c && c <= L'z') tmp = (uint32_t)(c - L'a') + 26;
		else if(L'0' <= c && c <= L'9') tmp = (uint32_t)(c - L'0') + 52;
		else if(c == L'+') tmp = 62;
		else if(c == L'/') tmp = 63;
		else break;  // '=' or invalid — stop like the SSZ
		bii += 6;
		if(bii > 8){
			bii -= 8;
			x |= tmp >> bii;
			out.push_back((uint8_t)x);
			x = 0;
		}
		x |= tmp << (8 - bii);
	}
	return MakeBytes(out);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool base64_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "uintToB64Char", "char (uint)",     (void*)Base64LibUIntToB64Char },
		{ "b64CharToUint", "uint (char)",     (void*)Base64LibB64CharToUint },
		{ "encB64",        "^char (^/ubyte)", (void*)Base64LibEncB64        },
		{ "decB64",        "^ubyte (^/char)", (void*)Base64LibDecB64        },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "base64";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
