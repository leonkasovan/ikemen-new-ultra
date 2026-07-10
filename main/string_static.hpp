#pragma once
//
// string_static.hpp
//
// Statically register every function exported by ssz_script/lib/string.ssz
// so that the SSZ runtime resolves them without loading string.ssz.
//
// When IKEMEN_NATIVE_STRING_LIB=1, native C++ string_util functions replace
// the SSZ string library. When 0, the SSZ script is used instead.

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_STRING_LIB

struct PluginUtil;
struct Reference;

extern "C"
{
	// ── Basic string operations ──
	void     SSZ_STDCALL string_trim(PluginUtil*, Reference*, Reference);
	void     SSZ_STDCALL string_toLower(PluginUtil*, Reference*, Reference);
	bool     SSZ_STDCALL string_equ(PluginUtil*, Reference, Reference);
	intptr_t SSZ_STDCALL string_find(PluginUtil*, Reference, Reference);
	intptr_t SSZ_STDCALL string_cFind(PluginUtil*, Reference, Reference);
	bool     SSZ_STDCALL string_cMatch(PluginUtil*, Reference, wchar_t);
	void     SSZ_STDCALL string_splitLines(PluginUtil*, Reference*, Reference);
	void     SSZ_STDCALL string_split(PluginUtil*, Reference*, Reference, Reference);
	void     SSZ_STDCALL string_join(PluginUtil*, Reference*, Reference, Reference);
	int      SSZ_STDCALL string_nextLine(PluginUtil*, intptr_t*, Reference);

	// ── Number-to-string conversions ──
	void     SSZ_STDCALL string_uToSo(PluginUtil*, Reference*, uint64_t);
	void     SSZ_STDCALL string_uToSx(PluginUtil*, Reference*, uint64_t);
	void     SSZ_STDCALL string_uToSX(PluginUtil*, Reference*, uint64_t);

	// ── UTF-8 / percent encoding ──
	void     SSZ_STDCALL string_sToU8(PluginUtil*, Reference*, Reference);
	void     SSZ_STDCALL string_u8ToS(PluginUtil*, Reference*, Reference);
	void     SSZ_STDCALL string_percentEnc(PluginUtil*, Reference*, Reference);
	void     SSZ_STDCALL string_percentDec(PluginUtil*, Reference*, Reference);

	// ── Character operations ──
	wchar_t  SSZ_STDCALL string_toLowerChar(PluginUtil*, wchar_t);

	// ── Format object (opaque handle-based) ──
	void*    SSZ_STDCALL Format_new(PluginUtil*);
	void     SSZ_STDCALL Format_delete(PluginUtil*, void*);
	wchar_t  SSZ_STDCALL Format_set(PluginUtil*, void*, Reference);
	bool     SSZ_STDCALL Format_isError(PluginUtil*, void*);
	void     SSZ_STDCALL Format_putSpace(PluginUtil*, void*, int);
	wchar_t  SSZ_STDCALL Format_d(PluginUtil*, void*, int64_t);
	wchar_t  SSZ_STDCALL Format_u(PluginUtil*, void*, uint64_t);
	wchar_t  SSZ_STDCALL Format_f(PluginUtil*, void*, double);
	wchar_t  SSZ_STDCALL Format_c(PluginUtil*, void*, wchar_t);
	wchar_t  SSZ_STDCALL Format_s(PluginUtil*, void*, Reference);
	void     SSZ_STDCALL Format_getOut(PluginUtil*, void*, Reference*);

	// ── String-to-number template specializations ──
	bool     SSZ_STDCALL sToNumber_int(PluginUtil*, int32_t*, Reference);
	bool     SSZ_STDCALL sToNumber_long(PluginUtil*, int64_t*, Reference);
	bool     SSZ_STDCALL sToNumber_float(PluginUtil*, float*, Reference);
	bool     SSZ_STDCALL sToNumber_double(PluginUtil*, double*, Reference);
	int32_t  SSZ_STDCALL sToN_int(PluginUtil*, Reference);
	int64_t  SSZ_STDCALL sToN_long(PluginUtil*, Reference);
	double   SSZ_STDCALL sToN_double(PluginUtil*, Reference);

	// ── Array utility template specializations ──
	void     SSZ_STDCALL string_toHex(PluginUtil*, Reference*, Reference);
	void     SSZ_STDCALL string_toUbyte(PluginUtil*, Reference*, Reference);
	void     SSZ_STDCALL string_copy(PluginUtil*, Reference*, Reference);
	void     SSZ_STDCALL string_clone(PluginUtil*, Reference*, Reference);

	// ── Split-and-convert template specializations ──
	void     SSZ_STDCALL svToAry_int(PluginUtil*, Reference*, Reference, Reference);
	void     SSZ_STDCALL svToAry_double(PluginUtil*, Reference*, Reference, Reference);
}

inline bool string_static_register()
{
	static const SSZ_FunctionEntry string_mapping[] =
	{
		{ "trim",            (void*)string_trim            },
		{ "toLower",         (void*)string_toLower         },
		{ "equ",             (void*)string_equ             },
		{ "find",            (void*)string_find            },
		{ "cFind",           (void*)string_cFind           },
		{ "cMatch",          (void*)string_cMatch          },
		{ "splitLines",      (void*)string_splitLines      },
		{ "split",           (void*)string_split           },
		{ "join",            (void*)string_join            },
		{ "nextLine",        (void*)string_nextLine        },
		{ "uToSo",           (void*)string_uToSo           },
		{ "uToSx",           (void*)string_uToSx           },
		{ "uToSX",           (void*)string_uToSX           },
		{ "sToU8",           (void*)string_sToU8           },
		{ "u8ToS",           (void*)string_u8ToS           },
		{ "percentEnc",      (void*)string_percentEnc      },
		{ "percentDec",      (void*)string_percentDec      },
		{ "toLowerChar",     (void*)string_toLowerChar     },
		{ "Format_new",      (void*)Format_new             },
		{ "Format_delete",   (void*)Format_delete          },
		{ "Format_set",      (void*)Format_set             },
		{ "Format_isError",  (void*)Format_isError         },
		{ "Format_putSpace", (void*)Format_putSpace        },
		{ "Format_d",        (void*)Format_d               },
		{ "Format_u",        (void*)Format_u               },
		{ "Format_f",        (void*)Format_f               },
		{ "Format_c",        (void*)Format_c               },
		{ "Format_s",        (void*)Format_s               },
		{ "Format_getOut",   (void*)Format_getOut          },
		{ "sToNumber_int",   (void*)sToNumber_int          },
		{ "sToNumber_long",  (void*)sToNumber_long         },
		{ "sToNumber_float", (void*)sToNumber_float        },
		{ "sToNumber_double",(void*)sToNumber_double       },
		{ "sToN_int",        (void*)sToN_int               },
		{ "sToN_long",       (void*)sToN_long              },
		{ "sToN_double",     (void*)sToN_double            },
		{ "toHex",           (void*)string_toHex           },
		{ "toUbyte",         (void*)string_toUbyte         },
		{ "copy",            (void*)string_copy            },
		{ "clone",           (void*)string_clone           },
		{ "svToAry_int",     (void*)svToAry_int            },
		{ "svToAry_double",  (void*)svToAry_double         },
	};

	return SSZ_RegisterFunction(
		"string",
		string_mapping,
		sizeof(string_mapping) / sizeof(string_mapping[0]));
}

#else

static inline bool string_static_register()
{
	// IKEMEN_NATIVE_STRING_LIB=0 — SSZ string.ssz script used instead.
	return true;
}

#endif
