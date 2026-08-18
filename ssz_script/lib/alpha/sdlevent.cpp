// ============================================================================
// sdlevent.cpp — native (C++) implementation of the SSZ `sdlevent` library.
//
// Converted from ssz_script/lib/alpha/sdlevent.ssz.  The `&Key` struct's
// methods (`reset()`/`checkDown()`) are pure field logic, and the timing
// core of `event(fps)` (the `nexttime`/`lastdraw`/`nexttimeFractionalPart`/
// `fskip` branch) is deterministic arithmetic, so both are native.  The
// stateful `eventUpdate()` event pump and the key-flag clears stay in SSZ
// (they read/write the public module-variable state through the `sdle`
// struct and dozens of `*Key` flags).
//
// The structs stay defined in the .ssz as data containers; methods delegate
// here with the fields passed as out-params:
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
// The timing core is `eventTiming(fps, now, nexttime=, lastdraw=, frac=,
// fskip=)` — a faithful transcription of the `branch` in the original
// `event(fps)`, with `now` passed in (the wrapper calls `.sdl.GetTicks(::)`)
// so the arithmetic is deterministic and testable.  Semantics verified by
// probe: all comparisons are uint32 wraparound (0d250/0d17 literals coerce
// to uint in the mixed conds); a matched `cond` body runs then falls to the
// `comm` block (lastdraw=now, fskip=false); the `else` body's `break` skips
// `comm` (fskip=true, lastdraw untouched).
//
// ABI: plugin convention — args arrive reversed (last SSZ param is first C++
// param); 32-bit args in the low 32 bits of an 8-byte slot; `type=` out-params
// arrive as pointers.
// ============================================================================

#include "sszdef.h"
#include "arrayandref.hpp"   // full Reference definition (for stdcall decoration)
#include "native_lib.hpp"

struct PluginUtil;

// The sdlplugin bridge fnptr used by the timing core (main/ssz/bridge.cpp).
extern "C" void SSZ_STDCALL Delay(PluginUtil*, uint32_t);

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

// event(fps) timing core — the `branch` of the original SSZ, with `now`
// passed in by the wrapper (which calls .sdl.GetTicks(::)).  Transcribed
// exactly (uint wraparound throughout; probed semantics):
//
//   uint uWait = (uint)1000 / (uint)fps;
//   // nexttimeNext():
//   .nexttime += `uWait;
//   .nexttimeFractionalPart += (float)(1000 % `fps) / (float)`fps;
//   if(.nexttimeFractionalPart >= 1.0){ .nexttime++; .nexttimeFractionalPart -= 1.0; }
//   branch{
//     uint now = .sdl.GetTicks(::);
//     uint dif = .nexttime - now;
//     nexttimeNext();
//   cond dif < uWait + 0x2:
//     .sdl.Delay(:dif:);
//   cond now - .lastdraw > 0d250:
//   cond dif+0d17 < 0d17:
//   else:
//     if(-dif > 0d150){ .nexttime = now; nexttimeNext(); }
//     .fskip = true;
//     break;                        // skips comm
//   comm:
//     .lastdraw = now;
//     .fskip = false;
//   }
//
// C++ (reversed args): pu, fskip=, frac=, lastdraw=, nexttime=, now, fps
static void SSZ_STDCALL EventLibTiming(
	PluginUtil* pu, bool* fskip, float* frac, uint32_t* lastdraw,
	uint32_t* nexttime, uint32_t now, int32_t fps)
{
	uint32_t uWait = 1000u / (uint32_t)fps;
	auto nexttimeNext = [&](){
		*nexttime += uWait;
		*frac += (float)(1000 % fps) / (float)fps;
		if(*frac >= 1.0f){
			(*nexttime)++;
			*frac -= 1.0f;
		}
	};
	uint32_t dif = *nexttime - now;
	nexttimeNext();
	if(dif < uWait + 0x2u){
		Delay(pu, dif);
	}else if((now - *lastdraw) > 250u){       // 0d250 as uint
		// empty body -> comm
	}else if((dif + 0x11u) < 0x11u){          // 0d17 wraps as uint
		// empty body -> comm
	}else{
		if((0u - dif) > 150u){                // -dif wraps as uint
			*nexttime = now;
			nexttimeNext();
		}
		*fskip = true;
		return;                               // break: skip comm
	}
	// comm:
	*lastdraw = now;
	*fskip = false;
}

	extern "C" bool sdlevent_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "keyReset",     "void (bool=)",                    (void*)KeyLibReset     },
		{ "keyCheckDown", "void (|.sdl.K=, bool=, bool=, bool=, bool=, |.sdl.K, ushort)", (void*)KeyLibCheckDown },
		{ "eventTiming",  "void (int, uint, uint=, uint=, float=, bool=)", (void*)EventLibTiming },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "sdlevent";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
