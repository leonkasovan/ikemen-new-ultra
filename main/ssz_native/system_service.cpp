// system_service.cpp — Static backing for system.ssz native bridge.
//
// system_service.hpp is header-only.  This file provides the static
// SystemData instance and no-arg convenience functions called by the
// SSZ bridge wrappers in bridge.cpp.
//
// Phase 3: All wrapper bodies are stubs.  Wired when dependent modules
// (file, sff, string, common, action, fight) are converted.

#include "system_service.hpp"

namespace ikemen::ssz_native {

// ── Internal static state ──
static SelectData g_select_state;
static SystemData g_system_state;

// Constructor-order init: wire the Select pointer before any bridge call.
// (Simpler than lazy init — g_select_state is trivially constructed.)
static bool g_system_inited = []{
	g_system_state.selinf.sel = &g_select_state;
	return true;
}();

// ── No-arg convenience functions for bridge/SSZ ABI ──
// These operate on g_system_state and are called by the bridge wrappers.

void system_add_char() { (void)g_system_inited; g_system_state.selinf.sel->addChar(""); }
void system_add_stage() { (void)g_system_inited; g_system_state.selinf.sel->addStage(""); }
int  system_set_stage_no(int i) { (void)g_system_inited; return g_system_state.selinf.sel->setStageNo(i); }
void system_select_stage(int no) { (void)g_system_inited; g_system_state.selinf.sel->selectStage(no); }
bool system_add_selchr(int pn, int cn, int pl) { (void)g_system_inited; return g_system_state.selinf.addSelchr(pn, cn, pl); }
void system_sel_reset() { (void)g_system_inited; g_system_state.selReset(); }

} // namespace ikemen::ssz_native
