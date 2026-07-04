// loader_service.cpp — Stub implementations for loader.ssz scaffolding.
//
// Phase 3: All bodies are no-ops.  Wired when dependent modules are native.

#include "loader_service.hpp"

namespace ikemen::ssz_native {

void loader_error(const std::string&) {}

bool loader_stage() { return false; }

int loader_chara(int) { return 0; }

bool loader_state_compile() { return false; }

void loader_load() {}

void loader_reset() {}

bool loader_run_tread() { return false; }

// ── No-arg convenience wrappers (bridge/SSZ ABI) ──
// Called by the SSZ bridge (Loader* wrappers in bridge.cpp).
// Currently stubs — they delegate to the parameter-taking versions with defaults.

void loader_error() { loader_error(""); }
int  loader_chara() { return loader_chara(0); }

} // namespace ikemen::ssz_native
