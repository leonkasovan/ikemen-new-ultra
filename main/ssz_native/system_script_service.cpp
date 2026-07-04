// system_script_service.cpp — Stub for system-script.ssz scaffolding.

#include "system_script_service.hpp"

namespace ikemen::ssz_native {

void system_script_init(lua_State*) {
	// Phase 3: register all system-level callbacks here
}

void system_script_init() {
	system_script_init(nullptr);
}

} // namespace ikemen::ssz_native
