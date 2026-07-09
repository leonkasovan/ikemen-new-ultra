// native_main.hpp — C++ boot sequence replacing ssz/ikemen.ssz main()
//
// When IKEMEN_NATIVE_ENTRY=1, main.cpp calls native_main() instead of
// Run(L"ssz/ikemen.ssz").  This provides the same boot sequence entirely
// in C++ without invoking the SSZ JIT compiler:
//
//   1. SDL window init (sdl_init, fullScreenMode, etc.)
//   2. Lua state creation + system_script_init (registers 300+ Lua callbacks)
//   3. Register 4 Lua callbacks: loadStart, selectStart, game, sszReload
//   4. Call common_flag_init, common_set_size, common_reset_remap_input
//   5. L.runFile(cfg.system) — drives the main Lua system script
//   6. Cleanup and shutdown
//
// Dependencies:
//   - All native service modules (common_service, fighting_service, etc.)
//   - Lua C API (lauxlib.h, lualib.h)
//   - SDL2 via sdlplugin_service
//
// Build with: make IKEMEN_NATIVE_ENTRY=1

#pragma once

namespace ikemen::ssz_native {

/// Run the native boot sequence, replacing ssz/ikemen.ssz.
/// Returns true on success, false if initialization failed.
bool native_main(int argc, char* argv[]);

} // namespace ikemen::ssz_native
