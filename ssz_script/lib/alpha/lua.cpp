// ============================================================================
// lua.cpp — native (C++) implementation of the SSZ `lua` library.
//
// Converted from ssz_script/lib/alpha/lua.ssz.  The SSZ file keeps the
// stateful `&State` struct (data container + auto new/delete) and delegates
// its method bodies here:
//
//     lib lua = <lua>;
//     public &State
//     {
//       index ptr = 0;
//       new()        { .lua.newState(`ptr=); }
//       public bool runFile(^/char str) { ret .lua.runFile(`ptr, str); }
//       ...
//     }
//
// Every function is the SAME C++ implementation the static <lua> plugin
// already exports (main/ssz/bridge.cpp wrappers over main/lua/lua.cpp,
// registered via lua_plugin.hpp) — the native registry and the static
// plugin registry are separate, so the same function pointers are
// reachable as both `plugin ... = <lua>;` and `lib lua = <lua>;`.
//
// The `ref` params map to DynamicRef* (out-params) / DynamicRef (values);
// `func` params map to intptr_t delegate slots (see Register below).
//
// ABI: plugin convention — args arrive reversed; `type=` out-params arrive
// as pointers; `^/x` strings arrive as Reference.
// ============================================================================

#include "sszdef.h"
#include "arrayandref.hpp"   // full Reference/DynamicRef definitions
#include "native_lib.hpp"

// Declares every function below with the plugin ABI (extern "C").
#include "lua/lua_plugin.hpp"

// ---------------------------------------------------------------------------
// Registration — re-exports the static plugin's function pointers under the
// native library name `lua`, with the SSZ signature strings from the
// original plugin declarations in alpha/lua.ssz.
// ---------------------------------------------------------------------------

extern "C" bool lua_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "newState",    "index ()",                    (void*)NewState    },
		{ "close",       "void (index)",                (void*)Close       },
		{ "runFile",     "bool (index, ^/char)",        (void*)RunFile     },
		{ "runString",   "bool (index, ^/char)",        (void*)RunString   },
		{ "getTop",      "int (index)",                 (void*)GetTop      },
		{ "getGlobal",   "void (index, ^/char)",        (void*)GetGlobal   },
		{ "pcall",       "bool (index, int, int)",      (void*)Pcall       },
		{ "pop",         "void (index, int)",           (void*)Pop         },
		{ "pushNumber",  "void (index, double)",        (void*)PushNumber  },
		{ "isNumber",    "bool (index, int)",           (void*)IsNumber    },
		{ "toNumber",    "double (index, int)",         (void*)ToNumber    },
		{ "pushBoolean", "void (index, bool)",          (void*)PushBoolean },
		{ "isBoolean",   "bool (index, int)",           (void*)IsBoolean   },
		{ "toBoolean",   "bool (index, int)",           (void*)ToBoolean   },
		{ "pushString",  "void (index, ^/char)",        (void*)PushString  },
		{ "isString",    "bool (index, int)",           (void*)IsString    },
		{ "toString",    "void (index, ^char=, int)",   (void*)ToString    },
		{ "pushRef",     "void (index, ref=)",          (void*)PushRef     },
		{ "toRef",       "void (index, ref=, int)",     (void*)ToRef       },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "lua";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
