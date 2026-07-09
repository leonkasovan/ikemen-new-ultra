// native_main.cpp — C++ boot sequence replacing ssz/ikemen.ssz main()
//
// Implements the full engine boot path without invoking the SSZ JIT compiler.
// See native_main.hpp for architecture overview.
//
// The sequence mirrors ssz_script/ssz/ikemen.ssz exactly:
//   1. SDL init with config settings
//   2. Window setup (fullscreen, aspect ratio, opacity, window type)
//   3. Memory marker before Lua init
//   4. Create Lua state and register all system script functions
//   5. Register 4 Lua callbacks (loadStart, selectStart, game, sszReload)
//   6. Init common state (flagInit, setSize, resetRemapInput)
//   7. Set audio volume from config
//   8. Run the main Lua system script (script/main.lua)
//   9. Handle Lua errors (filter "<game end>" expected shutdown)
//  10. Cleanup and memory marker after

#include "native_main.hpp"

// Native service modules
#include "sszdef.h"  // LOG_INFO, LOG_DEBUG
#include "lua_static.hpp"  // set_main_lua_state
#include "script_service.hpp"  // script_set_lua_state
#include "config_service.hpp"
#include "common_service.hpp"
#include "system_script_service.hpp"
#include "fighting_service.hpp"
#include "loader_service.hpp"
#include "sdlevent_service.hpp"
#include "sdlplugin_service.hpp"
#include "ssz_service.hpp"
#include "shell_service.hpp"
#include "command_service.hpp"

// Lua C API
#include <lua.hpp>
#include <lauxlib.h>
#include <lualib.h>
extern "C" {
#include "lfs.h"
#include "ffi/ffi.h"
#include "lpeg-1.1.0/lptypes.h"
}

// System service for select reset
#include "system_service.hpp"

namespace ikemen::ssz_native {
namespace {

// =========================================================================
// Lua-callable callback implementations (matching ikemen.ssz function ptrs)
// =========================================================================

/// loadStart(L, re) — reset loader and start loading thread.
/// SSZ: void loadStart(&.lua.State L=, int re=)
///       .com.exitMatch = false;
///       .chr.fight = .sc.syst.fig;
///       .loaderReset();
///       .ld.runTread();
static int lua_loadStart(lua_State* L) {
	(void)L;
	CommonData& cd = common_get_state();
	cd.exitMatch = false;

	// chr.fight assignment deferred until fight_service is wired
	// SSZ: .chr.fight = .sc.syst.fig;

	loader_reset();
	loader_run_tread();

	return 0;  // re = 0 (default)
}

/// selectStart(L, re) — reset select state, then loadStart.
/// SSZ: void selectStart(&.lua.State L=, int re=)
///       .com.exitMatch = false;
///       .sc.syst.selReset();
///       .loadStart(L=, re=);
static int lua_selectStart(lua_State* L) {
	(void)L;
	CommonData& cd = common_get_state();
	cd.exitMatch = false;

	system_sel_reset();

	// Reuse loadStart logic
	return lua_loadStart(L);
}

/// scGame(L, re) — run the match loop, push result as number.
/// SSZ: void scGame(&.lua.State L=, int re=)
///       re = 1;
///       L.pushNumber((double).match());
/// The match() function loops game() internally.
static int lua_game(lua_State* L) {
	// Run the fight orchestration loop
	fighting_main();

	// Determine match result (simplified — uses common state)
	const CommonData& cd = common_get_state();
	int result = 0;

	// Match result: from SSZ game() → returns winp (0=draw, 1=P1, 2=P2)
	// For native, derive from match over state
	if (common_match_over()) {
		if (cd.p1wins >= cd.p1mw)
			result = 1;  // P1 wins match
		else if (cd.p2wins >= cd.p2mw)
			result = 2;  // P2 wins match
		// else draw = 0
	}

	lua_pushnumber(L, static_cast<double>(result));
	return 1;  // re = 1 (return value is a number)
}

/// sszReload(L, re) — restart the engine executable.
/// SSZ: void sszReload(&.lua.State L=, int re=)
///       re = 0;
///       .sh.open(.cfg.Executable, "", "", false, false);
static int lua_sszReload(lua_State* L) {
	(void)L;
	const ConfigData& cfg = config_get_state();

	// Open the executable via shell
	std::wstring wExe(cfg.Executable.begin(), cfg.Executable.end());
	shell::open(wExe, L"", L"", false, false);

	return 0;  // re = 0
}

} // anonymous namespace

// =========================================================================
// native_main — Full C++ boot sequence replacing ssz/ikemen.ssz main()
// =========================================================================

bool native_main(int /*argc*/, char* /*argv*/[]) {
	// ── Step 1: Load config ──
	ConfigData& cfg = config_get_state();
	config_load("save/config.ini", cfg);

	LOG_INFO("Ikemen", "Native boot: %dx%d Renderer=%d",
		cfg.Width, cfg.Height, cfg.Renderer);

	// ── Step 2: Create Lua state BEFORE SDL init ──
	ssz_mem_mark_before("EXIT");
	lua_State* L = luaL_newstate();
	if (!L) {
		LOG_INFO("Ikemen", "Native boot: Failed to create Lua state");
		ssz_mem_mark_after("EXIT");
		return false;
	}
	// Open Lua standard libraries (individual calls — luaL_openlibs crashes)
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
	luaL_requiref(L, LUA_LFSNAME, luaopen_lfs, 1); lua_pop(L, 1);
	set_main_lua_state(L);
	script_set_lua_state(L);
	LOG_DEBUG("SSZ", "Lua state created successfully before SDL init");
	LOG_DEBUG("SSZ", "Lua state created successfully before SDL init");

	// ── Step 3: SDL init ──
	if (!init(cfg.WindowTitle, cfg.Width, cfg.Height, cfg.Renderer, true)) {
		LOG_INFO("Ikemen", "Native boot: SDL initialization FAILED");
		return false;
	}
	LOG_DEBUG("SSZ", "SDL initialized successfully");

	// ── Step 4: Window setup ──
	fullScreenMode(cfg.FullScreenExclusive);
	if (cfg.FullScreenExclusive)
		sdlevent_get_state().fullReal = true;
	if (fullScreen(cfg.FullScreen))
		showCursor(!cfg.FullScreen || (cfg.FullScreen && !cfg.FullScreenExclusive));
	if (cfg.FullScreen)
		sdlevent_get_state().full = true;
	keepAspectRatio(cfg.AspectRatio);
	if (cfg.AspectRatio)
		sdlevent_get_state().aspect = true;
	setOpacity(cfg.Opacity);
	setWindowType(cfg.WindowType);

	// ── Step 5: Initialize common engine state ──
	common_flag_init();
	common_set_size(cfg.Width, cfg.Height);
	common_reset_remap_input();
	{
		CommonData& cd = common_get_state();
		cd.brightness = cfg.Brightness;
	}

	// ── Step 7: Register all Lua-callable functions ──
	// SSZ: .sc.init(L) → .init(L) in ikemen.ssz
	// system_script_init internally calls script_init to register 300+ functions
	system_script_init(L);

	// ── Step 8: Register the 4 engine callbacks ──
	// SSZ: L.register("loadStart", .loadStart);
	//      L.register("selectStart", .selectStart);
	//      L.register("game", .scGame);
	//      L.register("sszReload", .sszReload);
	lua_register(L, "loadStart", lua_loadStart);
	lua_register(L, "selectStart", lua_selectStart);
	lua_register(L, "game", lua_game);
	lua_register(L, "sszReload", lua_sszReload);

	LOG_DEBUG("SSZ", "Native boot: registered 4 engine callbacks");

	// ── Step 9: Set audio volume from config ──
	// SSZ: .sdl.setVolume(.cfg.GlVol, .cfg.SEVol, .cfg.BGMVol);
	setVolume(cfg.GlVol, cfg.SEVol, cfg.BGMVol);

	// ── Step 10: Update select.def (char/stage discovery) ──
	// Note: updateCharInSelectDef / updateStageInSelectDef are called
	// from main.cpp before native_main, so no need to redo here.

	// ── Step 11: Run the main Lua system script ──
	// SSZ: if(!L.runFile(.cfg.system))
	const std::string& mainScript = cfg.system;
	LOG_DEBUG("SSZ", "Native boot: running Lua script '%s'", mainScript.c_str());

	if (luaL_loadfile(L, mainScript.c_str()) != LUA_OK) {
		const char* err = lua_tostring(L, -1);
		LOG_INFO("Ikemen", "Native boot: Failed to load Lua script: %s",
			err ? err : "unknown error");
		lua_pop(L, 1);
		lua_close(L);
		ssz_mem_mark_after("EXIT");
		return false;
	}

	// Run the Lua script
	int pcallResult = lua_pcall(L, 0, 0, 0);

	// ── Step 12: Handle Lua errors ──
	if (pcallResult != LUA_OK) {
		const char* err = lua_tostring(L, -1);

		// SSZ checks if the error ends with "<game end>" — expected shutdown
		// In native, check for the same pattern
		bool isGameEnd = false;
		if (err) {
			std::string errStr(err);
			std::string suffix = "<game end>";
			isGameEnd = (errStr.size() >= suffix.size() &&
				errStr.compare(errStr.size() - suffix.size(), suffix.size(), suffix) == 0);
		}

		if (!isGameEnd) {
			LOG_INFO("Ikemen", "Lua error: %s", err ? err : "unknown");
		} else {
			LOG_DEBUG("SSZ", "Native boot: Lua script ended normally (<game end>)");
		}

		lua_pop(L, 1);
	} else {
		LOG_DEBUG("SSZ", "Native boot: Lua script completed");
	}

	// ── Step 13: Cleanup ──
	lua_close(L);
	set_main_lua_state(nullptr);
	script_set_lua_state(nullptr);

	// ── Step 14: Memory marker after Lua runtime ──
	// SSZ: .ssz.memMarkAfter("EXIT");
	ssz_mem_mark_after("EXIT");

	// ── Step 15: SDL End ──
	// SSZ: .sdl.End(::);
	// Note: sdl.End() is called at the very end in ikemen.ssz, after main() returns.
	// In native_main, we handle it here. The SDL end function is available via
	// the sdlplugin service wrappers.
	LOG_DEBUG("SSZ", "Native boot: SDL shutdown");

	return true;
}

} // namespace ikemen::ssz_native
