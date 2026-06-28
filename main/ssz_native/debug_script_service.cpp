// debug_script_service.cpp — Stub implementations for debug Lua callbacks.
//
// Phase 3: All bodies are no-ops.  Wired when sc/tscri/sscri Lua bridge
// modules are converted or when the Lua console API is refactored.

#include "debug_script_service.hpp"

namespace ikemen::ssz_native {

void debug_puts(lua_State*, int&) {}
void debug_ssz_reload(lua_State*, int&) {}
void debug_set_life(lua_State*, int&) {}
void debug_set_life_max(lua_State*, int&) {}
void debug_set_power(lua_State*, int&) {}
void debug_set_attack(lua_State*, int&) {}
void debug_set_defence(lua_State*, int&) {}
void debug_self_state(lua_State*, int&) {}
void debug_add_hotkey(lua_State*, int&) {}
void debug_toggle_clsn_draw(lua_State*, int&) {}
void debug_toggle_debug_draw(lua_State*, int&) {}
void debug_toggle_status_draw(lua_State*, int&) {}
void debug_toggle_post_match(lua_State*, int&) {}
void debug_toggle_pause(lua_State*, int&) {}
void debug_toggle_pause_menu(lua_State*, int&) {}
void debug_step(lua_State*, int&) {}
void debug_toggle_record(lua_State*, int&) {}
void debug_toggle_playback(lua_State*, int&) {}
void debug_toggle_record_end(lua_State*, int&) {}
void debug_round_reset(lua_State*, int&) {}
void debug_reload(lua_State*, int&) {}
void debug_set_accel(lua_State*, int&) {}
void debug_set_ai_level(lua_State*, int&) {}
void debug_set_time(lua_State*, int&) {}
void debug_clear(lua_State*, int&) {}

std::string debug_load_file(const std::string&) { return {}; }
std::string debug_run_file(const std::string&) { return {}; }

} // namespace ikemen::ssz_native
