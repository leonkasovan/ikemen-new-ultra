// ssz_service.hpp — Native C++ wrapping of ssz_script/lib/ssz.ssz
//
// lib/ssz.ssz (24 lines) implements the SSZ JIT compiler interface —
// memory marking, script compilation, and execution. The native
// implementation lives in main/ssz/ (jitcompiler.hpp, sourcetree.hpp,
// x86.hpp, etc.) and is registered via ssz_static.hpp.
//
// This file provides a type-safe C++ convenience layer on top of the
// extern "C" bridge functions declared in ssz_static.hpp.

#pragma once

#include <string>

namespace ikemen::ssz_native {

// ── ssz_service ──
// Wraps the SSZ JIT compiler ABI for use by other native services.
// All functions delegate to the static plugin table entries registered
// in ssz_static.hpp.
struct SszService {

	/// Run a script file. Returns true on success.
	static bool run(const std::string& file);

	/// Memory marking — label the start of a memory region.
	static void memMarkBefore(const std::string& label);

	/// Memory marking — label the end of a memory region.
	static void memMarkAfter(const std::string& label);

	/// Compile a single .ssz file. Returns error string (empty = success).
	static std::string compileFile(const std::string& file);

	/// Compile a code string. Returns error string (empty = success).
	static std::string compileString(const std::string& code, const std::string& dir);

	/// Create a new compiler instance. Returns opaque handle.
	static intptr_t newCompiler();

	/// Destroy a compiler instance.
	static void deleteCompiler(intptr_t ptr);

	/// Run compiled code via a compiler instance. Returns true on success.
	static bool compilerRun(intptr_t ptr);
};

// Module-level convenience wrappers matching lib/ssz.ssz public API.
void ssz_mem_mark_before(const std::string& label);
void ssz_mem_mark_after(const std::string& label);
bool ssz_run(const std::string& file);

} // namespace ikemen::ssz_native
