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

// ── Parameterized functions ──
// These receive the def file path from the bridge and perform real work.

bool system_add_char(const std::string& defPath) {
	return g_system_state.selinf.sel->addChar(defPath);
}

std::string system_add_stage(const std::string& defPath) {
	return g_system_state.selinf.sel->addStage(defPath);
}

std::string system_get_stage_name(int i) {
	return g_system_state.selinf.sel->getStageName(i);
}

std::string system_get_selected_stage_def() {
	auto* sel = g_system_state.selinf.sel;
	if (!sel) return {};
	int no = sel->selectedStageNo;
	if (no < 0 || no == 0) return {}; // RANDOM or not set
	int idx = no - 1;
	if (idx >= 0 && idx < static_cast<int>(sel->stagelist.size()))
		return sel->stagelist[idx].def;
	return {};
}

int system_get_selected_stage_no() {
	auto* sel = g_system_state.selinf.sel;
	return sel ? sel->selectedStageNo : -1;
}

std::string system_get_selected_char_def(int pn) {
	auto* sel = g_system_state.selinf.sel;
	if (!sel) return {};
	if (pn < 0 || pn >= static_cast<int>(g_system_state.selinf.p.size())) return {};
	auto& player = g_system_state.selinf.p[pn];
	if (player.selchr.empty()) return {};
	int idx = player.selchr[0].i;
	if (idx < 0 || idx >= static_cast<int>(sel->charlist.size())) return {};
	return sel->charlist[idx].def;
} } // namespace ikemen::ssz_native
