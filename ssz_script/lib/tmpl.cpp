// ============================================================================
// tmpl.cpp — native `_t` (TYPE_TOKEN) generic-function test fixture.
//
// Exercises the JIT's call-site type inference for native signatures that
// carry the template placeholder `_t` (encoded as TYPE_TOKEN by
// TypeNameToTokens in native_lib.hpp).  The concrete type is inferred from
// the call-site arguments in KansuuPointer (jitcompiler.hpp) — the first
// parameter mentioning `_t` binds it to that argument's type, then every
// TYPE_TOKEN in the signature (params and return) is substituted with the
// binding before type-checking and code emission.
//
//     lib t = <tmpl>;
//     int  m = .t.min(3, 5);        // _t = int  (from the arguments)
//     ^int a = .t.make(99);         // _t = int, returns ^int
//
// 64-bit (`long`) numeric instantiations are exercised here — `_t` value
// params arrive in full 8-byte slots and the return uses the full slot, so
// the C++ side declares int64_t.  (An `int`-element native function would
// declare int32_t per the 32-bit-slot rule — one C implementation serves one
// element type.)  The ABI otherwise follows the plugin convention (args
// reversed, first arg PluginUtil*).  See PROGRESS.md for what this unblocks
// (and what stack/table still need beyond it).
// ============================================================================

#include <cstdint>
#include <cstring>

#include "sszdef.h"
#include "native_lib.hpp"
#include "arrayandref.hpp"
#include "pluginutil.hpp"

struct PluginUtil;

// ---------------------------------------------------------------------------
// Functions (plugin ABI — arguments arrive reversed vs. the SSZ declaration)
// ---------------------------------------------------------------------------

// SSZ: _t min(_t a, _t b)  — 64-bit numeric element type.
// ABI: _t value args arrive in 8-byte slots (full slot for long), and the
// return uses the full slot, so an int64_t declaration is the safe generic
// choice — it is correct for `long` instantiations.  (A native _t function
// whose element type is `int` would instead declare int32_t per the 32-bit
// slot rule; one C implementation serves one element type.)
static int64_t SSZ_STDCALL TmplLibMin(PluginUtil*, int64_t b, int64_t a)
{
	return a < b ? a : b;
}

// SSZ: _t add(_t a, _t b)
static int64_t SSZ_STDCALL TmplLibAdd(PluginUtil*, int64_t b, int64_t a)
{
	return a + b;
}

// SSZ: void inc(_t v=)  — out-param: write *vp + 1 back into the caller's slot
static void SSZ_STDCALL TmplLibInc(PluginUtil*, int64_t* vp)
{
	(*vp)++;
}

// SSZ: void fill(^_t buf=, _t item)  — overwrite every element of the ^_t
// array field with item.  The `^_t buf=` out-param arrives as a pointer to
// the caller's Reference slot (the `&X` field-delegation pattern); write in
// place through it.
static void SSZ_STDCALL TmplLibFill(PluginUtil*, int64_t item, Reference* bufp)
{
	Reference& buf = *bufp;
	if(buf.null()) return;
	intptr_t n = buf.len() / (intptr_t)sizeof(int64_t);
	int64_t* p = (int64_t*)buf.atpos();
	for(intptr_t i = 0; i < n; i++) p[i] = item;
}

// SSZ: ^_t make(_t v)  — heap Reference wrapping a single _t element.
// Returns the address of the heap Reference (null on allocation failure);
// the JIT unpacks the returned struct's fields into the temp-ref registers.
static intptr_t SSZ_STDCALL TmplLibMake(PluginUtil*, int64_t v)
{
	Reference* r = (Reference*)sszrefnewfunc(sizeof(Reference));
	if(r != nullptr){
		r->init();
		r->refnew(1, sizeof(int64_t));
		if(!r->null()) memcpy(r->atpos(), &v, sizeof(int64_t));
	}
	return (intptr_t)r;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool tmpl_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "min",  "_t (_t, _t)",       (void*)TmplLibMin  },
		{ "add",  "_t (_t, _t)",       (void*)TmplLibAdd  },
		{ "inc",  "void (_t=)",        (void*)TmplLibInc  },
		{ "fill", "void (^_t=, _t)",   (void*)TmplLibFill },
		{ "make", "^_t (_t)",          (void*)TmplLibMake },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "tmpl";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
