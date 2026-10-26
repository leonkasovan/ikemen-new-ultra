#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <vector>

#include "lua.hpp"
extern "C" {
#include "lfs.h"
#include "ffi/ffi.h"
#include "lpeg-1.1.0/lptypes.h"
}

#include "sszdef.h"
#include "typeid.h"
#include "arrayandref.hpp"

// SSZCALLBACK typedef (replaces pluginutil.hpp include)
typedef void* (SSZ_STDCALL* SSZCALLBACK)(void*, intptr_t, void*, intptr_t, intptr_t);


const char SszRefMetaName[] = "SszRef";

SSZCALLBACK g_callback = nullptr;
intptr_t g_refDestroy = 0, g_refCopy = 0;
void* g_handle = nullptr;

static int g_luaCallbackDepth = 0;
static void LuaProcessDeferredClose();

// Unified LOG_DEBUG/LOG_INFO macros are defined in sszdef.h

int refGc(lua_State *L)
{
	auto arg = (DynamicRef*)lua_touserdata(L, 1);
	++g_luaCallbackDepth;
	g_callback(g_handle, g_refDestroy, &arg, sizeof(arg), 0);
	--g_luaCallbackDepth;
	LuaProcessDeferredClose();
	return 0;
}

int funcCall(lua_State* L)
{
	intptr_t func = lua_tointeger(L, lua_upvalueindex(1));
	int ret = 0;
	#pragma pack(push, 1)
	struct{
		int32_t* ret;
		lua_State** pL;
	} arg = {&ret, &L};
	#pragma pack(pop)
	++g_luaCallbackDepth;
	g_callback(g_handle, func, &arg, sizeof(arg), 0);
	--g_luaCallbackDepth;
	if(ret < 0) return luaL_error(L, "%s", lua_tostring(L, -1));
	LuaProcessDeferredClose();
	return ret;
}

void SSZ_STDCALL LuaInit(intptr_t refcopy, intptr_t refdest, SSZCALLBACK callback, void* handle)
{
	g_callback = callback;
	g_handle = handle;
	g_refDestroy = refdest;
	g_refCopy = refcopy;
}

lua_State* SSZ_STDCALL NewState()
{
	lua_State* L = luaL_newstate();
	if (!L) return nullptr;
	
	luaL_requiref(L, "_G", luaopen_base, 1); lua_pop(L, 1);
	luaL_requiref(L, LUA_LOADLIBNAME, luaopen_package, 1); lua_pop(L, 1);
	luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1); lua_pop(L, 1);
	luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1); lua_pop(L, 1);
	luaL_requiref(L, LUA_IOLIBNAME, luaopen_io, 1); lua_pop(L, 1);
	luaL_requiref(L, LUA_OSLIBNAME, luaopen_os, 1); lua_pop(L, 1);
	luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1); lua_pop(L, 1);
	luaL_requiref(L, LUA_BITLIBNAME, luaopen_bit32, 1); lua_pop(L, 1);
	luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1); lua_pop(L, 1);
	luaL_requiref(L, LUA_DBLIBNAME, luaopen_debug, 1); lua_pop(L, 1);
	// NOTE: luaopen_ffi is intentionally NOT loaded. Nothing in the game
	// scripts uses FFI (the only reference in common.lua is commented out),
	// and the ffi library's DynASM-based init (compile_globals) emits raw
	// machine code that faults under ASLR-relocated module layouts
	// (0xc0000005 in "unknown" module — see Event Log, e.g. 2026-08-21).
	// Skipping it removes both that startup segfault and the lua_close()
	// cdata-finalizer crash noted below.
	//
	// DECISION (supersedes the lazy-init approach on branch `optimized`):
	// commit c6cd1e9 deferred compile_globals to the first FFI code-gen call.
	// That still left the cdata finalizer crash reachable on lua_close(), so
	// FFI is dropped entirely instead. external/lua-5.2.4/ffi/ is left at
	// upstream eager-init state; luaL_openlibs() is never called (libs are
	// opened individually here), so the linit.c entry is inert. Do not
	// re-add require('ffi') to scripts without revisiting this.
	luaL_requiref(L, LUA_LFSNAME, luaopen_lfs, 1); lua_pop(L, 1);
	luaL_requiref(L, LUA_LPEGNAME, luaopen_lpeg, 1); lua_pop(L, 1);
	
	luaL_newmetatable(L, SszRefMetaName);
	lua_pushstring(L, "__gc");
	lua_pushcfunction(L, refGc);
	lua_settable(L, -3);
	return L;
}


// -----------------------------------------------------------------------------
// Safe/deferred Lua teardown
// -----------------------------------------------------------------------------
// Do not call lua_close() while returning through SSZ callback/JIT frames.
// The original crash was in Lua FFI cdata finalization during lua_close():
//   lua_close -> callallpendingfinalizers -> cdata_gc -> to_cdata
// where the cdata pointer matched an SSZ callback argument buffer. FFI is no
// longer loaded (see NewState above), which removes the cdata-finalizer path,
// but the deferred-close machinery is kept as a defensive guard for SSZ Ref
// userdata teardown. Skipping the faulting instruction with VEH is unsafe
// (variable-length x86 instructions) and MinGW GCC cannot use MSVC
// __try/__except. For now we quarantine states by clearing their stack and
// intentionally avoid lua_close(); this trades a bounded leak for a stable demo.
static std::vector<lua_State*> g_deferredLuaClose;

static bool LuaCloseAlreadyDeferred(lua_State* L)
{
	for (size_t i = 0; i < g_deferredLuaClose.size(); ++i)
	{
		if (g_deferredLuaClose[i] == L) return true;
	}
	return false;
}

static void LuaSafeQuarantine(lua_State* L)
{
	if (!L) return;

	// NOTE: there used to be an ffi.cleanup() call here to invalidate cdata
	// objects before closing. FFI is no longer loaded (see NewState), so the
	// global `ffi` is always nil and the call was a no-op; removed.

	// Remove ordinary Lua stack references. Do NOT force a full GC here: a full
	// GC can run corrupted finalizers that crash lua_close().
	lua_settop(L, 0);
}

static void LuaProcessDeferredClose()
{
	if (g_luaCallbackDepth != 0) return;

	for (size_t i = 0; i < g_deferredLuaClose.size(); ++i)
	{
		LuaSafeQuarantine(g_deferredLuaClose[i]);
	}
	g_deferredLuaClose.clear();
}

void SSZ_STDCALL Close(lua_State* L)
{
	if (!L) return;

	if (g_luaCallbackDepth != 0)
	{
		if (!LuaCloseAlreadyDeferred(L))
		{
			g_deferredLuaClose.push_back(L);
		}
		return;
	}

	LuaSafeQuarantine(L);
	// LOG_INFO(\"Lua\", \"Close(): quarantined lua_State=%p; lua_close intentionally skipped\", (void*)L);
	lua_gc(L, LUA_GCCOLLECT, 0);
	lua_close(L);
	LuaProcessDeferredClose();
}

bool SSZ_STDCALL RunFile(const std::string& filename, lua_State* L)
{
	return
		!luaL_loadfile(L, filename.c_str())
		&& !lua_pcall(L, 0, 0, 0);
}

bool SSZ_STDCALL RunString(const std::string& s, lua_State* L)
{
	return
		!luaL_loadstring(L, s.c_str())
		&& !lua_pcall(L, 0, 0, 0);
}

int32_t SSZ_STDCALL GetTop(lua_State* L)
{
	return lua_gettop(L);
}

void SSZ_STDCALL GetGlobal(const std::string& var, lua_State* L)
{
	lua_getglobal(L, var.c_str());
}

void SSZ_STDCALL Register(intptr_t func, const std::string& var, lua_State* L)
{
	lua_pushinteger(L, func);
	lua_pushcclosure(L, funcCall, 1);
	lua_setglobal(L, var.c_str());
}

bool SSZ_STDCALL Pcall(int32_t nresults, int32_t nargs, lua_State* L)
{
	return !lua_pcall(L, nargs, nresults, 0);
}

void SSZ_STDCALL Pop(int32_t n, lua_State* L)
{
	lua_pop(L, n);
}

void SSZ_STDCALL PushNumber(double n, lua_State* L)
{
	lua_pushnumber(L, n);
}

bool SSZ_STDCALL IsNumber(int32_t idx, lua_State* L)
{
	return lua_isnumber(L, idx) != 0;
}

double SSZ_STDCALL ToNumber(int32_t idx, lua_State* L)
{
	return lua_tonumber(L, idx);
}

void SSZ_STDCALL PushBoolean(bool b, lua_State* L)
{
	lua_pushboolean(L, b);
}

bool SSZ_STDCALL IsBoolean(int32_t idx, lua_State* L)
{
	return lua_isboolean(L, idx) != 0;
}

bool SSZ_STDCALL ToBoolean(int32_t idx, lua_State* L)
{
	return lua_toboolean(L, idx) != 0;
}

void SSZ_STDCALL PushString(const std::string& s, lua_State* L)
{
	lua_pushstring(L, s.c_str());
}

bool SSZ_STDCALL IsString(int32_t idx, lua_State* L)
{
	return lua_isstring(L, idx) != 0;
}

void SSZ_STDCALL ToString(int32_t idx, lua_State* L, std::string& output)
{
	const char* str = lua_tostring(L, idx);
	if (str) output = str;
}

void SSZ_STDCALL PushRef(DynamicRef* userdata, lua_State* L)
{
	auto ud = (DynamicRef*)lua_newuserdata(L, sizeof(DynamicRef));
	*ud = *userdata;
	userdata->init();
	luaL_getmetatable(L, SszRefMetaName);
	lua_setmetatable(L, -2);
}

void SSZ_STDCALL ToRef(int32_t idx, DynamicRef* userdata, lua_State* L)
{
	auto ud = (DynamicRef*)lua_touserdata(L, idx);
	if(!ud) return;
	#pragma pack(push, 1)
	struct{
		DynamicRef d;
		DynamicRef* ud;
	} arg = {*ud, userdata};
	#pragma pack(pop)
	g_callback(g_handle, g_refCopy, &arg, sizeof(arg), 0);
}
