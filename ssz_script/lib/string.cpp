// ============================================================================
// string.cpp — native (C++) implementation of the SSZ `string` library's
// plain functions.
//
// Consumed from ssz_script/lib/string.ssz, which keeps the template
// functions (uToSxX/sToNumber/sToN/svToAry/cMatch/copy/clone/each/toHex/
// toUbyte), the list-returning functions (split/join/splitLines), the
// private char helpers (hex/heX/toLowerChar), and the &Format struct in
// SSZ and delegates the rest here:
//
//     lib sn = <string>;
//     ...
//     public bool equ(^/char str1, ^/char str2) { ret .sn.equ(str1, str2); }
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
// `^/char` string params arrive as `Reference` values; convert with the
// static PluginUtil::refToWstr.  Functions that return a string/byte array
// build a heap Reference (sszrefnewfunc + init) and return its address —
// the JIT unpacks the Reference fields into the temp-ref registers.
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

// SSZ: public ^char uToSo(ulong uinte)  — unsigned octal representation.
// Ported verbatim from the original: find the top 3-bit group, then emit
// '0'..'7' digits from the top down.
static intptr_t SSZ_STDCALL StrLibUToSo(PluginUtil*, uint64_t uinte)
{
	uint32_t shift = 63;
	while(shift != 0 && (uinte >> shift) == 0) shift -= 3;
	std::WSTR buf;
	buf += (WCHR)(L'0' + (uinte >> shift & 0x7));
	while(shift != 0){
		shift -= 3;
		buf += (WCHR)(L'0' + (uinte >> shift & 0x7));
	}
	return MakeStr(buf);
}

// SSZ: public bool equ(^/char str1, ^/char str2)
static bool SSZ_STDCALL StrLibEqu(PluginUtil*, Reference str2, Reference str1)
{
	return PluginUtil::refToWstr(str1) == PluginUtil::refToWstr(str2);
}

// SSZ: public ^char toLower(^/char str)  — ASCII A-Z -> a-z, rest unchanged
static intptr_t SSZ_STDCALL StrLibToLower(PluginUtil*, Reference str)
{
	std::WSTR s = PluginUtil::refToWstr(str);
	for(size_t i = 0; i < s.size(); i++){
		WCHR c = s[i];
		if(L'A' <= c && c <= L'Z') s[i] = (WCHR)(c + (L'a' - L'A'));
	}
	return MakeStr(s);
}

// SSZ: public int nextLine(index i=, ^/char str)
// Returns 1 for '\n', 2 for "\r\n", 0 when no newline is found before the end.
static int64_t SSZ_STDCALL StrLibNextLine(PluginUtil*, Reference str, int32_t i)
{
	std::WSTR s = PluginUtil::refToWstr(str);
	for(; i < (int32_t)s.size(); ){
		WCHR c = s[i];
		if(c == L'\n') return 1;
		if(c == L'\r'){
			if(i+1 < (int32_t)s.size() && s[i+1] == L'\n') return 2;
		}
		i++;
	}
	return 0;
}

// SSZ: public ^/char trim(^/char str)  — strip leading/trailing ' \t\r\n'
static intptr_t SSZ_STDCALL StrLibTrim(PluginUtil*, Reference str)
{
	std::WSTR s = PluginUtil::refToWstr(str);
	auto isBlank = [](WCHR c){
		return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n';
	};
	size_t b = 0, e = s.size();
	while(b < e && isBlank(s[b])) b++;
	while(e > b && isBlank(s[e-1])) e--;
	return MakeStr(s.substr(b, e - b));
}

// SSZ: public index find(^/char ptn, ^/char str)  — first match index or -1
static int64_t SSZ_STDCALL StrLibFind(PluginUtil*, Reference str, Reference ptn)
{
	size_t p = PluginUtil::refToWstr(str).find(PluginUtil::refToWstr(ptn));
	return p == std::WSTR::npos ? -1 : (int64_t)p;
}

// SSZ: public index cFind(^/char cclass, ^/char str)
// — index of the first char of str that appears in cclass, or -1
static int64_t SSZ_STDCALL StrLibCFind(PluginUtil*, Reference str, Reference cclass)
{
	size_t p = PluginUtil::refToWstr(str).find_first_of(
		PluginUtil::refToWstr(cclass));
	return p == std::WSTR::npos ? -1 : (int64_t)p;
}

// SSZ: public ^ubyte sToU8(^/char s)  — UTF-16 -> UTF-8 bytes
static intptr_t SSZ_STDCALL StrLibSToU8(PluginUtil*, Reference sref)
{
	std::WSTR s = PluginUtil::refToWstr(sref);
	std::vector<uint8_t> out;
	for(size_t i = 0; i < s.size(); i++){
		uint32_t c = (uint32_t)(WCHR)s[i];
		if(
			(c >> 10) == 0x36 && i+1 < s.size()
			&& ((uint32_t)(WCHR)s[i+1] >> 10) == 0x37)
		{
			c = ((c & 0x3ff) << 10 | ((uint32_t)(WCHR)s[++i] & 0x3ff))
				+ 0x10000;
		}
		if(c < 0x80){
			out.push_back((uint8_t)c);
		}else if(c < 0x800){
			out.push_back((uint8_t)((c >> 6) | 0xC0));
			out.push_back((uint8_t)((c & 0x3f) | 0x80));
		}else if(c < 0x10000){
			out.push_back((uint8_t)((c >> 12) | 0xE0));
			out.push_back((uint8_t)(((c >> 6) & 0x3f) | 0x80));
			out.push_back((uint8_t)((c & 0x3f) | 0x80));
		}else{
			out.push_back((uint8_t)((c >> 18) | 0xF0));
			out.push_back((uint8_t)(((c >> 12) & 0x3f) | 0x80));
			out.push_back((uint8_t)(((c >> 6) & 0x3f) | 0x80));
			out.push_back((uint8_t)((c & 0x3f) | 0x80));
		}
	}
	return MakeBytes(out);
}

// SSZ: public ^char u8ToS(^/ubyte utf8)  — UTF-8 bytes -> UTF-16 string
static intptr_t SSZ_STDCALL StrLibU8ToS(PluginUtil*, Reference utf8ref)
{
	const uint8_t* p = (const uint8_t*)utf8ref.atpos();
	size_t n = (size_t)utf8ref.len();
	std::WSTR out;
	size_t i = 0;
	while(i < n){
		uint32_t c = p[i];
		if(c < 0xc0){
			// single byte — no extra
		}else if(c < 0xe0){
			c &= 0x1f;
			c = c << 6 | (p[++i] & 0x3f);
		}else if(c < 0xf0){
			c &= 0xf;
			c = c << 6 | (p[++i] & 0x3f);
			c = c << 6 | (p[++i] & 0x3f);
		}else if(c < 0xf8){
			c &= 0x7;
			c = c << 6 | (p[++i] & 0x3f);
			c = c << 6 | (p[++i] & 0x3f);
			c = c << 6 | (p[++i] & 0x3f);
		}
		if(c < 0x10000){
			out += (WCHR)c;
		}else{
			c -= 0x10000;
			out += (WCHR)(((c >> 10) & 0x3ff) | 0xd800);
			out += (WCHR)((c & 0x3ff) | 0xdc00);
		}
		i++;
	}
	return MakeStr(out);
}

// SSZ: public ^char percentEnc(^/char str)
// UTF-8 encode, then percent-encode everything except unreserved chars
// (A-Z a-z 0-9 - . _ ~).
static intptr_t SSZ_STDCALL StrLibPercentEnc(PluginUtil* pu, Reference str)
{
	std::WSTR s = PluginUtil::refToWstr(str);
	std::vector<uint8_t> utf8;
	for(size_t i = 0; i < s.size(); i++){
		uint32_t c = (uint32_t)(WCHR)s[i];
		if(
			(c >> 10) == 0x36 && i+1 < s.size()
			&& ((uint32_t)(WCHR)s[i+1] >> 10) == 0x37)
		{
			c = ((c & 0x3ff) << 10 | ((uint32_t)(WCHR)s[++i] & 0x3ff))
				+ 0x10000;
		}
		if(c < 0x80) utf8.push_back((uint8_t)c);
		else if(c < 0x800){
			utf8.push_back((uint8_t)((c >> 6) | 0xC0));
			utf8.push_back((uint8_t)((c & 0x3f) | 0x80));
		}else if(c < 0x10000){
			utf8.push_back((uint8_t)((c >> 12) | 0xE0));
			utf8.push_back((uint8_t)(((c >> 6) & 0x3f) | 0x80));
			utf8.push_back((uint8_t)((c & 0x3f) | 0x80));
		}else{
			utf8.push_back((uint8_t)((c >> 18) | 0xF0));
			utf8.push_back((uint8_t)(((c >> 12) & 0x3f) | 0x80));
			utf8.push_back((uint8_t)(((c >> 6) & 0x3f) | 0x80));
			utf8.push_back((uint8_t)((c & 0x3f) | 0x80));
		}
	}
	std::WSTR out;
	auto isUnreserved = [](uint8_t u){
		return
			('A' <= u && u <= 'Z') || ('a' <= u && u <= 'z')
			|| ('0' <= u && u <= '9')
			|| u == '-' || u == '.' || u == '_' || u == '~';
	};
	for(size_t i = 0; i < utf8.size(); i++){
		uint8_t u = utf8[i];
		if(isUnreserved(u)){
			out += (WCHR)u;
		}else{
			out += L'%';
			uint8_t hi = u >> 4, lo = u & 0xf;
			out += (WCHR)(hi < 10 ? L'0' + hi : L'A' + hi - 10);
			out += (WCHR)(lo < 10 ? L'0' + lo : L'A' + lo - 10);
		}
	}
	return MakeStr(out);
}

// SSZ: public ^char percentDec(^/char str)
// Decode %XX sequences into bytes, accumulate them as UTF-8, then convert
// to a UTF-16 string.
static intptr_t SSZ_STDCALL StrLibPercentDec(PluginUtil*, Reference str)
{
	std::WSTR s = PluginUtil::refToWstr(str);
	std::vector<uint8_t> utf8;
	std::WSTR out;
	auto flush = [&](){
		if(utf8.size() > 0){
			// reuse the u8ToS logic
			const uint8_t* p = utf8.data();
			size_t n = utf8.size(), i = 0;
			while(i < n){
				uint32_t c = p[i];
				if(c < 0xc0){
					// single byte
				}else if(c < 0xe0){
					c &= 0x1f;
					c = c << 6 | (p[++i] & 0x3f);
				}else if(c < 0xf0){
					c &= 0xf;
					c = c << 6 | (p[++i] & 0x3f);
					c = c << 6 | (p[++i] & 0x3f);
				}else if(c < 0xf8){
					c &= 0x7;
					c = c << 6 | (p[++i] & 0x3f);
					c = c << 6 | (p[++i] & 0x3f);
					c = c << 6 | (p[++i] & 0x3f);
				}
				if(c < 0x10000){
					out += (WCHR)c;
				}else{
					c -= 0x10000;
					out += (WCHR)(((c >> 10) & 0x3ff) | 0xd800);
					out += (WCHR)((c & 0x3ff) | 0xdc00);
				}
				i++;
			}
			utf8.clear();
		}
	};
	for(size_t i = 0; i < s.size(); ){
		if(s[i] == L'%' && i+2 < s.size()){
			auto hexVal = [](WCHR c) -> int{
				if(L'0' <= c && c <= L'9') return (int)(c - L'0');
				if(L'A' <= c && c <= L'F') return (int)(c - L'A') + 10;
				if(L'a' <= c && c <= L'f') return (int)(c - L'a') + 10;
				return -1;
			};
			int h = hexVal(s[i+1]), l = hexVal(s[i+2]);
			if(h >= 0 && l >= 0){
				utf8.push_back((uint8_t)((h << 4) | l));
				i += 3;
				continue;
			}
		}
		flush();
		out += s[i++];
	}
	flush();
	return MakeStr(out);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool string_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "uToSo",      "^char (ulong)",             (void*)StrLibUToSo      },
		{ "equ",        "bool (^/char, ^/char)",     (void*)StrLibEqu        },
		{ "toLower",    "^char (^/char)",            (void*)StrLibToLower    },
		{ "nextLine",   "int (index, ^/char)",       (void*)StrLibNextLine   },
		{ "trim",       "^/char (^/char)",           (void*)StrLibTrim       },
		{ "find",       "index (^/char, ^/char)",    (void*)StrLibFind       },
		{ "cFind",      "index (^/char, ^/char)",    (void*)StrLibCFind      },
		{ "sToU8",      "^ubyte (^/char)",           (void*)StrLibSToU8      },
		{ "u8ToS",      "^char (^/ubyte)",           (void*)StrLibU8ToS      },
		{ "percentEnc", "^char (^/char)",            (void*)StrLibPercentEnc },
		{ "percentDec", "^char (^/char)",            (void*)StrLibPercentDec },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "string";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
