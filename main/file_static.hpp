#pragma once
//
// file_static.hpp
//
// Statically register every function exported by file.cpp
// so that the SSZ runtime resolves them without loading file.dll.
//
// NOTE: file.cpp's "Close" was renamed to "FileClose" in the C symbol
//       to avoid a linker clash with lua.cpp's "Close".
//       The SSZ mapping still maps "Close" -> FileClose.
//

#include "static_plugin_registry.hpp"

#include <cstdio>    // for FILE*

struct PluginUtil;
struct Reference;

extern "C"
{
	intptr_t SSZ_STDCALL Open(PluginUtil*, Reference, Reference);
	void     SSZ_STDCALL FileClose(PluginUtil*, FILE*);
	bool     SSZ_STDCALL Read(PluginUtil*, intptr_t, void*, FILE*);
	intptr_t SSZ_STDCALL ReadAry(PluginUtil*, intptr_t, Reference, FILE*);
	bool     SSZ_STDCALL Write(PluginUtil*, intptr_t, void*, FILE*);
	intptr_t SSZ_STDCALL WriteAry(PluginUtil*, intptr_t, Reference, FILE*);
	bool     SSZ_STDCALL Seek(PluginUtil*, int32_t, int64_t, FILE*);
	void     SSZ_STDCALL LoadAsciiText(PluginUtil*, Reference*, Reference);
	bool     SSZ_STDCALL SaveAsciiText(PluginUtil*, Reference, Reference);
	bool     SSZ_STDCALL Delete(PluginUtil*, Reference);
	bool     SSZ_STDCALL Move(PluginUtil*, Reference, Reference);
	bool     SSZ_STDCALL Copy(PluginUtil*, bool, Reference, Reference);
	void     SSZ_STDCALL Find(PluginUtil*, Reference*, Reference);
	void     SSZ_STDCALL FindDir(PluginUtil*, Reference*, Reference);
	bool     SSZ_STDCALL CreateDir(PluginUtil*, Reference);
	bool     SSZ_STDCALL RemoveDir(PluginUtil*, Reference);
	bool     SSZ_STDCALL SetCurrentDir(PluginUtil*, Reference);
	void     SSZ_STDCALL GetCurrentDir(PluginUtil*, Reference*);
}

inline bool file_static_register()
{
	static const SSZ_FunctionEntry file_mapping[] =
	{
		{ "Open",           (void*)Open           },
		{ "Close",          (void*)FileClose      },
		{ "Read",           (void*)Read           },
		{ "ReadAry",        (void*)ReadAry        },
		{ "Write",          (void*)Write          },
		{ "WriteAry",       (void*)WriteAry       },
		{ "Seek",           (void*)Seek           },
		{ "LoadAsciiText",  (void*)LoadAsciiText  },
		{ "SaveAsciiText",  (void*)SaveAsciiText  },
		{ "Delete",         (void*)Delete         },
		{ "Move",           (void*)Move           },
		{ "Copy",           (void*)Copy           },
		{ "Find",           (void*)Find           },
		{ "FindDir",        (void*)FindDir        },
		{ "CreateDir",      (void*)CreateDir      },
		{ "RemoveDir",      (void*)RemoveDir      },
		{ "SetCurrentDir",  (void*)SetCurrentDir  },
		{ "GetCurrentDir",  (void*)GetCurrentDir  },
	};

	return SSZ_RegisterFunction(
		"file",
		file_mapping,
		sizeof(file_mapping) / sizeof(file_mapping[0]));
}
