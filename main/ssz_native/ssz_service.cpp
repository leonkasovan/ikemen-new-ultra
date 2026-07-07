// ssz_service.cpp — Native C++ wrappers for ssz_script/lib/ssz.ssz
//
// Delegates to the SSZ JIT compiler native entry points defined in
// main/ssz/ssz.cpp (not the extern "C" bridge wrappers — those take
// PluginUtil* and Reference).  The native entry points accept plain
// std::wstring or CompilerState* and can be called from any C++ code.

#include "ssz_service.hpp"
#include "ssz_trace.hpp"

#include <string>

// SSZ_STDCALL definition (lightweight header, ~150 lines)
#include "../ssz/sszdef.h"

// Forward declaration only — ssz_service never calls CompilerState methods
// directly. All operations go through standalone native entry points.
class CompilerState;

// =========================================================================
// Native SSZ compiler entry points (defined in main/ssz/ssz.cpp)
//
// These are C++ functions (NOT extern "C"), with C++ name mangling.
// ssz_service.cpp and ssz.cpp are linked into the same binary, so we
// just need matching declarations.
// =========================================================================

// MemMarkBefore / MemMarkAfter — memory snapshot markers (defined in ssz.cpp)
void SSZ_STDCALL MemMarkBefore(const std::wstring& wtag);
void SSZ_STDCALL MemMarkAfter(const std::wstring& wtag);

// Run — compile and run a script file (defined in ssz.cpp)
bool SSZ_STDCALL Run(const std::wstring& scriptPath);

// Compiler lifecycle (defined in ssz.cpp)
CompilerState* SSZ_STDCALL NewCompiler();
void           SSZ_STDCALL DeleteCompiler(CompilerState* cs);

// Compilation (defined in ssz.cpp)
std::wstring SSZ_STDCALL CompilerCompile(
    const std::wstring& file, CompilerState* cs);
std::wstring SSZ_STDCALL CompilerCompileString(
    const std::wstring& code, const std::wstring& dir, CompilerState* cs);

// Execution (defined in ssz.cpp)
bool SSZ_STDCALL CompilerRun(CompilerState* cs);

// =========================================================================
// Helpers
// =========================================================================
namespace {

std::wstring to_wstring(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

std::string to_string(const std::wstring& ws) {
    return std::string(ws.begin(), ws.end());
}

} // anonymous namespace

namespace ikemen::ssz_native {

// =========================================================================
// SszService — type-safe wrappers around the native compiler entry points
// =========================================================================

bool SszService::run(const std::string& file) {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::run");
	return Run(to_wstring(file));
}

void SszService::memMarkBefore(const std::string& label) {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::memMarkBefore");
	MemMarkBefore(to_wstring(label));
}

void SszService::memMarkAfter(const std::string& label) {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::memMarkAfter");
	MemMarkAfter(to_wstring(label));
}

std::string SszService::compileFile(const std::string& file) {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::compileFile");
	CompilerState* cs = NewCompiler();
	if (!cs) return "Failed to create SSZ compiler instance";
	std::wstring err = CompilerCompile(to_wstring(file), cs);
	DeleteCompiler(cs);
	return to_string(err);
}

std::string SszService::compileString(const std::string& code, const std::string& dir) {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::compileString");
	CompilerState* cs = NewCompiler();
	if (!cs) return "Failed to create SSZ compiler instance";
	std::wstring err = CompilerCompileString(to_wstring(code), to_wstring(dir), cs);
	DeleteCompiler(cs);
	return to_string(err);
}

intptr_t SszService::newCompiler() {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::newCompiler");
	return reinterpret_cast<intptr_t>(NewCompiler());
}

void SszService::deleteCompiler(intptr_t ptr) {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::deleteCompiler");
	if (ptr)
		DeleteCompiler(reinterpret_cast<CompilerState*>(ptr));
}

bool SszService::compilerRun(intptr_t ptr) {
	SSZ_TRACE_CAT(TRACE_SYS, "SszService::compilerRun");
	if (!ptr) return false;
	return CompilerRun(reinterpret_cast<CompilerState*>(ptr));
}

// ── Module-level convenience wrappers ──

void ssz_mem_mark_before(const std::string& label) {
	SszService::memMarkBefore(label);
}

void ssz_mem_mark_after(const std::string& label) {
	SszService::memMarkAfter(label);
}

bool ssz_run(const std::string& file) {
	return SszService::run(file);
}

} // namespace ikemen::ssz_native
