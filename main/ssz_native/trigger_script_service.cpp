// trigger_script_service.cpp — Stub for trigger-script.ssz scaffolding.
//
// Phase 3: Stub.  When wired, register_function registers all 170+ Lua
// callbacks defined in ssz_script/ssz/trigger-script.ssz.

#include "trigger_script_service.hpp"

namespace ikemen::ssz_native {

void register_function(lua_State*) {
	// Phase 3: register all 170+ trigger callbacks here
}

void register_function() {
	register_function(nullptr);
}

} // namespace ikemen::ssz_native
