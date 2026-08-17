// ============================================================================
// table.cpp — native (C++) core for the SSZ `table` library.
//
// Declared in SSZ scripts as:
//
//     lib t = <table>;
//
// The NameTable/IntTable template structs and the intHash<_t> template are
// deeply template-bound (their fields and delegate parameters are typed by
// the template argument), so they stay defined in ssz_script/lib/table.ssz.
// The one fully concrete public function, `hash(^/char str)`, is implemented
// here and delegated from table.ssz:
//
//     public uint hash(^/char str)
//     {
//       ret .t.hash(str);
//     }
//
// ABI note: string parameters arrive as `Reference` values; convert them
// with ikemen::ssz_bridge::refToWstring().  Arguments arrive reversed vs.
// the SSZ declaration, but this function has a single parameter so the C++
// side receives it directly.
// ============================================================================

#include <cstdint>
#include <string>

#include "sszdef.h"
#include "bridge.hpp"
#include "native_lib.hpp"

struct PluginUtil;
struct Reference;

// ---------------------------------------------------------------------------
// Functions (plugin ABI)
// ---------------------------------------------------------------------------

// SSZ: public uint hash(^/char str)
//
//     uint h = 0x0;
//     loop{index i = 0; while; do:
//       h += (uint)i ^ (uint)str[i];
//       i++;
//     while i < #str:
//     }
//     ret h;
//
// Characters are UTF-16 code units (WCHR), the same units `str[i]` indexes.
static uint32_t SSZ_STDCALL TableLibHash(PluginUtil* pu, Reference str)
{
	std::wstring wstr = ikemen::ssz_bridge::refToWstring(pu, str);
	uint32_t h = 0;
	for(size_t i = 0; i < wstr.size(); i++){
		h += (uint32_t)i ^ (uint32_t)wstr[i];
	}
	return h;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool table_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "hash", "uint (^/char)", (void*)TableLibHash },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "table";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
