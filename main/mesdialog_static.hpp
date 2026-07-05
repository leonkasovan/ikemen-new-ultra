#pragma once
//
// mesdialog_static.hpp
//
// Statically registers every function exported by mesdialog.cpp
// so that the SSZ runtime resolves them without loading mesdialog.dll.
//
// REGISTRATION IS UNCONDITIONAL — bridge functions are always compiled.

#include "static_plugin_registry.hpp"

// Always register — bridge functions are always compiled in bridge.cpp

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


