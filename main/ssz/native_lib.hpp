#pragma once
//
// native_lib.hpp
//
// Native (C++) library registry for SSZ.
//
// A native library lets a script write
//
//     lib time = <time>;
//
// and have `time` resolve to functions implemented in C++ (e.g.
// ssz_script/lib/time.cpp) instead of a .ssz file.  Each native function
// is exposed to the SSZ compiler exactly like a `plugin` declaration:
// the module SourceTree gets a plugin-typed henshuu whose type string
// carries the SSZ signature tokens and whose last slot holds the C
// function pointer.  Calls are emitted with the plugin calling
// convention (PluginCall): the JIT passes the PluginUtil* (g_gpsf.sf)
// plus the declared arguments, and reads the result from the return slot.
//
// A native function must therefore have the plugin ABI:
//
//     uint32_t SSZ_STDCALL TickCount(PluginUtil*);
//     int64_t  SSZ_STDCALL UnixTime(PluginUtil*);
//     int64_t  SSZ_STDCALL Div(PluginUtil*, int64_t x, int64_t y);
//
// IMPORTANT — 32-bit SSZ arguments (int/uint/bool/float/...) arrive in the
// LOW 32 bits of an 8-byte slot, with the upper 32 bits unspecified.  Declare
// such parameters with their native 32-bit type (int32_t/uint32_t/float), not
// int64_t — reading 8 bytes would include garbage in the high half.  This
// matches the plugin bridges in main/ssz/bridge.cpp (e.g. ThreadDelay takes
// uint32_t).  Returns use the full slot, so a 64-bit return type is safe.
//
// The signature string describes the SSZ view of the function, e.g.
//     "uint ()"             ->  uint tickCount()
//     "long (long, long)"   ->  long div(long x, long y)
//     "^int (long)"         ->  ^int ymdhms(long time)
//

#include <string>
#include <vector>

#include "typeid.h"
#include "tokenkind.h"

// ---------------------------------------------------------------------
// SSZ signature -> token encoding
//
// A plugin-typed henshuu's type string is laid out like the token stream
// the parser produces for `plugin T name(params) = <lib>`:
//
//     PLUGIN_TOKEN
//     SIGNATURE_TOKEN
//     <return type tokens>
//     SHOUKAKKOOPEN_TOKEN
//     <parameter type tokens, comma separated>
//     SHOUKAKKOCLOSE_TOKEN
//     <intptr_t function pointer>
// ---------------------------------------------------------------------

namespace NativeLib
{

// ---------------------------------------------------------------------
// Struct / enum type-name resolution
//
// Native signature strings may reference SSZ struct and enum types
// (`&Event`, `&.Rect`, `&.f.File`, `|SDLKey`).  The token encoding for
// these is AND_TOKEN/OR_TOKEN plus the type's class id in the compiler's
// global funclist — the same encoding the parser produces for
// `plugin ... (:&Event=:)`.  native_lib.hpp cannot see SourceTree, so the
// caller (SourceTree::NativeLibFrom) supplies a resolver callback that
// looks the name up exactly like PathtoClassID does.
// ---------------------------------------------------------------------
struct NativeTypeContext
{
	void* state;
	// Resolve a type path ("Event", ".Rect", ".f.File") to a funclist
	// class id; wantclass=false means an enum.  Returns -1 on failure.
	int32_t (*resolveType)(void* state, const std::string& path, bool wantclass);
};

// Append the token encoding for a single SSZ type name ("long", "^int", ...).
// Struct/enum names (`&X`/`|X`, optionally dot-qualified) require a non-null
// ctx with a resolveType callback.
inline bool TypeNameToTokens(
	const std::string& t, std::vector<intptr_t>& out,
	const NativeTypeContext* ctx = nullptr)
{
	auto trim = [](std::string s){
		while(s.size() > 0 && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
		while(s.size() > 0 && (s[0] == ' ' || s[0] == '\t')) s.erase(0, 1);
		return s;
	};
	if(t.rfind("public ", 0) == 0){
		// "public <type>"  — public module variable (cross-module access)
		auto sub = trim(t.substr(7));
		if(!TypeNameToTokens(sub, out, ctx)) return false;
		out.insert(out.begin(), PUBLIC_TOKEN);
		return true;
	}
	if(!t.empty() && t.back() == '='){
		// "<type>="  — out-parameter (passed by reference), encoded exactly
		// like the parser's DAINYUU_TOKEN handling: base type tokens plus a
		// trailing ~DAINYUU_TOKEN marker.  The C function receives a pointer
		// to the caller's Reference slot as this parameter.
		auto sub = trim(t.substr(0, t.size()-1));
		if(!TypeNameToTokens(sub, out, ctx)) return false;
		out.push_back(~DAINYUU_TOKEN);
		return true;
	}
	if(t == "_t")    { out.push_back(TYPE_TOKEN);   return true; }
	if(t == "ref")   {
		// Bare `ref` (dynamic reference) — encoded exactly like the parser's
		// TypeNanika REF_TOKEN case (REF_TOKEN + NULL_TOKEN = DYNREF).
		out.push_back(REF_TOKEN);
		out.push_back(NULL_TOKEN);
		return true;
	}
	if(t.rfind("func", 0) == 0){
		// func$<ret>(<params>) — delegate type, encoded like the parser's
		// TypeNanika FUNC_TOKEN + DOLLAR_TOKEN cases:
		//   FUNC_TOKEN SIGNATURE_TOKEN <ret> ( <p1> , <p2> ... )
		// Params may themselves be func types (nested parens) or carry
		// out-param `=` markers (trailing ~DAINYUU_TOKEN).
		auto rest = trim(t.substr(4));
		if(rest.size() == 0 || rest[0] != '$') return false;
		rest = trim(rest.substr(1));
		auto paren = rest.find('(');
		if(paren == std::string::npos || rest.back() != ')') return false;
		auto fret = trim(rest.substr(0, paren));
		auto fparams = rest.substr(paren + 1, rest.size() - paren - 2);
		out.push_back(FUNC_TOKEN);
		out.push_back(SIGNATURE_TOKEN);
		if(!TypeNameToTokens(fret, out, ctx)) return false;
		out.push_back(SHOUKAKKOOPEN_TOKEN);
		// Split params on commas at paren-depth 0 (nested func parens and
		// the `=` marker don't open depth; only '(' / ')' do).
		int depth = 0;
		size_t start = 0;
		bool first = true;
		auto fflush = [&](size_t end, bool& ok){
			auto piece = trim(fparams.substr(start, end - start));
			if(piece.size() == 0) return;
			if(!first) out.push_back(COMMA_TOKEN);
			if(!TypeNameToTokens(piece, out, ctx)){ ok = false; return; }
			first = false;
		};
		bool fok = true;
		for(size_t i = 0; i <= fparams.size(); i++){
			char c = i < fparams.size() ? fparams[i] : ',';
			if(c == '('){ depth++; continue; }
			if(c == ')'){ depth--; continue; }
			if(c == ',' && depth == 0){
				fflush(i, fok);
				if(!fok) return false;
				start = i + 1;
			}
		}
		out.push_back(SHOUKAKKOCLOSE_TOKEN);
		return true;
	}
	if(t == "void")   { out.push_back(VOID_TOKEN);   return true; }
	if(t == "bool")   { out.push_back(BOOL_TOKEN);   return true; }
	if(t == "byte")   { out.push_back(BYTE_TOKEN);   return true; }
	if(t == "ubyte")  { out.push_back(UBYTE_TOKEN);  return true; }
	if(t == "short")  { out.push_back(SHORT_TOKEN);  return true; }
	if(t == "ushort") { out.push_back(USHORT_TOKEN); return true; }
	if(t == "int")    { out.push_back(INT_TOKEN);    return true; }
	if(t == "uint")   { out.push_back(UINT_TOKEN);   return true; }
	if(t == "long")   { out.push_back(LONG_TOKEN);   return true; }
	if(t == "ulong")  { out.push_back(ULONG_TOKEN);  return true; }
	if(t == "float")  { out.push_back(FLOAT_TOKEN);  return true; }
	if(t == "double") { out.push_back(DOUBLE_TOKEN); return true; }
	if(t == "char")   { out.push_back(CHAR_TOKEN);   return true; }
	if(t == "index")  { out.push_back(INDEX_TOKEN);  return true; }
	if(t.size() >= 2 && t[0] == '/'){
		// /<type>  — string type (e.g. /char), encoded as WARU_TOKEN + <type>
		auto sub = trim(t.substr(1));
		std::vector<intptr_t> tmp;
		if(!TypeNameToTokens(sub, tmp, ctx)) return false;
		out.push_back(WARU_TOKEN);
		for(auto tk : tmp) out.push_back(tk);
		return true;
	}
	if(t.size() >= 2 && t[0] == '^'){
		// ^<type>  — pointer / reference
		auto sub = trim(t.substr(1));
		if(sub == "null"){ out.push_back(REF_TOKEN); out.push_back(NULL_TOKEN); return true; }
		std::vector<intptr_t> tmp;
		if(!TypeNameToTokens(sub, tmp)) return false;
		out.push_back(REF_TOKEN);
		for(auto tk : tmp) out.push_back(tk);
		return true;
	}
	if(t.size() >= 2 && t[0] == '%'){
		// %<type>  — dynamic list
		auto sub = trim(t.substr(1));
		if(sub == "null"){ out.push_back(LIST_TOKEN); out.push_back(NULL_TOKEN); return true; }
		std::vector<intptr_t> tmp;
		if(!TypeNameToTokens(sub, tmp)) return false;
		out.push_back(LIST_TOKEN);
		for(auto tk : tmp) out.push_back(tk);
		return true;
	}
	if(t.size() >= 2 && (t[0] == '&' || t[0] == '|')){
		// &<struct> / |<enum> — class/struct and enum types, optionally
		// dot-qualified (&.Rect, &.f.File, |.sdl.K).  Encoded exactly like
		// the parser's TypeNanika OR/AND case: the marker token plus the
		// type's funclist class id, resolved by the caller-supplied
		// callback against the importing module's scope.
		bool wantclass = t[0] == '&';
		auto sub = trim(t.substr(1));
		if(sub.empty() || ctx == nullptr || ctx->resolveType == nullptr) return false;
		int32_t ci = ctx->resolveType(ctx->state, sub, wantclass);
		if(ci < 0) return false;
		out.push_back(wantclass ? AND_TOKEN : OR_TOKEN);
		out.push_back((intptr_t)ci);
		return true;
	}
	return false;
}

// Build the plugin type token string (sans function pointer) from an SSZ
// signature like "uint ()" or "long (long, long)".  Pass a non-null ctx to
// allow struct/enum types in the signature.
inline bool BuildPluginType(
	const std::string& sig, std::vector<intptr_t>& type,
	const NativeTypeContext* ctx = nullptr)
{
	// Split "<ret> (<params>)" at the top-level '(' — the one whose
	// matching ')' is the final character.  A func-typed return
	// (e.g. "func$void(int) (int)") has its own parens before it, so a
	// plain first-'(' split would land inside the return type.
	size_t paren = std::string::npos;
	{
		int depth = 0;
		for(size_t i = sig.size(); i-- > 0; ){
			if(sig[i] == ')') depth++;
			else if(sig[i] == '('){
				if(depth > 0){
					depth--;
					if(depth == 0){ paren = i; break; }  // matches the final ')'
				}else{
					break;
				}
			}
		}
	}
	if(paren == std::string::npos || sig.back() != ')') return false;
	auto ret = sig.substr(0, paren);
	std::string params;
	if(sig.size() > paren + 2){
		params = sig.substr(paren + 1, sig.size() - paren - 2);
	}
	auto trim = [](std::string s){
		while(s.size() > 0 && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
		while(s.size() > 0 && (s[0] == ' ' || s[0] == '\t')) s.erase(0, 1);
		return s;
	};
	ret = trim(ret);
	params = trim(params);

	type.clear();
	type.push_back(PLUGIN_TOKEN);
	type.push_back(SIGNATURE_TOKEN);
	if(!TypeNameToTokens(ret, type, ctx)) return false;
	type.push_back(SHOUKAKKOOPEN_TOKEN);
	if(params.size() > 0 && trim(params).size() > 0){
		// Split on commas at paren-depth 0 so func-typed params
		// ("func$void(int, int)") stay intact.
		int depth = 0;
		size_t pos = 0;
		bool first = true;
		for(size_t i = 0; i <= params.size(); i++){
			char c = i < params.size() ? params[i] : ',';
			if(c == '('){ depth++; continue; }
			if(c == ')'){ depth--; continue; }
			if(c == ',' && depth == 0){
				auto piece = trim(params.substr(pos, i - pos));
				if(piece.size() > 0){
					if(!first) type.push_back(COMMA_TOKEN);
					if(!TypeNameToTokens(piece, type, ctx)) return false;
					first = false;
				}
				pos = i + 1;
			}
		}
	}
	type.push_back(SHOUKAKKOCLOSE_TOKEN);
	return true;
}

struct NativeFunction
{
	std::string name;
	std::string signature;  // SSZ signature, e.g. "long (long, long)"
	void* fnptr;            // C function with plugin ABI
};

// A module-level variable exposed by a native library (e.g. math's
// `randseed`).  Synthesized as an ordinary SSZ module variable: reads and
// writes go through the module's own variable frame, sized by the declared
// type.  Note that the frame slot is separate from any state a C++ function
// keeps internally — a native library that needs its variables to track
// native state must either keep that state in C++ (and treat the registered
// variable as interface parity) or accept the frame-backed value as the
// authoritative one.
struct NativeVariable
{
	std::string name;
	std::string type;       // SSZ type name, e.g. "int"
};

struct NativeLibrary
{
	std::string name;
	std::vector<NativeFunction> functions;
	std::vector<NativeVariable> variables;
};

// Registry: library name -> NativeLibrary.
// Populated at startup by each native library's register function
// (e.g. time_lib_register() in ssz_script/lib/time.cpp).
inline std::vector<NativeLibrary>& Registry()
{
	static std::vector<NativeLibrary> registry;
	return registry;
}

inline bool RegisterLibrary(const NativeLibrary& lib)
{
	for(auto& l : Registry()){
		if(l.name == lib.name) return false;  // already registered
	}
	Registry().push_back(lib);
	return true;
}

inline const NativeLibrary* FindLibrary(const std::string& name)
{
	for(auto& l : Registry()){
		if(l.name == name) return &l;
	}
	return nullptr;
}

}  // namespace NativeLib
