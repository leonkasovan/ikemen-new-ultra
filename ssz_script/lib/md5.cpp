// ============================================================================
// md5.cpp — native (C++) implementation of the SSZ `md5` library's module
// functions.
//
// Consumed from ssz_script/lib/md5.ssz, which keeps the stateful `&Md5`
// struct in SSZ and delegates the one-shot functions here:
//
//     lib md = <md5>;
//     ...
//     public ^ubyte md5(^/ubyte data) { ret .md.md5(data); }
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
// `^/ubyte` byte-array params arrive as `Reference` values; read the raw
// bytes from atpos()/len().  Array/string returns build a heap Reference
// and return its address (see the string library for the same pattern).
//
// The algorithm is RFC 1321 MD5, matching the SSZ implementation in md5.ssz
// (same T constants, padding, and little-endian digest output).
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
// Helpers: build heap References for byte-array / string returns
// ---------------------------------------------------------------------------

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

static intptr_t MakeStr(const std::WSTR& w)
{
	Reference* r = (Reference*)sszrefnewfunc(sizeof(Reference));
	if(r != nullptr){
		r->init();
		PluginUtil::wstrToRef(*r, w);
	}
	return (intptr_t)r;
}

// ---------------------------------------------------------------------------
// MD5 (RFC 1321)
// ---------------------------------------------------------------------------

namespace {

#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | ~(z)))
#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

// floor(2^32 * abs(sin(i))) for i = 1..64
static const uint32_t T[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
	0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
	0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
	0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
	0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
	0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

static const uint8_t R1[16] = { 7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22 };
static const uint8_t R2[16] = { 5, 9,14,20, 5, 9,14,20, 5, 9,14,20, 5, 9,14,20 };
static const uint8_t R3[16] = { 4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23 };
static const uint8_t R4[16] = { 6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21 };
static const uint8_t O1[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15 };
static const uint8_t O2[16] = { 1, 6,11, 0, 5,10,15, 4, 9,14, 3, 8,13, 2, 7,12 };
static const uint8_t O3[16] = { 5, 8,11,14, 1, 4, 7,10,13, 0, 3, 6, 9,12,15, 2 };
static const uint8_t O4[16] = { 0, 7,14, 5,12, 3,10, 1, 8,15, 6,13, 4,11, 2, 9 };

static void Md5Block(uint32_t abcd[4], const uint8_t* block)
{
	uint32_t a = abcd[0], b = abcd[1], c = abcd[2], d = abcd[3];
	uint32_t X[16];
	for(int i = 0; i < 16; i++){
		X[i] = (uint32_t)block[i*4]
			| ((uint32_t)block[i*4+1] << 8)
			| ((uint32_t)block[i*4+2] << 16)
			| ((uint32_t)block[i*4+3] << 24);
	}
	// round 1
	a = abcd[0]; b = abcd[1]; c = abcd[2]; d = abcd[3];
	for(int i = 0; i < 16; i++){
		uint32_t t = a + F(b, c, d) + X[O1[i]] + T[i];
		a = ROTL(t, R1[i]) + b;
		uint32_t u = d;
		d = c; c = b; b = a; a = u;
	}
	// round 2
	for(int i = 0; i < 16; i++){
		uint32_t t = a + G(b, c, d) + X[O2[i]] + T[16+i];
		a = ROTL(t, R2[i]) + b;
		uint32_t u = d;
		d = c; c = b; b = a; a = u;
	}
	// round 3
	for(int i = 0; i < 16; i++){
		uint32_t t = a + H(b, c, d) + X[O3[i]] + T[32+i];
		a = ROTL(t, R3[i]) + b;
		uint32_t u = d;
		d = c; c = b; b = a; a = u;
	}
	// round 4
	for(int i = 0; i < 16; i++){
		uint32_t t = a + I(b, c, d) + X[O4[i]] + T[48+i];
		a = ROTL(t, R4[i]) + b;
		uint32_t u = d;
		d = c; c = b; b = a; a = u;
	}
	abcd[0] += a; abcd[1] += b; abcd[2] += c; abcd[3] += d;
}

static std::vector<uint8_t> Md5Digest(const uint8_t* data, size_t n)
{
	uint32_t abcd[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
	uint64_t bitlen = (uint64_t)n * 8;

	std::vector<uint8_t> msg;
	if(n > 0) msg.assign(data, data + n);
	// padding: 0x80, zeros, then the 64-bit little-endian bit length
	msg.push_back(0x80);
	while((msg.size() % 64) != 56) msg.push_back(0);
	for(int i = 0; i < 8; i++) msg.push_back((uint8_t)(bitlen >> (8*i)));

	for(size_t i = 0; i < msg.size(); i += 64){
		Md5Block(abcd, msg.data() + i);
	}

	std::vector<uint8_t> digest(16);
	for(int i = 0; i < 16; i++){
		digest[i] = (uint8_t)(abcd[i >> 2] >> ((i & 3) << 3));
	}
	return digest;
}

#undef F
#undef G
#undef H
#undef I
#undef ROTL

}  // namespace

// ---------------------------------------------------------------------------
// Functions (plugin ABI — arguments arrive reversed vs. the SSZ declaration)
// ---------------------------------------------------------------------------

// SSZ: public ^ubyte md5(^/ubyte data)
static intptr_t SSZ_STDCALL Md5LibMd5(PluginUtil*, Reference data)
{
	// a null Reference is an empty byte array (SSZ's own empty-list form)
	const uint8_t* p = nullptr;
	size_t n = 0;
	if(!data.null()){
		p = (const uint8_t*)data.atpos();
		n = (size_t)data.len();
	}
	return MakeBytes(Md5Digest(p, n));
}

// SSZ: public ^char md5str(^/ubyte data)  — lowercase hex of the digest
// (matches string.ssz toHex!ubyte? used by the original md5str)
static intptr_t SSZ_STDCALL Md5LibMd5Str(PluginUtil*, Reference data)
{
	const uint8_t* p = nullptr;
	size_t n = 0;
	if(!data.null()){
		p = (const uint8_t*)data.atpos();
		n = (size_t)data.len();
	}
	std::vector<uint8_t> digest = Md5Digest(p, n);
	std::WSTR out;
	static const WCHR hexc[] = L"0123456789abcdef";
	for(size_t i = 0; i < digest.size(); i++){
		out += hexc[digest[i] >> 4];
		out += hexc[digest[i] & 0xf];
	}
	return MakeStr(out);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool md5_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "md5",    "^ubyte (^/ubyte)", (void*)Md5LibMd5    },
		{ "md5str", "^char (^/ubyte)",  (void*)Md5LibMd5Str },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "md5";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
