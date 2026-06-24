#include "system-script.hpp"
#include "system.hpp"
#include "../string.hpp"
#include "sszdef.h"

#include <windows.h>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace ikemen {
namespace {

std::wstring utf8ToWide(const char* utf8)
{
	if (!utf8) return L"";
	int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
	if (len <= 0) return L"";
	std::wstring w(len - 1, 0);
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &w[0], len);
	return w;
}

std::string wideToUtf8(const std::wstring& w)
{
	if (w.empty()) return "";
	int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (len <= 0) return "";
	std::string a(len - 1, 0);
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &a[0], len, nullptr, nullptr);
	return a;
}

int l_addChar(lua_State* L)
{
	const char* str = luaL_checkstring(L, 1);
	LOG_DEBUG("SCRIPT", "addChar called: '%s'", str);
	auto wstr = utf8ToWide(str);
	auto lines = splitLines(wstr);
	for (auto& line : lines) {
		auto t = trim(line);
		if (!t.empty()) {
			LOG_DEBUG("SCRIPT", "addChar -> syst.selinf.sel.addChar() line='%ls'", t.c_str());
			syst.selinf.sel.addChar(t);
			LOG_DEBUG("SCRIPT", "addChar -> sys.selinf.sel.addChar() OK, charlist size=%zu", syst.selinf.sel.charlist.size());
		}
	}
	return 0;
}

int l_getCharName(lua_State* L)
{
	int n = (int)luaL_checkinteger(L, 1);
	auto ch = syst.selinf.sel.getChar(n);
	std::string utf8 = wideToUtf8(ch.name);
	lua_pushstring(L, utf8.c_str());
	return 1;
}

int l_getCharFileName(lua_State* L)
{
	int n = (int)luaL_checkinteger(L, 1);
	auto ch = syst.selinf.sel.getChar(n);
	std::string utf8 = wideToUtf8(ch.def);
	lua_pushstring(L, utf8.c_str());
	return 1;
}

int l_loadLifebar(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	auto wpath = utf8ToWide(path);
	syst.fig.new_(1);
	auto err = syst.fig.load(wpath);
	if (!err.empty()) {
		std::string utf8err = wideToUtf8(err);
		lua_pushstring(L, utf8err.c_str());
		return 1;
	}
	return 0;
}

int l_setLifebarDisplay(lua_State* L)
{
	bool ld = lua_toboolean(L, 1) != 0;
	com.lifebarDisplay = ld ? 1 : 0;
	return 0;
}

} // namespace

void registerSystemScriptCallbacks(lua_State* L)
{
	lua_pushcfunction(L, l_addChar);          lua_setglobal(L, "addChar");
	lua_pushcfunction(L, l_getCharName);      lua_setglobal(L, "getCharName");
	lua_pushcfunction(L, l_getCharFileName);  lua_setglobal(L, "getCharFileName");
	lua_pushcfunction(L, l_loadLifebar);      lua_setglobal(L, "loadLifebar");
	lua_pushcfunction(L, l_setLifebarDisplay); lua_setglobal(L, "setLifebarDisplay");
}

} // namespace ikemen
