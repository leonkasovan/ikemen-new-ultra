// debug_script_service.hpp — Native C++ implementation for ssz_script/ssz/debug-script.ssz
//
// debug-script.ssz implements the Lua debug API — functions registered as
// Lua callbacks for developer tools (stat manipulation, display toggles,
// recording, hotkeys, reload, etc.).
//
// All functions follow the standard lua_CFunction pattern:
//   int func(lua_State* L)
// They parse arguments from Lua, mutate game state via native services,
// and return 0 (or 1 for value returns).
//
// Registration: debug_script_init(lua_State* L) registers all 25 debug
// functions with Lua. Called from debug_load_file() or directly from
// the Lua bridge when IKEMEN_NATIVE_DEBUG_SCRIPT_LIB=1.

#pragma once

#include <string>
#include <vector>

// Forward declarations for Lua types
struct lua_State;

namespace ikemen::ssz_native {

// ── Hotkey entry — stored from addHotkey calls ──
struct HotkeyEntry {
	std::string key;
	bool ctrl;
	bool alt;
	bool shift;
	std::string script;
};

// ── Debug Script State ──
// Module-level globals from debug-script.ssz
struct DebugScriptState {
	bool roundResetFlg{false};
	bool reloadFlg{false};
	bool noHUDDisplay{false};
	lua_State* L{nullptr};
	std::vector<std::string> clipboardText;  // Clipboard buffer (replaces SSZ com.clipboardText)
	std::vector<HotkeyEntry> hotkeys;        // Registered debug hotkeys (from addHotkey)
};

// ── Lua callback function declarations ──
// Each implements one SSZ debug-script function as a lua_CFunction.

int lua_debug_puts(lua_State* L);
int lua_debug_ssz_reload(lua_State* L);
int lua_debug_set_life(lua_State* L);
int lua_debug_set_life_max(lua_State* L);
int lua_debug_set_power(lua_State* L);
int lua_debug_set_attack(lua_State* L);
int lua_debug_set_defence(lua_State* L);
int lua_debug_self_state(lua_State* L);
int lua_debug_add_hotkey(lua_State* L);
int lua_debug_toggle_clsn_draw(lua_State* L);
int lua_debug_toggle_debug_draw(lua_State* L);
int lua_debug_toggle_status_draw(lua_State* L);
int lua_debug_toggle_post_match(lua_State* L);
int lua_debug_toggle_pause(lua_State* L);
int lua_debug_toggle_pause_menu(lua_State* L);
int lua_debug_step(lua_State* L);
int lua_debug_toggle_record(lua_State* L);
int lua_debug_toggle_playback(lua_State* L);
int lua_debug_toggle_record_end(lua_State* L);
int lua_debug_round_reset(lua_State* L);
int lua_debug_reload(lua_State* L);
int lua_debug_set_accel(lua_State* L);
int lua_debug_set_ai_level(lua_State* L);
int lua_debug_set_time(lua_State* L);
int lua_debug_clear(lua_State* L);

// ── Init ──
// Registers all 25 debug functions with the given Lua state so they
// are callable from Lua scripts as global functions (puts, setLife,
// toggleClsnDraw, etc.).
//
// Called from debug_load_file() or from the SSZ bridge when
// IKEMEN_NATIVE_DEBUG_SCRIPT_LIB=1.
void debug_script_init(lua_State* L);

// No-arg convenience wrapper — calls debug_script_init on stored Lua state.
void debug_script_init();

// ── File loading ──
// Loads and runs a debug Lua script file. Sets up the Lua environment
// first by calling script_init(), register_function(), system_script_init(),
// and debug_script_init(), then runs the file.
//
// Returns empty string on success, error message on failure.
std::string debug_load_file(const std::string& file);
std::string debug_run_file(const std::string& file);

// No-arg convenience wrappers for bridge/SSZ ABI.
std::string debug_load_file();
std::string debug_run_file();

// ── State accessor ──
DebugScriptState& debug_script_get_state();

} // namespace ikemen::ssz_native
