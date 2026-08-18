// ============================================================================
// funcref.cpp — native `ref` / `func` delegate signature test fixture.
//
// Exercises the two signature shapes TypeNameToTokens (native_lib.hpp) now
// encodes for native libraries:
//
//   `ref`             -> REF_TOKEN NULL_TOKEN (DYNREF)  — dynamic reference
//   `func$void(int)`  -> FUNC_TOKEN SIGNATURE_TOKEN VOID_TOKEN ( INT_TOKEN )
//                        — delegate type, matching the parser's TypeNanika
//                        FUNC_TOKEN + DOLLAR_TOKEN encoding
//
//     lib fr = <funcref>;
//     ref r;
//     .fr.setRefInt(r=, 42);      // ref out-param  -> DynamicRef*
//     int v = .fr.getRefInt(r);   // ref value      -> DynamicRef (by value)
//     .fr.callVoid(.cb);          // func value     -> intptr_t slot
//
// ABI notes (all mirror the Lua bridge, ssz_script/lib/alpha/lua.ssz):
//   - arguments arrive reversed (last SSZ param = first C++ param)
//   - `ref=` out-params arrive as DynamicRef* (pointer to caller's slot)
//   - `ref` value params arrive as DynamicRef (16 bytes, copied to the slot)
//   - `func` value params arrive as intptr_t (the delegate handle)
// ============================================================================

#include <cstdint>
#include <cstring>

#include "sszdef.h"
#include "native_lib.hpp"
#include "arrayandref.hpp"

struct PluginUtil;

static intptr_t g_lastFunc = 0;

// ---------------------------------------------------------------------------
// ref (DYNREF) functions
// ---------------------------------------------------------------------------

// SSZ: void refNull(ref o=)
static void SSZ_STDCALL FrRefNull(PluginUtil*, DynamicRef* o)
{
	o->init();
}

// SSZ: void refCopy(ref o=, ref r)
static void SSZ_STDCALL FrRefCopy(PluginUtil*, DynamicRef r, DynamicRef* o)
{
	*o = r;
}

// SSZ: bool refIsNull(ref r)
static uint32_t SSZ_STDCALL FrRefIsNull(PluginUtil*, DynamicRef r)
{
	return r.obj.null() ? 1 : 0;
}

// SSZ: void setRefInt(ref o=, int v)  — allocate a 4-byte heap cell holding v
static void SSZ_STDCALL FrSetRefInt(PluginUtil*, int32_t v, DynamicRef* o)
{
	o->init();
	o->obj.refnew(1, sizeof(int32_t));
	if(!o->obj.null()) memcpy(o->obj.atpos(), &v, sizeof(int32_t));
	o->typ = INT_TYPEID;
}

// SSZ: int getRefInt(ref r)
static int32_t SSZ_STDCALL FrGetRefInt(PluginUtil*, DynamicRef r)
{
	if(r.obj.null()) return 0;
	int32_t v = 0;
	memcpy(&v, r.obj.atpos(), sizeof(int32_t));
	return v;
}

// ---------------------------------------------------------------------------
// func (delegate) functions
// ---------------------------------------------------------------------------

// SSZ: void callVoid(func$void(int) f)  — record the delegate handle
static void SSZ_STDCALL FrCallVoid(PluginUtil*, intptr_t f)
{
	g_lastFunc = f;
}

// SSZ: intptr_t lastFunc()  — retrieve the recorded delegate handle
static intptr_t SSZ_STDCALL FrLastFunc(PluginUtil*)
{
	return g_lastFunc;
}

// SSZ: void callOut(func$void(int=) f)  — func whose param is an out-param
static void SSZ_STDCALL FrCallOut(PluginUtil*, intptr_t f)
{
	g_lastFunc = f;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool funcref_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "refNull",   "void (ref=)",              (void*)FrRefNull   },
		{ "refCopy",   "void (ref=, ref)",         (void*)FrRefCopy   },
		{ "refIsNull", "bool (ref)",               (void*)FrRefIsNull },
		{ "setRefInt", "void (ref=, int)",         (void*)FrSetRefInt },
		{ "getRefInt", "int (ref)",                (void*)FrGetRefInt },
		{ "callVoid",  "void (func$void(int))",    (void*)FrCallVoid  },
		{ "callOut",   "void (func$void(int=))",   (void*)FrCallOut   },
		{ "lastFunc",  "index ()",                 (void*)FrLastFunc  },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "funcref";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
