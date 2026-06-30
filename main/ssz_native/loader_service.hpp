// loader_service.hpp — Native C++ scaffolding for ssz_script/ssz/loader.ssz
//
// loader.ssz implements the loading screen logic — character/stage loading,
// state compilation, and threading.
//
// Phase 3: All function bodies are stubs.  Wired when dependent modules
// (com, chr, sff, stage, statebuilder, system-script, sdlplugin) are at
// least partially native.

#pragma once

#include <string>

namespace ikemen::ssz_native {

// ── State enum ──
// Maps to |State in loader.ssz.
enum class LoaderState {
	NotYet,
	Loading,
	Complete,
	Error,
	Cancel
};

// ── Loader Data ──
// Module-level globals from loader.ssz.
struct LoaderData {
	LoaderState state{LoaderState::NotYet};
	std::string errorMes;

	// sszc: ^&.lua.SszCompiler (opaque — requires Lua/SszCompiler)
	// code: ^^/char[n] per-player compiled state code (opaque array)
};

// ── Public function stubs ──

// error(m) — set error message.
void loader_error(const std::string& m);

// stage() — load the selected stage.  Returns true on success.
bool loader_stage();

// chara(pn) — load character for player slot pn.
// Returns 1 on success, 0 if no character needed, -1 on error.
int loader_chara(int pn);

// stateCompile() — compile per-player state code into sszc.
bool loader_state_compile();

// load() — main loading loop (character/stage loading + compilation).
void loader_load();

// reset() — cancel loading and reset state.
void loader_reset();

// runTread() _ sic — start loading thread.  Returns false if already running.
bool loader_run_tread();

} // namespace ikemen::ssz_native
