// ============================================================================
// mesdialog.cpp — native (C++) implementation of the SSZ `mesdialog` library.
//
// Converted from ssz_script/lib/alpha/mesdialog.ssz.  The SSZ file keeps the
// `|CodePage` enum (type vocabulary) and the template-bound `veryUnsafeCopy`
// (per-instantiation `dst_t`/`src_t` types, resolved via the static plugin
// registry inside the template body); the module-level functions delegate
// here:
//
//     |CodePage { ... }            // must be declared BEFORE the import
//     lib md = <mesdialog>;
//     public void SetSharedString(^/char str) { .md.setSharedString(str); }
//
// Every function is the SAME C++ implementation the static <mesdialog>
// plugin already exports (main/ssz/bridge.cpp wrappers over
// main/mesdialog/mesdialog.cpp, registered via mesdialog_plugin.hpp) — the
// native registry and the static plugin registry are separate, so the same
// function pointers are reachable as both `plugin ... = <mesdialog>;` and
// `lib md = <mesdialog>;`.
//
// The signature strings are the SSZ views of the plugin declarations they
// replace, including the `|CodePage` enum type — resolved by the native
// registry's type resolver when the module is imported (the importing
// module must already have declared `|CodePage`).
//
// ABI: plugin convention — args arrive reversed; `type=` out-params arrive
// as pointers; `^/x` strings arrive as Reference.
// ============================================================================

#include "sszdef.h"
#include "arrayandref.hpp"   // full Reference definition (mesdialog_plugin.hpp assumes it)
#include "native_lib.hpp"

// Declares every function below with the plugin ABI (extern "C").
#include "mesdialog/mesdialog_plugin.hpp"

// ---------------------------------------------------------------------------
// Registration — re-exports the static plugin's function pointers under the
// native library name `mesdialog`, with the SSZ signature strings from the
// original plugin declarations in alpha/mesdialog.ssz.
// ---------------------------------------------------------------------------

extern "C" bool mesdialog_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "yesNo",              "bool (^/char)",                                  (void*)YesNo              },
		{ "getClipboardStr",    "bool (^char=)",                                  (void*)GetClipboardStr    },
		{ "getInifileString",   "void (^/char, ^/char, ^/char, ^/char, ^char=)",  (void*)GetInifileString   },
		{ "getInifileInt",      "int (^/char, ^/char, ^/char, int)",              (void*)GetInifileInt      },
		{ "writeInifileString", "bool (^/char, ^/char, ^/char, ^/char)",          (void*)WriteInifileString },
		{ "inputStr",           "void (^/char, ^char=)",                          (void*)InputStr           },
		{ "unCompress",         "bool (^ubyte=, ^/ubyte)",                        (void*)UnCompress         },
		{ "ubytesToStr",        "void (|CodePage, ^char=, ^/ubyte)",              (void*)UbytesToStr        },
		{ "strToUbytes",        "void (|CodePage, ^ubyte=, ^/char)",              (void*)StrToUbytes        },
		{ "asciiToLocal",       "void (^char=, ^/char)",                          (void*)AsciiToLocal       },
		{ "setSharedString",    "void (^/char)",                                  (void*)SetSharedString    },
		{ "getSharedString",    "void (^char=)",                                  (void*)GetSharedString    },
		{ "tazyuuCheck",        "index (^/char)",                                 (void*)TazyuuCheck        },
		{ "closeTazyuuHandle",  "void (index)",                                   (void*)CloseTazyuuHandle  },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "mesdialog";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
