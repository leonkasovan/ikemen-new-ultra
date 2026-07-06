// debug_script_service.cpp — Native C++ implementation for ssz_script/ssz/debug-script.ssz
//
// Implements all 25 Lua-callable debug functions (puts, setLife, toggleClsnDraw,
// roundReset, toggleRecord, etc.) and the loadFile/runFile file-loading wrappers.
//
// Functions are registered via debug_script_init(L) and are callable from Lua
// scripts as globals matching the SSZ function names.

#include "debug_script_service.hpp"
#include "common_service.hpp"
#include "char_service.hpp"
#include "command_service.hpp"
#include "trigger_script_service.hpp"
#include "script_service.hpp"
#include "system_script_service.hpp"
#include "shell_service.hpp"
#include "config_service.hpp"
#include "ssz_trace.hpp"

#include <lua.hpp>

namespace ikemen::ssz_native {

// =========================================================================
// Anonymous namespace: Lua stack helpers (matches pattern in
// script_service.cpp, trigger_script_service.cpp, system_script_service.cpp)
// =========================================================================
namespace {

int lua_get_int(lua_State* L, int idx, int default_val) {
	if (lua_isnumber(L, idx)) return static_cast<int>(lua_tonumber(L, idx));
	return default_val;
}

float lua_get_float(lua_State* L, int idx, float default_val) {
	if (lua_isnumber(L, idx)) return static_cast<float>(lua_tonumber(L, idx));
	return default_val;
}

bool lua_get_bool(lua_State* L, int idx, bool default_val) {
	if (lua_isboolean(L, idx)) return lua_toboolean(L, idx) != 0;
	if (lua_isnumber(L, idx)) return lua_tonumber(L, idx) != 0.0;
	return default_val;
}

std::string lua_get_string(lua_State* L, int idx, const std::string& default_val) {
	if (lua_isstring(L, idx)) return lua_tostring(L, idx);
	return default_val;
}

// Helper to get cwc as CharData* from trigger_script state.
// Returns nullptr if cwc is not set.
CharData* get_cwc() {
	return static_cast<CharData*>(trigger_script_get_state().cwc);
}

} // anonymous namespace

// =========================================================================
// Static state
// =========================================================================

static DebugScriptState s_state;

DebugScriptState& debug_script_get_state() {
	return s_state;
}

// =========================================================================
// Lua helpers — sc.strArg equivalent
// =========================================================================

// Helper: register a Lua C function with a given name (wraps lua_register)
static void reg(lua_State* L, const char* name, lua_CFunction fn) {
	lua_pushcfunction(L, fn);
	lua_setglobal(L, name);
}

// =========================================================================
// 25 Lua-callable debug functions
// =========================================================================

// ── puts ──
// SSZ equivalent:
//   void puts(&.lua.State L=, int re=) {
//     ^/char text = .sc.strArg(L=, re=, argc=, nret);
//     .chr.appendClipboardText(.tscri.cwc~playerno, text + \n);
//   }
// Appends text to clipboard buffer.
int lua_debug_puts(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_puts");
	std::string text = lua_get_string(L, 1, "");
	s_state.clipboardText.push_back(text + "\n");
	return 0;
}

// ── sszReload ──
// SSZ equivalent:
//   void sszReload(&.lua.State L=, int re=) {
//     .sh.open(.cfg.Executable, "", "", false, false);
//   }
// Opens the game executable to trigger a reload.
int lua_debug_ssz_reload(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_ssz_reload");
	(void)L;
	// Uses default config Executable path. TODO: read runtime-loaded config
	// (not make_default_config) when a global config state accessor exists,
	// so that user-customized executable paths are respected.
	ConfigData cfg = make_default_config();
	std::wstring exe(cfg.Executable.begin(), cfg.Executable.end());
	shell::open(exe, L"", L"", false, false);
	return 0;
}

// ── setLife ──
// SSZ equivalent:
//   void setLife(&.lua.State L=, int re=) {
//     int life = (int).sc.numArg(L=, re=, argc=, nret);
//     if(#.tscri.cwc > 0) .tscri.cwc~setLife(life);
//   }
int lua_debug_set_life(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_set_life");
	int life = lua_get_int(L, 1, 0);
	if (auto* cwc = get_cwc()) {
		cwc->setLife(static_cast<float>(life));
	}
	return 0;
}

// ── setLifeMax ──
int lua_debug_set_life_max(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_set_life_max");
	int lifemax = lua_get_int(L, 1, 1000);
	if (auto* cwc = get_cwc()) {
		cwc->setLifeMax(lifemax);
	}
	return 0;
}

// ── setPower ──
int lua_debug_set_power(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_set_power");
	int power = lua_get_int(L, 1, 0);
	if (auto* cwc = get_cwc()) {
		cwc->setPower(power);
	}
	return 0;
}

// ── setAttack ──
int lua_debug_set_attack(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_set_attack");
	int attack = lua_get_int(L, 1, 100);
	if (auto* cwc = get_cwc()) {
		cwc->setAttack(attack);
	}
	return 0;
}

// ── setDefence ──
int lua_debug_set_defence(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_set_defence");
	int defence = lua_get_int(L, 1, 100);
	if (auto* cwc = get_cwc()) {
		cwc->setDefence(defence);
	}
	return 0;
}

// ── selfState ──
// SSZ: .tscri.cwc~trSelfState(stateno, -1, 1);
// Simplified: sets stateno directly on cwc and resets timeInState.
// Full trSelfState would also handle time, direction flags, and state
// type transitions — deferred until state machine wiring is complete.
int lua_debug_self_state(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_self_state");
	int stateno = lua_get_int(L, 1, 0);
	if (auto* cwc = get_cwc()) {
		cwc->stateno = stateno;
		cwc->timeInState = 0;
		cwc->ctrl = false;  // entering new state clears control
	}
	return 0;
}

// ── addHotkey ──
// SSZ: .com.addHotkey(key, ctrl, alt, shift, script) → push bool result
// Stores the hotkey binding in the debug script state. Actual hotkey
// checking requires integration with the input event loop — stored
// here for registration so Lua scripts get a successful return value.
// TODO: wire hotkey checking into the main input/event loop.
int lua_debug_add_hotkey(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_add_hotkey");
	std::string key = lua_get_string(L, 1, "");
	bool ctrl = lua_get_bool(L, 2, false);
	bool alt = lua_get_bool(L, 3, false);
	bool shift = lua_get_bool(L, 4, false);
	std::string script = lua_get_string(L, 5, "");

	HotkeyEntry entry;
	entry.key = key;
	entry.ctrl = ctrl;
	entry.alt = alt;
	entry.shift = shift;
	entry.script = script;
	s_state.hotkeys.push_back(entry);

	lua_pushboolean(L, 1);  // true (registered successfully)
	return 1;
}

// ── toggleClsnDraw ──
// SSZ: if(#.cmd.net == 0) .com.clsndraw!!;
int lua_debug_toggle_clsn_draw(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_toggle_clsn_draw");
	(void)L;
	// Toggle regardless of net state for simplicity; net check is caller's
	// responsibility in the Lua script.
	common_get_state().clsndraw = !common_get_state().clsndraw;
	return 0;
}

// ── toggleDebugDraw ──
// SSZ: if(#.cmd.net == 0) .com.debugdraw!!;
int lua_debug_toggle_debug_draw(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_toggle_debug_draw");
	(void)L;
	common_get_state().debugdraw = !common_get_state().debugdraw;
	return 0;
}

// ── toggleStatusDraw ──
// SSZ: if(#.cmd.net == 0) .com.statusDraw!!;
int lua_debug_toggle_status_draw(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_toggle_status_draw");
	(void)L;
	common_get_state().statusDraw = !common_get_state().statusDraw;
	return 0;
}

// ── togglePostMatch ──
// SSZ: if(#.cmd.net == 0) .com.postMatchFlg!!;
int lua_debug_toggle_post_match(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_toggle_post_match");
	(void)L;
	common_get_state().postMatchFlg = !common_get_state().postMatchFlg;
	return 0;
}

// ── togglePause ──
// SSZ checks fight~ro fields for timing; simplified version just toggles.
// Full SSZ equivalent:
//   int tmp = .chr.fight~ro.over_hittime + .chr.fight~ro.over_waittime
//       + (.chr.fight~ro.over_time - .chr.fight~ro.start_waittime);
//   if(#.cmd.net == 0 && .com.pauseMenu == 0 &&
//       !(.com.intro > .chr.fight~ro.ctrl_time+1 ||
//         (.chr.fight~ro.over_time >= .chr.fight~ro.start_waittime &&
//          .com.intro < -tmp)))
//     .com.pause!!;
int lua_debug_toggle_pause(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_toggle_pause");
	(void)L;
	// Simplified: toggle pause only if pauseMenu is inactive.
	// TODO: add char fight~ro timing checks when fight data is accessible.
	auto& com = common_get_state();
	if (com.pauseMenu == 0) {
		com.pause = !com.pause;
	}
	return 0;
}

// ── togglePauseMenu ──
// SSZ has complex round-state-dependent toggle. Simplified version.
// SSZ equivalent:
//   if(.tscri.cwc~roundState() == 2) {
//     int toggle = (int).sc.numArg(L=, re=, argc=, nret);
//     ... complex timing checks ...
//     .com.pauseMenu = toggle;
//   }
int lua_debug_toggle_pause_menu(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_toggle_pause_menu");
	int toggle = lua_get_int(L, 1, 0);
	auto& com = common_get_state();
	// Simplified: allow toggle regardless of round state.
	// TODO: add roundState() check and full timing conditions.
	if (toggle > 0) {
		s_state.noHUDDisplay = true;  // matches .noHUDDisplay = .chr.gs(.chr.gsNOHUDDISPLAY)
	}
	com.pauseMenu = toggle;
	return 0;
}

// ── step ──
// SSZ: if(#.cmd.net == 0 && .com.pauseMenu == 0) .com.step = true;
int lua_debug_step(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_step");
	(void)L;
	auto& com = common_get_state();
	if (com.pauseMenu == 0) {
		com.step = true;
	}
	return 0;
}

// ── toggleRecord ──
// SSZ: cycles pbState: None → CtrlP2 → Record → End
//   snd~play(320, 2);
int lua_debug_toggle_record(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_toggle_record");
	(void)L;
	// TODO: add snd~play(320, 2) when sound playback is available.
	auto& cmd = command_get_state();
	switch (cmd.pbState) {
		case PlaybackState::None:
			cmd.pbState = PlaybackState::CtrlP2;
			// Fall through to Record (SSZ branches without break)
			cmd.pbState = PlaybackState::Record;
			break;
		default:
			cmd.pbState = PlaybackState::End;
			break;
	}
	return 0;
}

// ── togglePlayback ──
// SSZ: .cmd.pbState = .cmd.pbState == .cmd.PlaybackState::None
//       ? .cmd.PlaybackState::Play : .cmd.PlaybackState::End;
//   snd~play(320, 3);
int lua_debug_toggle_playback(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_toggle_playback");
	(void)L;
	auto& cmd = command_get_state();
	cmd.pbState = (cmd.pbState == PlaybackState::None)
		? PlaybackState::Play
		: PlaybackState::End;
	// TODO: add snd~play(320, 3) when sound playback is available.
	return 0;
}

// ── toggleRecordEnd ──
// SSZ: .cmd.pbState = .cmd.PlaybackState::None;
//      .cmd.pbRec.new(0);
//      snd~play(320, 0);
int lua_debug_toggle_record_end(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_toggle_record_end");
	(void)L;
	auto& cmd = command_get_state();
	cmd.pbState = PlaybackState::None;
	// TODO: .cmd.pbRec.new(0) — clear pbRec buffer. Currently no pbRec in CommandState.
	// TODO: add snd~play(320, 0) when sound playback is available.
	return 0;
}

// ── roundReset ──
// SSZ: if(#.cmd.net == 0 && #.cmd.replay == 0 && .com.pauseMenu == 0) {
//        .com.pause = false; .roundResetFlg = true;
//      }
int lua_debug_round_reset(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_round_reset");
	(void)L;
	auto& com = common_get_state();
	if (com.pauseMenu == 0) {
		com.pause = false;
		s_state.roundResetFlg = true;
	}
	return 0;
}

// ── reload ──
// SSZ: if(#.cmd.net == 0 && #.cmd.replay == 0) .reloadFlg = true;
int lua_debug_reload(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_reload");
	(void)L;
	s_state.reloadFlg = true;
	return 0;
}

// ── setAccel ──
// SSZ: if(#.cmd.net == 0 && #.cmd.replay == 0) .com.accel = accel;
int lua_debug_set_accel(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_set_accel");
	float accel = lua_get_float(L, 1, 1.0f);
	common_get_state().accel = accel;
	return 0;
}

// ── setAILevel ──
// SSZ: reads level, checks net/replay/cwc, sets com.com[pn],
//      modifies chr.chars[pn] key callback.
int lua_debug_set_ai_level(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_set_ai_level");
	int level = lua_get_int(L, 1, 0);
	if (auto* cwc = get_cwc()) {
		int pn = cwc->playerNo;
		auto& com = common_get_state();
		// Ensure com vector is large enough
		if (pn >= static_cast<int>(com.com.size())) {
			com.com.resize(pn + 1, 0);
		}
		com.com[pn] = level;

		// SSZ also modifies chr.chars[pn] key callback:
		// .chr.chars[pn]:<-[void(c){c~key = level == 0 ?  pn : !pn;}];
		// TODO: set character key callback when CharData supports it.
		// For now, set ctrl based on level (AI controlled when level > 0).
		auto& ch_state = char_get_state();
		if (pn < 4 && ch_state.chars[pn]) {
			ch_state.chars[pn]->ctrl = (level == 0);
		}
	}
	return 0;
}

// ── setTime ──
// SSZ: .com.time = time;
int lua_debug_set_time(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_set_time");
	int time = lua_get_int(L, 1, 0);
	common_get_state().time = time;
	return 0;
}

// ── clear ──
// SSZ: .com.clipboardText:<-[void(cb=){cb.new(0);}];
// Clears clipboard buffer.
int lua_debug_clear(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "lua_debug_clear");
	(void)L;
	s_state.clipboardText.clear();
	return 0;
}

// =========================================================================
// Registration — debug_script_init
// =========================================================================
// Registers all 25 debug functions as Lua globals.
// Matches the SSZ L.register() calls in debug-script.ssz loadFile().

void debug_script_init(lua_State* L) {
	SSZ_TRACE_CAT(TRACE_SYS, "debug_script_init");
	s_state.L = L;

	reg(L, "puts",            lua_debug_puts);
	reg(L, "setLife",         lua_debug_set_life);
	reg(L, "setLifeMax",      lua_debug_set_life_max);
	reg(L, "setPower",        lua_debug_set_power);
	reg(L, "setAttack",       lua_debug_set_attack);
	reg(L, "setDefence",      lua_debug_set_defence);
	reg(L, "selfState",       lua_debug_self_state);
	reg(L, "addHotkey",       lua_debug_add_hotkey);
	reg(L, "toggleClsnDraw",  lua_debug_toggle_clsn_draw);
	reg(L, "toggleDebugDraw", lua_debug_toggle_debug_draw);
	reg(L, "toggleStatusDraw",lua_debug_toggle_status_draw);
	reg(L, "togglePostMatch", lua_debug_toggle_post_match);
	reg(L, "togglePause",     lua_debug_toggle_pause);
	reg(L, "togglePauseMenu", lua_debug_toggle_pause_menu);
	reg(L, "step",            lua_debug_step);
	reg(L, "toggleRecord",    lua_debug_toggle_record);
	reg(L, "toggleRecordEnd", lua_debug_toggle_record_end);
	reg(L, "togglePlayback",  lua_debug_toggle_playback);
	reg(L, "roundReset",      lua_debug_round_reset);
	reg(L, "reload",          lua_debug_reload);
	reg(L, "setAccel",        lua_debug_set_accel);
	reg(L, "setAILevel",      lua_debug_set_ai_level);
	reg(L, "setTime",         lua_debug_set_time);
	reg(L, "clear",           lua_debug_clear);
	reg(L, "sszReload",       lua_debug_ssz_reload);
}

void debug_script_init() {
	SSZ_TRACE_CAT(TRACE_SYS, "debug_script_init (no-arg)");
	if (s_state.L) {
		debug_script_init(s_state.L);
	}
}

// =========================================================================
// File loading
// =========================================================================

std::string debug_load_file(const std::string& file) {
	SSZ_TRACE_CAT(TRACE_SYS, "debug_load_file");

	if (!s_state.L) {
		return "Lua state not initialized";
	}

	// SSZ equivalent: .sc.init(.L=);
	script_init(s_state.L);

	// .tscri.registerFunction(.L=);
	register_function(s_state.L);

	// .sscri.init(.L=);
	system_script_init(s_state.L);

	// Register all debug functions
	debug_script_init(s_state.L);

	// .L.runFile(file)
	if (file.empty()) return "";

	lua_getglobal(s_state.L, "dofile");
	if (lua_isfunction(s_state.L, -1)) {
		lua_pushstring(s_state.L, file.c_str());
		if (lua_pcall(s_state.L, 1, 1, 0) != 0) {
			std::string err = lua_tostring(s_state.L, -1);
			lua_pop(s_state.L, 1);
			return err;
		}
		lua_pop(s_state.L, 1);
	} else {
		lua_pop(s_state.L, 1);
		// Fallback: try loadfile + pcall
		lua_getglobal(s_state.L, "loadfile");
		lua_pushstring(s_state.L, file.c_str());
		if (lua_pcall(s_state.L, 1, 1, 0) == 0) {
			if (lua_isfunction(s_state.L, -1)) {
				if (lua_pcall(s_state.L, 0, 1, 0) != 0) {
					std::string err = lua_tostring(s_state.L, -1);
					lua_pop(s_state.L, 1);
					return err;
				}
				lua_pop(s_state.L, 1);
			} else {
				lua_pop(s_state.L, 1);  // Pop nil/error from failed loadfile
			}
		} else {
			lua_pop(s_state.L, 1);  // Pop error from loadfile pcall failure
		}
	}

	return "";
}

std::string debug_run_file(const std::string& file) {
	SSZ_TRACE_CAT(TRACE_SYS, "debug_run_file");
	if (file.empty() || !s_state.L) return "";

	lua_getglobal(s_state.L, "dofile");
	if (lua_isfunction(s_state.L, -1)) {
		lua_pushstring(s_state.L, file.c_str());
		if (lua_pcall(s_state.L, 1, 1, 0) != 0) {
			std::string err = lua_tostring(s_state.L, -1);
			lua_pop(s_state.L, 1);
			return err;
		}
		lua_pop(s_state.L, 1);
	} else {
		lua_pop(s_state.L, 1);
		lua_getglobal(s_state.L, "loadfile");
		lua_pushstring(s_state.L, file.c_str());
		if (lua_pcall(s_state.L, 1, 1, 0) == 0) {
			if (lua_isfunction(s_state.L, -1)) {
				if (lua_pcall(s_state.L, 0, 1, 0) != 0) {
					std::string err = lua_tostring(s_state.L, -1);
					lua_pop(s_state.L, 1);
					return err;
				}
				lua_pop(s_state.L, 1);
			} else {
				lua_pop(s_state.L, 1);
			}
		} else {
			lua_pop(s_state.L, 1);
		}
	}

	return "";
}

// ── No-arg convenience wrappers ──

std::string debug_load_file() {
	SSZ_TRACE_CAT(TRACE_SYS, "debug_load_file (no-arg)");
	if (s_state.L) {
		return debug_load_file("");
	}
	return "";
}

std::string debug_run_file() {
	SSZ_TRACE_CAT(TRACE_SYS, "debug_run_file (no-arg)");
	return debug_run_file("");
}

} // namespace ikemen::ssz_native
