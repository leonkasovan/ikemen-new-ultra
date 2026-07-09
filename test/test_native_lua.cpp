// test_native_lua.cpp — Tests script/trigger/system_script Lua callback registrations.
// Links full engine (including SDL) but only calls init + registration verification.
// No game state needed — pure Lua registry checks.
//
// Build: make test-native-lua

#include <stdint.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <windows.h>

#define SDL_MAIN_HANDLED
#include "sszdef.h"
#include "ssz_native/plugin_native_api.hpp"
#include "ssz_native/script_service.hpp"
#include "ssz_native/trigger_script_service.hpp"
#include "ssz_native/system_script_service.hpp"
#include "ssz_native/common_service.hpp"
#include "ssz_native/lua_service.hpp"
#include <lua.hpp>

// NewState() is declared in plugin_native_api.hpp and defined in lua.cpp
lua_State* SSZ_STDCALL NewState();

#define TEST(name, expr) do { g_tests++; if (!(expr)) { g_fails++; std::wcerr << L"FAIL: " << name << std::endl; } else { std::wcout << L"PASS: " << name << std::endl; } } while(0)

static int g_tests = 0, g_fails = 0;

static lua_State* make_lua() {
    lua_State* L = luaL_newstate();
    if (L) luaL_openlibs(L);
    return L;
}

static void test_script_service()
{
    using namespace ikemen::ssz_native;
    std::wcout << L"\n--- Script service ---" << std::endl;

    // L1: nullptr safety
    script_init(nullptr);              TEST(L"script_init(nullptr)", true);
    script_set_lua_state(nullptr);     TEST(L"script_set_lua_state(nullptr)", true);
    script_init();                     TEST(L"script_init() no-arg", true);
    ScriptState ss;                    TEST(L"ScriptState line empty", ss.line.empty());
    ScriptState& ref = script_get_state(); TEST(L"script_get_state", &ref != nullptr);

    // L2: register + verify
    lua_State* L = make_lua();
    TEST(L"luaL_newstate", L != nullptr);
    if (!L) {
        std::wcout << L"  SKIP: luaL_newstate returned NULL" << std::endl;
        return;
    }
    script_init(L);

    const char* names[] = {
        "numArg","blArg","strArg","refArg","sffNew","sndNew","fontNew",
        "commandNew","sszRandom","setAutoguard","exitMatch","setSharedString",
        "startButton","getSysCtrl","setSysCtrl","inputText","clipboardPaste",
        "loadVideo","playBGM","sszOpen","batOpen","webOpen"
    };
    for (auto n : names) {
        lua_getglobal(L, n);
        std::string lbl = std::string("script::") + n + " reg";
        TEST(lbl.c_str(), lua_isfunction(L, -1));
        lua_pop(L, 1);
    }
    lua_getglobal(L, "nonexistent");
    TEST(L"nonexistent nil", lua_isnil(L, -1)); lua_pop(L, 1);
    lua_close(L);
}

static void test_trigger_script_service()
{
    using namespace ikemen::ssz_native;
    std::wcout << L"\n--- Trigger script service ---" << std::endl;

    register_function(nullptr);  TEST(L"register_function(nullptr)", true);
    register_function();         TEST(L"register_function() no-arg", true);
    TriggerScriptState ts;       TEST(L"TriggerScriptState cwc null", ts.cwc == nullptr);
    TriggerScriptState& ref = trigger_script_get_state(); TEST(L"trigger_script_get_state", &ref != nullptr);

    lua_State* L = make_lua();   TEST(L"luaL_newstate", L != nullptr);
    if (!L) return;
    register_function(L);

    const char* names[] = {"player","parent","root","helper","target","partner"};
    for (auto n : names) {
        lua_getglobal(L, n);
        std::string lbl = std::string("trigger::") + n + " reg";
        TEST(lbl.c_str(), lua_isfunction(L, -1));
        lua_pop(L, 1);
    }
    // pcall safe defaults
    for (auto [name, args] : {std::pair{"parent",0}, {"root",0}, {"player",1}, {"helper",1}, {"target",1}}) {
        lua_getglobal(L, name);
        if (lua_isfunction(L, -1)) {
            if (args > 0) lua_pushnumber(L, args == 1 ? 1 : -1);
            int status = lua_pcall(L, args, 1, 0);
            if (status == LUA_OK) {
                bool is_bool = lua_isboolean(L, -1);
                std::string lbl = std::string("trigger::") + name + "() ret bool";
                TEST(lbl.c_str(), is_bool);
                lua_pop(L, 1);
            } else { lua_pop(L, 1); }
        }
    }
    lua_close(L);
}

static void test_system_script_service()
{
    using namespace ikemen::ssz_native;
    std::wcout << L"\n--- System script service ---" << std::endl;

    system_script_init(nullptr);  TEST(L"system_script_init(nullptr)", true);
    system_script_init();         TEST(L"system_script_init() no-arg", true);
    SystemScriptState ss;         TEST(L"SystemScriptState created", true);
    SystemScriptState& ref = system_script_get_state(); TEST(L"system_script_get_state", &ref != nullptr);

    lua_State* L = make_lua();   TEST(L"luaL_newstate", L != nullptr);
    if (!L) return;
    system_script_init(L);  // also calls script_init

    const char* names[] = {
        "sleep","textImgNew","textImgGetWidth","textImgSetFont","textImgSetBank",
        "textImgSetAlpha","textImgSetWindow","textImgSetAlign","textImgSetText",
        "textImgSetPos","textImgAddPos","textImgSetScale","textImgDraw",
        "animNew","animSetPos","animAddPos","animSetTile","animSetColorKey",
        "animSetPal","animSetAlpha","animSetScale","animSetWindow","animGetFrame",
        "animUpdate","animReset","animDraw",
        "enterNetPlay","exitNetPlay","enterReplay","exitReplay",
        "netplay","replay","synchronize",
        "setCom","setTag","setAutoLevel",
        "setGameType","setGameMode","getGameMode",
        "setService","getService","setPlayerSide","getPlayerSide",
        "setPauseVar","getPauseVar",
        "setListenPort","getListenPort","setUserName","getUserName",
        "setInputDisplay","setAttackDisplay",
        "setPowerStateP1","setPowerStateP2","setLifeStateP1","setLifeStateP2",
        "setDummyState","setDummyDistance","setDummyGuard","setDummyRecovery",
        "setCounterHit","setBrightness","getBrightness","setOpacity",
        "setVolume","setVideoVolume","getVideoVolume","setPanStr",
        "getScreenshotsPath","getWindowTitle","getWidth","getHeight",
        "setGameRes","setWindowType","getWindowType",
        "setFullScreenMode","getFullScreenMode","setScreenMode","getScreenMode",
        "setAspectRatio","getAspectRatio","takeScreenShot",
        "swapGamepad","disableGamepad","swapController",
        "setInputConfig","getInputKeyboard","getInputID","inputReset",
        "addChar","addStage","setRandomSpr",
        "setSelColRow","setSelCellSize","setSelCellScale",
        "numSelCells","setStage","selectStage","setStgMusic","setTeamMode",
        "getCharName","getStageName",
    };
    for (auto n : names) {
        lua_getglobal(L, n);
        std::string lbl = std::string("system::") + n + " reg";
        TEST(lbl.c_str(), lua_isfunction(L, -1));
        lua_pop(L, 1);
    }
    // Verify script functions also registered
    lua_getglobal(L, "numArg"); TEST(L"script::numArg via system_init", lua_isfunction(L, -1)); lua_pop(L, 1);
    lua_getglobal(L, "sffNew"); TEST(L"script::sffNew via system_init", lua_isfunction(L, -1)); lua_pop(L, 1);

    // pcall safe defaults
    lua_getglobal(L, "textImgGetWidth");
    if (lua_isfunction(L, -1) && lua_pcall(L, 0, 1, 0) == LUA_OK) {
        TEST(L"textImgGetWidth()=0.0", lua_tonumber(L, -1) == 0.0); lua_pop(L, 1);
    }
    lua_getglobal(L, "getGameMode");
    if (lua_isfunction(L, -1) && lua_pcall(L, 0, 1, 0) == LUA_OK) {
        TEST(L"getGameMode() str", lua_isstring(L, -1)); lua_pop(L, 1);
    }
    lua_getglobal(L, "parent");
    if (lua_isfunction(L, -1) && lua_pcall(L, 0, 1, 0) == LUA_OK) {
        TEST(L"parent() false", lua_toboolean(L, -1) == false); lua_pop(L, 1);
    }
    lua_close(L);
}

int main()
{
    std::wcout << L"=== Native Lua module tests ===\n";
    test_script_service();
    test_trigger_script_service();
    test_system_script_service();
    std::wcout << L"\n=== " << g_tests << L" tests, " << g_fails << L" failures ===\n";
    return g_fails > 0 ? 1 : 0;
}
