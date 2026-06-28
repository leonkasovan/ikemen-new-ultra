// debug_script_service.hpp — Native C++ scaffolding for ssz_script/ssz/debug-script.ssz
//
// debug-script.ssz implements the Lua debug API — functions registered as
// Lua callbacks for developer tools (stat manipulation, toggles, recording,
// hotkeys, reload, etc.).
//
// All functions receive a lua_State* (L) and int re (return code), parse
// arguments via the SSZ script runtime, and mutate game state.
//
// Design note: This is a Lua callback bridge module. Unlike share_service
// and system_service (which are DTO structs), this module's functions are
// designed to be registered as Lua C callbacks via sc/tscri/sscri init.
//
// Phase 3: All function bodies are stubs. Wired when sc/tscri/sscri Lua
// bridge modules are converted or when the engine's Lua console/API layer
// is refactored.

#pragma once

#include <string>

// Forward declarations for Lua types
struct lua_State;

namespace ikemen::ssz_native {

// ── Debug Script State ──
// Module-level globals from debug-script.ssz
struct DebugScriptState {
	bool roundResetFlg{false};
	bool reloadFlg{false};
	bool noHUDDisplay{false};
	lua_State* L{nullptr};

	// Module-level globals as independent flags (not wrapped in the struct
	// when used as free functions by the Lua bridge — kept here for struct
	// completeness).
};

// ── Lua callback function stubs ──
// Each matches the SSZ signature: void func(lua_State* L, int& re)
// Phase 3: bodies are no-ops until the Lua bridge is wired.

void debug_puts(lua_State* L, int& re);
void debug_ssz_reload(lua_State* L, int& re);
void debug_set_life(lua_State* L, int& re);
void debug_set_life_max(lua_State* L, int& re);
void debug_set_power(lua_State* L, int& re);
void debug_set_attack(lua_State* L, int& re);
void debug_set_defence(lua_State* L, int& re);
void debug_self_state(lua_State* L, int& re);
void debug_add_hotkey(lua_State* L, int& re);
void debug_toggle_clsn_draw(lua_State* L, int& re);
void debug_toggle_debug_draw(lua_State* L, int& re);
void debug_toggle_status_draw(lua_State* L, int& re);
void debug_toggle_post_match(lua_State* L, int& re);
void debug_toggle_pause(lua_State* L, int& re);
void debug_toggle_pause_menu(lua_State* L, int& re);
void debug_step(lua_State* L, int& re);
void debug_toggle_record(lua_State* L, int& re);
void debug_toggle_playback(lua_State* L, int& re);
void debug_toggle_record_end(lua_State* L, int& re);
void debug_round_reset(lua_State* L, int& re);
void debug_reload(lua_State* L, int& re);
void debug_set_accel(lua_State* L, int& re);
void debug_set_ai_level(lua_State* L, int& re);
void debug_set_time(lua_State* L, int& re);
void debug_clear(lua_State* L, int& re);

// ── File loading ──
// loadFile and runFile are higher-level operations that wire the Lua
// environment.  Phase 3: stubs returning empty string (success).
std::string debug_load_file(const std::string& file);
std::string debug_run_file(const std::string& file);

} // namespace ikemen::ssz_native
