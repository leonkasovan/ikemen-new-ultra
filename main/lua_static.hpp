#pragma once
//
// lua_static.hpp
//
// Statically register every function exported by lua.cpp
// so that the SSZ runtime resolves them without loading lua.dll.
//
// HOW TO USE
// ----------
// 1.  #include this header in the translation unit that owns main().
// 2.  Call  lua_static_register()  BEFORE the SSZ compiler runs.
// 3.  The SSZ scripts continue to use:
//         plugin void Close(:index:) = "dll/lua.dll";
//     but NO lua.dll file is needed — the function pointer is
//     resolved from the static registry.

#include "static_plugin_registry.hpp"

#if IKEMEN_NATIVE_LUA_LIB

// -----------------------------------------------------------------------
// Forward-declare types needed in function signatures.
// (sszdef.h, typeid.h, and arrayandref.hpp are assumed to be
//  already included by the caller before this header is included.)
// -----------------------------------------------------------------------

struct PluginUtil;   // forward
struct lua_State;    // forward (from Lua headers)
struct Reference;    // forward (from arrayandref.hpp)
struct DynamicRef;   // forward (from arrayandref.hpp)

// Main Lua state accessor — used by native modules (script_service, etc.)
// to register Lua-callable callbacks. Set on first call to NewState().
lua_State* get_main_lua_state();
void set_main_lua_state(lua_State* L);

extern "C"
{
	void       SSZ_STDCALL LuaInit(PluginUtil*, intptr_t, intptr_t);
	lua_State* SSZ_STDCALL NewState(PluginUtil*);
	void       SSZ_STDCALL Close(PluginUtil*, lua_State*);
	bool       SSZ_STDCALL RunFile(PluginUtil*, Reference, lua_State*);
	bool       SSZ_STDCALL RunString(PluginUtil*, Reference, lua_State*);
	int32_t    SSZ_STDCALL GetTop(PluginUtil*, lua_State*);
	void       SSZ_STDCALL GetGlobal(PluginUtil*, Reference, lua_State*);
	void       SSZ_STDCALL Register(PluginUtil*, intptr_t, Reference, lua_State*);
	bool       SSZ_STDCALL Pcall(PluginUtil*, int32_t, int32_t, lua_State*);
	void       SSZ_STDCALL Pop(PluginUtil*, int32_t, lua_State*);
	void       SSZ_STDCALL PushNumber(PluginUtil*, double, lua_State*);
	bool       SSZ_STDCALL IsNumber(PluginUtil*, int32_t, lua_State*);
	double     SSZ_STDCALL ToNumber(PluginUtil*, int32_t, lua_State*);
	void       SSZ_STDCALL PushBoolean(PluginUtil*, bool, lua_State*);
	bool       SSZ_STDCALL IsBoolean(PluginUtil*, int32_t, lua_State*);
	bool       SSZ_STDCALL ToBoolean(PluginUtil*, int32_t, lua_State*);
	void       SSZ_STDCALL PushString(PluginUtil*, Reference, lua_State*);
	bool       SSZ_STDCALL IsString(PluginUtil*, int32_t, lua_State*);
	void       SSZ_STDCALL ToString(PluginUtil*, int32_t, Reference*, lua_State*);
	void       SSZ_STDCALL PushRef(PluginUtil*, DynamicRef*, lua_State*);
	void       SSZ_STDCALL ToRef(PluginUtil*, int32_t, DynamicRef*, lua_State*);
}

// -----------------------------------------------------------------------
// Build the mapping table and register it.
// -----------------------------------------------------------------------

/// Call once before the SSZ compiler starts.
/// Returns true on success.
inline bool lua_static_register()
{
	static const SSZ_FunctionEntry lua_mapping[] =
	{
		{ "Init",        (void*)LuaInit	    },
		{ "NewState",    (void*)NewState    },
		{ "Close",       (void*)Close       },
		{ "RunFile",     (void*)RunFile     },
		{ "RunString",   (void*)RunString   },
		{ "GetTop",      (void*)GetTop      },
		{ "GetGlobal",   (void*)GetGlobal   },
		{ "Register",    (void*)Register    },
		{ "Pcall",       (void*)Pcall       },
		{ "Pop",         (void*)Pop         },
		{ "PushNumber",  (void*)PushNumber  },
		{ "IsNumber",    (void*)IsNumber    },
		{ "ToNumber",    (void*)ToNumber    },
		{ "PushBoolean", (void*)PushBoolean },
		{ "IsBoolean",   (void*)IsBoolean   },
		{ "ToBoolean",   (void*)ToBoolean   },
		{ "PushString",  (void*)PushString  },
		{ "IsString",    (void*)IsString    },
		{ "ToString",    (void*)ToString    },
		{ "PushRef",     (void*)PushRef     },
		{ "ToRef",       (void*)ToRef       },
	};

	return SSZ_RegisterFunction(
		"lua",
		lua_mapping,
		sizeof(lua_mapping) / sizeof(lua_mapping[0]));
}

#else
static inline bool lua_static_register()
{
	// IKEMEN_NATIVE_LUA_LIB=0 — SSZ lua.ssz script used instead.
	return true;
}
#endif

