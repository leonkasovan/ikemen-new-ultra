// ============================================================================
// regex.cpp — native (C++) implementation of the SSZ `regex` library's
// `&Regex` struct methods.
//
// Consumed from ssz_script/lib/regex.ssz, which keeps the struct (field
// `index re`) in SSZ and delegates the method bodies here:
//
//     lib rx = <regex>;
//     ...
//     public ^char init(^/char reText, int flag)
//     {
//       ^char error;
//       `re = .rx.newRegex(reText, (flag&`I) != 0, error=);
//       ret error;
//     }
//
// The module is resolved by the native-lib registry (main/ssz/native_lib.hpp).
// The wrappers below forward to the clean native operations in
// main/regex/regex.cpp (the same functions the static bridge wraps):
// NewRegex/DeleteRegex/RegexSearch.  RegexSearch's `^^/char` result is a
// list of *slices* into the source string; the construction mirrors
// main/ssz/bridge.cpp's wrapper so both paths stay identical.  The static
// `regex` plugin and this native lib coexist: `plugin ... = <regex>`
// declarations still resolve to the static registry.
// ============================================================================

#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#include <regex>
#define RNS std
#else
#include <boost/regex.hpp>
#define RNS boost
#endif

#include "sszdef.h"
#include "native_lib.hpp"
#include "arrayandref.hpp"
#include "pluginutil.hpp"
#include "bridge.hpp"   // ikemen::ssz_bridge::RegexMatchInfo

struct PluginUtil;

// Clean natives defined in main/regex/regex.cpp
extern RNS::wregex* SSZ_STDCALL NewRegex(
	bool i, const std::wstring& ptn, std::wstring* error);
extern void SSZ_STDCALL DeleteRegex(RNS::wregex* re);
extern std::vector<ikemen::ssz_bridge::RegexMatchInfo> SSZ_STDCALL RegexSearch(
	const std::wstring& str, RNS::wregex* re);

// SSZ `^/char` param -> native wstring (same helper as the other native libs).
static std::wstring RefToWstring(Reference r)
{
#ifdef _WIN32
	return PluginUtil::refToWstr(r);
#else
	return PluginUtil::wToGw(PluginUtil::refToWstr(r));
#endif
}

// ---------------------------------------------------------------------------
// &Regex struct methods — the struct stays in SSZ (field `index re`), the
// method bodies delegate here with the handle passed by value or as an
// out-param for the zeroing case.  Args arrive reversed vs. the SSZ
// declaration; out-params as pointers to the caller's slots.
// ---------------------------------------------------------------------------

// SSZ: &Regex.init(^/char reText, bool ignorecase, ^char error=) -> index
// Args arrive reversed: (pu, error*, ic, reText).
static intptr_t SSZ_STDCALL RegexLibNewRegex(
	PluginUtil* pu, Reference* error, bool ic, Reference ptn)
{
	error->releaseanddelete();
	std::wstring err;
	RNS::wregex* re = NewRegex(ic, RefToWstring(ptn), &err);
	if(!err.empty()){
		pu->wstrToRef(*error, err);
		delete re;
		re = nullptr;
	}
	return (intptr_t)re;
}

// SSZ: &Regex.clear() — delete the handle and zero the field.
// Args arrive reversed: (pu, re*).
static void SSZ_STDCALL RegexLibDeleteRegex(PluginUtil*, intptr_t* re)
{
	if(*re != 0){
		DeleteRegex((RNS::wregex*)*re);
		*re = 0;
	}
}

// SSZ: &Regex.search(^/char str, ^^/char matches=) — list of slices.
// Args arrive reversed: (pu, matches*, str, re).
static void SSZ_STDCALL RegexLibSearch(
	PluginUtil* pu, Reference* matches, Reference str, intptr_t re)
{
	pu->setSSZFunc();
	matches->releaseanddelete();
	if(re == 0) return;

	std::vector<ikemen::ssz_bridge::RegexMatchInfo> result =
		RegexSearch(RefToWstring(str), (RNS::wregex*)re);
	if(result.empty()) return;

	matches->refnew((intptr_t)result.size(), sizeof(Reference));
	for(size_t i = 0; i < result.size(); i++){
		auto& m = result[i];
		((Reference*)matches->atpos())[i].init();
		if(m.len > 0){
			((Reference*)matches->atpos())[i].copy(str);
			((Reference*)matches->atpos())[i].position += m.pos * sizeof(WCHR);
			((Reference*)matches->atpos())[i].length = m.len * sizeof(WCHR);
		}else{
			((Reference*)matches->atpos())[i].position =
				(m.pos != -1 ? str.pos() + m.pos : -1) * sizeof(WCHR);
		}
	}
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool regex_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "newRegex",    "index (^/char, bool, ^char=)",      (void*)RegexLibNewRegex    },
		{ "deleteRegex", "void (index=)",                     (void*)RegexLibDeleteRegex },
		{ "regexSearch", "void (index, ^/char, ^^/char=)",    (void*)RegexLibSearch      },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "regex";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
