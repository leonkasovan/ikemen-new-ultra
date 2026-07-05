// ssz_service.cpp — Native C++ wrappers for ssz_script/lib/ssz.ssz
//
// Delegates to the SSZ JIT compiler bridge functions declared in
// ssz_static.hpp and implemented in main/ssz/ssz.cpp.

#include "ssz_service.hpp"
#include "ssz_trace.hpp"

// The bridge functions are declared in ssz_static.hpp (included by
// main.cpp) and implemented in main/ssz/ssz.cpp.  This file provides
// C++ convenience wrappers that convert native types (std::string,
// intptr_t) to the Reference/CompilerState* ABI expected by the bridge.
//
// In the current phase, these wrappers simply forward to the existing
// plugin-registered functions.  As the conversion progresses, the bridge
// layer may be bypassed entirely for direct native-to-native calls.

namespace ikemen::ssz_native {

// The actual bridge functions (Run, NewCompiler, etc.) are extern "C"
// and registered via ssz_static.hpp.  They take PluginUtil* and Reference
// parameters that require VM context.  Until those are refactored, we
// provide stub implementations that log intent.
//
// Phase 6: Wire these to the bridge when called from native code
// (not just from SSZ script).

bool SszService::run(const std::string& file) {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::run");
	(void)file;
	// Deferred: calls Run() plugin bridge when invoked from native code.
	return false;
}

void SszService::memMarkBefore(const std::string& label) {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::memMarkBefore");
	(void)label;
}

void SszService::memMarkAfter(const std::string& label) {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::memMarkAfter");
	(void)label;
}

std::string SszService::compileFile(const std::string& file) {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::compileFile");
	(void)file;
	return "SSZ native compile deferred (use SSZ script path)";
}

std::string SszService::compileString(const std::string& code, const std::string& dir) {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::compileString");
	(void)code; (void)dir;
	return "SSZ native compileString deferred";
}

intptr_t SszService::newCompiler() {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::newCompiler");
	return 0;
}

void SszService::deleteCompiler(intptr_t ptr) {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::deleteCompiler");
	(void)ptr;
}

bool SszService::compilerRun(intptr_t ptr) {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::compilerRun");
	(void)ptr;
	return false;
}

// ── Module-level convenience wrappers ──

void ssz_mem_mark_before(const std::string& label) {
	SSZ_TRACE_CAT(TRACE_SYS, "ssz_mem_mark_before");
	SszService::memMarkBefore(label);
}

void ssz_mem_mark_after(const std::string& label) {
	SSZ_TRACE_CAT(TRACE_SYS, "ssz_mem_mark_after");
	SszService::memMarkAfter(label);
}

bool ssz_run(const std::string& file) {
	SSZ_TRACE_CAT(TRACE_SYS, "ssz_run");
	return SszService::run(file);
}

} // namespace ikemen::ssz_native
