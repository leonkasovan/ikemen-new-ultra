// ============================================================================
// ssz.cpp — native (C++) implementation of the SSZ `ssz` library's module
// functions and `&Compiler` struct methods.
//
// Consumed from ssz_script/lib/ssz.ssz, which delegates the module functions
// (memMarkBefore/memMarkAfter/run) and the struct methods here:
//
//     lib sz = <ssz>;
//     ...
//     public bool run(^/char file) { ret .sz.run(file); }
//
// The module is resolved by the native-lib registry (main/ssz/native_lib.hpp).
// The ssz plugin IS the SSZ runtime itself: its extern "C" functions in
// main/ssz/ssz.cpp already have exactly the plugin ABI these signatures
// require, so this library re-exports them directly (a small wrapper handles
// &Compiler.delete's zeroing).  The static `ssz` plugin and this native lib
// coexist.
// ============================================================================

#include <cstdint>

#include "sszdef.h"
#include "native_lib.hpp"
#include "arrayandref.hpp"   // full Reference (12 bytes on x86) so the
                             // __stdcall decorations match ssz.o exactly
#include "pluginutil.hpp"    // full PluginUtil

struct PluginUtil;
struct CompilerState;

// Re-exported extern "C" definitions in main/ssz/ssz.cpp (plugin ABI).
extern "C" void SSZ_STDCALL MemMarkBefore(PluginUtil*, Reference tag);
extern "C" void SSZ_STDCALL MemMarkAfter(PluginUtil*, Reference tag);
extern "C" bool SSZ_STDCALL Run(PluginUtil*, Reference r);
extern "C" CompilerState* SSZ_STDCALL NewCompiler(PluginUtil*);
extern "C" void SSZ_STDCALL DeleteCompiler(PluginUtil*, CompilerState* cs);
extern "C" void SSZ_STDCALL CompilerCompile(
	PluginUtil*, Reference* err, Reference file, CompilerState* cs);
extern "C" void SSZ_STDCALL CompilerCompileString(
	PluginUtil*, Reference* err, Reference dir, Reference code, CompilerState* cs);
extern "C" bool SSZ_STDCALL CompilerRun(PluginUtil*, CompilerState* cs);

// ---------------------------------------------------------------------------
// Registration — the re-exported functions are the plugin ABI already.
// ---------------------------------------------------------------------------

// SSZ: &Compiler.delete() — delete the handle and zero the field.
// Args arrive reversed: (pu, ptr*).
static void SSZ_STDCALL SszLibDeleteCompiler(PluginUtil* pu, intptr_t* ptr)
{
	if(*ptr != 0){
		DeleteCompiler(pu, (CompilerState*)*ptr);
		*ptr = 0;
	}
}

extern "C" bool ssz_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		// SSZ module functions
		{ "memMarkBefore",     "void (^/char)",           (void*)MemMarkBefore       },
		{ "memMarkAfter",      "void (^/char)",           (void*)MemMarkAfter        },
		{ "run",               "bool (^/char)",           (void*)Run                 },
		// SSZ &Compiler struct methods
		{ "newCompiler",       "index ()",                (void*)NewCompiler         },
		{ "deleteCompiler",    "void (index=)",           (void*)SszLibDeleteCompiler },
		{ "compilerCompile",   "void (index, ^/char, ^/char=)", (void*)CompilerCompile },
		{ "compilerCompileString",
			"void (index, ^/char, ^/char, ^/char=)",     (void*)CompilerCompileString },
		{ "compilerRun",       "bool (index)",            (void*)CompilerRun         },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "ssz";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
