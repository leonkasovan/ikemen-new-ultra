// ============================================================================
// sdlevent.cpp — native (C++) implementation of the SSZ `sdlevent` library.
//
// Converted from ssz_script/lib/alpha/sdlevent.ssz.  Only the `&Key` struct's
// methods are convertible: `reset()` and `checkDown()` are pure field
// logic.  The stateful `event()`/`eventUpdate()` functions read and write a
// large module-variable state (nexttime, lastdraw, dozens of key flags, the
// `sdle` event struct) and stay in SSZ.
//
// The struct itself stays defined in the .ssz as a data container; its
// methods delegate here with the fields passed as out-params:
//
//     lib ev = <sdlevent>;
//     public void checkDown(|.sdl.K k, ushort m)
//     {
//       .ev.keyCheckDown(`key=, `shift=, `ctrl=, `alt=, `down=, k, m);
//     }
//
// The `|.sdl.K` enum type is resolved against the importing module's scope
// (dot-qualified through the `sdl` lib import) — the same
// NativeTypeContext resolution the sdlplugin bridge uses.
//
// ABI: plugin convention — args arrive reversed (last SSZ param is first C++
// param); 32-bit args in the low 32 bits of an 8-byte slot; `type=` out-params
// arrive as pointers.
// ============================================================================

#include "sszdef.h"
#include "arrayandref.hpp"   // full Reference definition (for stdcall decoration)
#include "native_lib.hpp"

struct PluginUtil;

// SDL key-modifier flags (KMOD_*), matching the constants in alpha/sdlplugin.ssz.
#define KMOD_LSHIFT 0x0001
#define KMOD_RSHIFT 0x0002
#define KMOD_LCTRL  0x0040
#define KMOD_RCTRL  0x0080
#define KMOD_LALT   0x0100
#define KMOD_RALT   0x0200

// &Key::reset()  —  `down = false;
static void SSZ_STDCALL KeyLibReset(PluginUtil*, bool* down)
{
	*down = false;
}

// &Key::checkDown(|.sdl.K k, ushort m)  —
//   `down |= `key == k
//            && `shift == ((m & (KMOD_LSHIFT|KMOD_RSHIFT)) != 0)
//            && `ctrl  == ((m & (KMOD_LCTRL |KMOD_RCTRL )) != 0)
//            && `alt   == ((m & (KMOD_LALT  |KMOD_RALT  )) != 0);
//
// SSZ order: key=, shift=, ctrl=, alt=, down=, k, m
// C++ order (reversed): m, k, down=, alt=, ctrl=, shift=, key=
static void SSZ_STDCALL KeyLibCheckDown(
	PluginUtil*, uint32_t m, int32_t k,
	bool* down, bool* alt, bool* ctrl, bool* shift, int32_t* key)
{
	uint16_t um = (uint16_t)m;
	bool hit =
		*key == k
		&& *shift == (((um & (KMOD_LSHIFT | KMOD_RSHIFT)) != 0) ? true : false)
		&& *ctrl  == (((um & (KMOD_LCTRL  | KMOD_RCTRL )) != 0) ? true : false)
		&& *alt   == (((um & (KMOD_LALT   | KMOD_RALT  )) != 0) ? true : false);
	*down = *down || hit;
}

extern "C" bool sdlevent_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "keyReset",     "void (bool=)",                    (void*)KeyLibReset     },
		{ "keyCheckDown", "void (|.sdl.K=, bool=, bool=, bool=, bool=, |.sdl.K, ushort)", (void*)KeyLibCheckDown },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "sdlevent";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
