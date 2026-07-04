// common_service.cpp — Stub implementations for common.ssz scaffolding.
//
// Phase 3: All bodies are no-ops.  Wired when dependent modules are native.

#include "common_service.hpp"

namespace ikemen::ssz_native {

void common_flag_init(CommonData&) {}
void common_reset_remap_input(CommonData&) {}
void common_set_size(CommonData&, int, int) {}

bool common_tick_frame(const CommonData&) { return false; }
bool common_tick_next_frame(const CommonData&) { return false; }
float common_tick_interpola(const CommonData&) { return 0.0f; }
bool common_add_frame_time(CommonData&, float) { return false; }
void common_reset_frame_time(CommonData&) {}

bool common_match_over(const CommonData&) { return false; }
int common_next_line(int, const std::string&) { return 0; }
std::vector<std::string> common_split_lines(const std::string&) { return {}; }
double common_atof(const std::string&) { return 0.0; }
int common_atoi(const std::string&) { return 0; }

std::string common_load_text(const std::string&, bool) { return {}; }
std::string common_read_file_name(const std::string&, bool) { return {}; }
std::string common_load_file(const std::string&, std::string&, void*) { return {}; }

// ── No-arg convenience wrappers (internal static CommonData) ──
// These are called by the SSZ bridge (Common* wrappers in bridge.cpp).
// Currently stubs — they will be wired when module state integration is active.

static CommonData g_common_state;

void common_flag_init() { common_flag_init(g_common_state); }
void common_reset_remap_input() { common_reset_remap_input(g_common_state); }
void common_set_size(int w, int h) { common_set_size(g_common_state, w, h); }
bool common_tick_frame() { return common_tick_frame(g_common_state); }
bool common_tick_next_frame() { return common_tick_next_frame(g_common_state); }
float common_tick_interpola() { return common_tick_interpola(g_common_state); }
bool common_add_frame_time(float t) { return common_add_frame_time(g_common_state, t); }
void common_reset_frame_time() { common_reset_frame_time(g_common_state); }
bool common_match_over() { return common_match_over(g_common_state); }

} // namespace ikemen::ssz_native
