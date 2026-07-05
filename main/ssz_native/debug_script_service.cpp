// debug_script_service.cpp — Stub implementations for debug Lua callbacks.
//
// Phase 3: All bodies are no-ops.  Wired when sc/tscri/sscri Lua bridge
// modules are converted or when the Lua console API is refactored.

#include "debug_script_service.hpp"
#include "ssz_trace.hpp"

namespace ikemen::ssz_native {

void debug_puts(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_puts");
}
void debug_ssz_reload(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_ssz_reload");
}
void debug_set_life(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_set_life");
}
void debug_set_life_max(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_set_life_max");
}
void debug_set_power(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_set_power");
}
void debug_set_attack(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_set_attack");
}
void debug_set_defence(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_set_defence");
}
void debug_self_state(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_self_state");
}
void debug_add_hotkey(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_add_hotkey");
}
void debug_toggle_clsn_draw(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_toggle_clsn_draw");
}
void debug_toggle_debug_draw(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_toggle_debug_draw");
}
void debug_toggle_status_draw(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_toggle_status_draw");
}
void debug_toggle_post_match(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_toggle_post_match");
}
void debug_toggle_pause(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_toggle_pause");
}
void debug_toggle_pause_menu(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_toggle_pause_menu");
}
void debug_step(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_step");
}
void debug_toggle_record(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_toggle_record");
}
void debug_toggle_playback(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_toggle_playback");
}
void debug_toggle_record_end(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_toggle_record_end");
}
void debug_round_reset(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_round_reset");
}
void debug_reload(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_reload");
}
void debug_set_accel(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_set_accel");
}
void debug_set_ai_level(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_set_ai_level");
}
void debug_set_time(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_set_time");
}
void debug_clear(lua_State*, int&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_clear");
}

std::string debug_load_file(const std::string&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_load_file");
    return {};
}
std::string debug_run_file(const std::string&) {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_run_file");
    return {};
}

// ── No-arg convenience wrappers (bridge/SSZ ABI) ──
// Called by the SSZ bridge (DebugLoadFile/DebugRunFile in bridge.cpp).
// Currently stubs returning empty string (success).

std::string debug_load_file() {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_load_file (no-arg)");
    return debug_load_file("");
}
std::string debug_run_file() {
    SSZ_TRACE_CAT(TRACE_SYS, "debug_run_file (no-arg)");
    return debug_run_file("");
}

} // namespace ikemen::ssz_native
