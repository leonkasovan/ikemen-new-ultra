
#include <windows.h>
#include <locale.h>
#include <process.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <vector>

#include "lua.hpp"


#include "sszdef.h"
#include "typeid.h"
#include "arrayandref.hpp"
#include "pluginutil.hpp"


const char SszRefMetaName[] = "SszRef";

SSZCALLBACK g_callback;
intptr_t g_refDestroy, g_refCopy;
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

extern "C" void SSZ_STDCALL LuaInit(PluginUtil* pu, intptr_t refcopy, intptr_t refdest)
{
	g_callback = pu->psf->callback;
	g_handle = pu->handle;
	g_refDestroy = refdest;
	g_refCopy = refcopy;
}

extern "C" lua_State* SSZ_STDCALL NewState(PluginUtil* pu)
{
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
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
// The observed crash is in Lua FFI cdata finalization during lua_close():
//   lua_close -> callallpendingfinalizers -> cdata_gc -> to_cdata
// where the cdata pointer matched an SSZ callback argument buffer. Skipping the
// faulting instruction with VEH is unsafe (variable-length x86 instructions) and
// MinGW GCC cannot use MSVC __try/__except. For now we quarantine states by
// clearing their stack and intentionally avoid lua_close(); this trades a bounded
// leak for a stable demo while the FFI/SSZ ownership bug is fixed.
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

	// Call ffi.cleanup() to invalidate all cdata objects before closing (Option C)
	// This prevents access to freed SSZ memory during finalization
	lua_getglobal(L, "ffi");
	if (lua_istable(L, -1)) {
		lua_getfield(L, -1, "cleanup");
		if (lua_isfunction(L, -1)) {
			lua_pcall(L, 0, 0, 0);  /* Call ffi.cleanup() safely */
		} else {
			lua_pop(L, 1);  /* Not a function, pop it */
		}
	}
	lua_pop(L, 1);  /* Pop ffi table */

	// Remove ordinary Lua stack references. Do NOT force a full GC here: a full
	// GC can run the same corrupted FFI cdata finalizers that crash lua_close().
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

extern "C" void SSZ_STDCALL Close(PluginUtil* pu, lua_State* L)
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
	// LOG_INFO("Lua", "Close(): quarantined lua_State=%p; lua_close intentionally skipped", (void*)L);
	lua_gc(L, LUA_GCCOLLECT, 0);
	lua_close(L);
	LuaProcessDeferredClose();
}

extern "C" bool SSZ_STDCALL RunFile(PluginUtil* pu, Reference filename, lua_State* L)
{
	return
		!luaL_loadfile(L, pu->refToAstr(CP_UTF8, filename).c_str())
		&& !lua_pcall(L, 0, 0, 0);
}

extern "C" bool SSZ_STDCALL RunString(PluginUtil* pu, Reference s, lua_State* L)
{
	return
		!luaL_loadstring(L, pu->refToAstr(CP_UTF8, s).c_str())
		&& !lua_pcall(L, 0, 0, 0);
}

extern "C" int32_t SSZ_STDCALL GetTop(PluginUtil* pu, lua_State* L)
{
	return lua_gettop(L);
}

extern "C" void SSZ_STDCALL GetGlobal(PluginUtil* pu, Reference var, lua_State* L)
{
	lua_getglobal(L, pu->refToAstr(CP_UTF8, var).c_str());
}

extern "C" void SSZ_STDCALL Register(PluginUtil* pu, intptr_t func, Reference var, lua_State* L)
{
	lua_pushinteger(L, func);
	lua_pushcclosure(L, funcCall, 1);
	lua_setglobal(L, pu->refToAstr(CP_UTF8, var).c_str());
}

extern "C" bool SSZ_STDCALL Pcall(PluginUtil* pu, int32_t nresults, int32_t nargs, lua_State* L)
{
	return !lua_pcall(L, nargs, nresults, 0);
}

extern "C" void SSZ_STDCALL Pop(PluginUtil* pu, int32_t n, lua_State* L)
{
	lua_pop(L, n);
}

extern "C" void SSZ_STDCALL PushNumber(PluginUtil* pu, double n, lua_State* L)
{
	lua_pushnumber(L, n);
}

extern "C" bool SSZ_STDCALL IsNumber(PluginUtil* pu, int32_t idx, lua_State* L)
{
	return lua_isnumber(L, idx) != 0;
}

extern "C" double SSZ_STDCALL ToNumber(PluginUtil* pu, int32_t idx, lua_State* L)
{
	return lua_tonumber(L, idx);
}

extern "C" void SSZ_STDCALL PushBoolean(PluginUtil* pu, bool b, lua_State* L)
{
	lua_pushboolean(L, b);
}

extern "C" bool SSZ_STDCALL IsBoolean(PluginUtil* pu, int32_t idx, lua_State* L)
{
	return lua_isboolean(L, idx) != 0;
}

extern "C" bool SSZ_STDCALL ToBoolean(PluginUtil* pu, int32_t idx, lua_State* L)
{
	return lua_toboolean(L, idx) != 0;
}

extern "C" void SSZ_STDCALL PushString(PluginUtil* pu, Reference s, lua_State* L)
{
	lua_pushstring(L, pu->refToAstr(CP_UTF8, s).c_str());
}

extern "C" bool SSZ_STDCALL IsString(PluginUtil* pu, int32_t idx, lua_State* L)
{
	return lua_isstring(L, idx) != 0;
}

extern "C" void SSZ_STDCALL ToString(PluginUtil* pu, int32_t idx, Reference* s, lua_State* L)
{
	pu->setSSZFunc();
	pu->astrToRef(CP_UTF8, *s, lua_tostring(L, idx));
}

extern "C" void SSZ_STDCALL PushRef(PluginUtil* pu, DynamicRef* userdata, lua_State* L)
{
	auto ud = (DynamicRef*)lua_newuserdata(L, sizeof(DynamicRef));
	*ud = *userdata;
	userdata->init();
	luaL_getmetatable(L, SszRefMetaName);
	lua_setmetatable(L, -2);
}

extern "C" void SSZ_STDCALL ToRef(PluginUtil* pu, int32_t idx, DynamicRef* userdata, lua_State* L)
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

