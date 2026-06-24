#pragma once
//
// mesdialog_static.hpp
//
// Statically registers every function exported by mesdialog.cpp
// so that the SSZ runtime resolves them without loading mesdialog.dll.
//
// HOW TO USE
// ----------
// 1.  #include this header in the translation unit that owns main()
//     (or wherever the SSZ compiler is initialised).
//
// 2.  Call  mesdialog_static_register()  BEFORE the SSZ compiler runs.
//
//     #include "mesdialog_static.hpp"
//
//     int main() {
//         if (!mesdialog_static_register()) {
//             printf("Failed to register mesdialog functions.\n");
//             return 1;
//         }
//         // ... start SSZ compiler / runtime ...
//     }
//
// 3.  The SSZ scripts continue to use:
//         plugin void Close(:index:) = "dll/mesdialog.dll";
//     but NO mesdialog.dll file is needed — the function pointer is
//     resolved from the static registry.
//
// PREREQUISITES
// -------------
//   - Link mesdialog.cpp (and its dependencies) into the same executable.
//   - #include "static_plugin_registry.hpp" (pulled in automatically).
//

#include "static_plugin_registry.hpp"

// -----------------------------------------------------------------------
// Forward-declare types needed in function signatures.
// (sszdef.h, typeid.h, and arrayandref.hpp are assumed to be
//  already included by the caller before this header is included.)
// -----------------------------------------------------------------------

struct PluginUtil;  // forward (from pluginutil.hpp)
struct Reference;   // forward (from arrayandref.hpp)

extern "C"
{
	bool       SSZ_STDCALL YesNo             (PluginUtil*, Reference);
	void       SSZ_STDCALL VeryUnsafeCopy    (PluginUtil*, intptr_t, void*, void*);
	bool       SSZ_STDCALL GetClipboardStr   (PluginUtil*, Reference*);
	intptr_t   SSZ_STDCALL TazyuuCheck       (PluginUtil*, Reference);
	void       SSZ_STDCALL CloseTazyuuHandle (PluginUtil*, intptr_t);
	void       SSZ_STDCALL GetInifileString  (PluginUtil*, Reference*, Reference, Reference, Reference, Reference);
	int32_t    SSZ_STDCALL GetInifileInt     (PluginUtil*, int32_t, Reference, Reference, Reference);
	bool       SSZ_STDCALL WriteInifileString(PluginUtil*, Reference, Reference, Reference, Reference);
	bool       SSZ_STDCALL UnCompress        (PluginUtil*, Reference, Reference*);
	void       SSZ_STDCALL UbytesToStr       (PluginUtil*, Reference, Reference*, unsigned int);
	void       SSZ_STDCALL StrToUbytes       (PluginUtil*, Reference, Reference*, unsigned int);
	void       SSZ_STDCALL AsciiToLocal      (PluginUtil*, Reference, Reference*);
	void       SSZ_STDCALL SetSharedString   (PluginUtil*, Reference);
	void       SSZ_STDCALL GetSharedString   (PluginUtil*, Reference*);
	void       SSZ_STDCALL InputStr          (PluginUtil*, Reference*, Reference);
}

// -----------------------------------------------------------------------
// Build the mapping table and register it.
// -----------------------------------------------------------------------

/// Call once before the SSZ compiler starts.
/// Returns true on success.
inline bool mesdialog_static_register()
{
	static const SSZ_FunctionEntry mesdialog_mapping[] =
	{
		{ "YesNo",              (void*)YesNo              },
		{ "VeryUnsafeCopy",     (void*)VeryUnsafeCopy     },
		{ "GetClipboardStr",    (void*)GetClipboardStr    },
		{ "TazyuuCheck",        (void*)TazyuuCheck        },
		{ "CloseTazyuuHandle",  (void*)CloseTazyuuHandle  },
		{ "GetInifileString",   (void*)GetInifileString   },
		{ "GetInifileInt",      (void*)GetInifileInt      },
		{ "WriteInifileString", (void*)WriteInifileString },
		{ "UnCompress",         (void*)UnCompress         },
		{ "UbytesToStr",        (void*)UbytesToStr        },
		{ "StrToUbytes",        (void*)StrToUbytes        },
		{ "AsciiToLocal",       (void*)AsciiToLocal       },
		{ "SetSharedString",    (void*)SetSharedString    },
		{ "GetSharedString",    (void*)GetSharedString    },
		{ "InputStr",           (void*)InputStr           },
	};

	return SSZ_RegisterFunction(
		"mesdialog",
		mesdialog_mapping,
		sizeof(mesdialog_mapping) / sizeof(mesdialog_mapping[0]));
}
