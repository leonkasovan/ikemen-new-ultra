
#define SDL_MAIN_HANDLED
#include <winsock2.h>        // must precede <windows.h> pulled in by sszdef.h
#include "sszdef.h"
#include "commandline.hpp"
#include "mem_profiler.hpp"
#include "ssz_static.hpp"
#include "lua_static.hpp"
#include "mesdialog_static.hpp"
#include "ogg_static.hpp"
#include "sdlplugin_static.hpp"
#include "alert_static.hpp"
#include "file_static.hpp"
#include "math_static.hpp"
#include "regex_static.hpp"
#include "shell_static.hpp"
#include "socket_static.hpp"
#include "sound_static.hpp"
#include "thread_static.hpp"
#include "time_static.hpp"

#include "../script/ssz/ikemen.hpp"

// sszrefnewfunc / sszrefdeletefunc are defined in ssz.cpp and
// declared extern in sszdef.h — no per-TU definition needed here.


#include "typeid.h"
#include "arrayandref.hpp"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#define SSZ_CORE
#include "pluginutil.hpp"
#undef SSZ_CORE

// =========================================================================
//  Exit safety nets: VEH (crashes) + TLS callback (ExitProcess)
// =========================================================================

// Guard to prevent double-printing when multiple exit paths fire
// (e.g. atexit + TLS DLL_PROCESS_DETACH on normal process termination).
static bool g_memRankingPrinted = false;

static void SafePrintRanking()
{
	if (!g_memRankingPrinted)
	{
		g_memRankingPrinted = true;
		MemPrintRanking();
	}
}

// Vectored Exception Handler — prints memory ranking on crashes
// (access violations, div by zero, etc.) before the process terminates.
static LONG WINAPI MemProfilerVEH(PEXCEPTION_POINTERS /*ep*/)
{
	SafePrintRanking();
	return EXCEPTION_CONTINUE_SEARCH;
}

// TLS callback — fires on DLL_PROCESS_DETACH which catches direct
// ExitProcess() calls that bypass the CRT's atexit mechanism.
// Safe to call here: MemPrintRanking uses only std::sort + printf.
static void NTAPI MemProfilerTLS(PVOID /*h*/, DWORD reason, PVOID /*reserved*/)
{
	if (reason == DLL_PROCESS_DETACH)
	{
		SafePrintRanking();
	}
}

// Place the TLS callback pointer in the CRT's TLS callback array
// (.CRT$XLY section) so it is iterated during DLL_PROCESS_DETACH.
// GCC/MinGW attribute syntax — no #pragma needed.
// MinGW's CRT already defines _tls_used, so no explicit /INCLUDE necessary.
PIMAGE_TLS_CALLBACK _tls_callback
	__attribute__((section(".CRT$XLY"), used)) = MemProfilerTLS;

// Declare the exported Run function so it can be linked directly
extern "C" bool SSZ_STDCALL Run(PluginUtil* pu, Reference r);

// Build a Reference from a narrow string.
// Calls sszrefnewfunc directly — avoids Reference::refnew ODR ambiguity
// (the linker may pick a copy from another TU that references a different
//  sszrefnewfunc, causing a null-pointer crash inside refnew).
static Reference makeScriptRef(const char* path)
{
	Reference ref;
	ref.init();
	int wlen = MultiByteToWideChar(CP_ACP, 0, path, -1, nullptr, 0);
	if (wlen <= 0) return ref;
	wlen--; // exclude NUL terminator
	intptr_t bytes = wlen * (intptr_t)sizeof(WCHR);
	HeapObj* obj = (HeapObj*)sszrefnewfunc(sizeof(HeapObjHead) + bytes);
	if (!obj) return ref;
	obj->head.data      = obj->body.data;
	obj->head.datasize  = bytes;
	obj->head.mutex     = nullptr;
	obj->head.refcount  = 1;
	MultiByteToWideChar(CP_ACP, 0, path, -1, (wchar_t*)obj->body.data, wlen);
	ref.pointer  = obj;
	ref.position = 0;
	ref.length   = bytes;
	return ref;
}

int main(int argc, char *argv[]) {
	AddVectoredExceptionHandler(1, MemProfilerVEH);
	atexit(SafePrintRanking);
	setlocale(LC_CTYPE, "en_US.UTF-8");
	PluginUtil pu(nullptr, nullptr);//Dummy
	CommandLineString<WCHR> cmdline;
#ifdef _WIN32
	cmdline.set(GetCommandLineW());
	SetDllDirectoryW(L"lib/external"); //Change dir where external dlls are loaded.
#else
	std::vector<std::WSTR> arg;
	while(argc--) arg.push_back(pu.aToW(*argv++));
	cmdline.swap(arg);
#endif

	Reference ref = makeScriptRef(argc >= 2 ? argv[1] : "ssz/ikemen.ssz");
	LOG_INFO("Ikemen", "Running Script: %s", argc >= 2 ? argv[1] : "ssz/ikemen.ssz");

	LOG_DEBUG("SSZ", "=== I.K.E.M.E.N. Plus Ultra startup ===");
	LOG_DEBUG("SSZ", "Registering static plugins...");
	if (!ssz_static_register()) {
		LOG_INFO("Ikemen", "Failed to register SSZ functions");
		return 1;
	}
	if (!lua_static_register()) {
		LOG_INFO("Ikemen", "Failed to register Lua functions");
		return 1;
	}
	if (!mesdialog_static_register()) {
		LOG_INFO("Ikemen", "Failed to register MesDialog functions");
		return 1;
	}
	if (!ogg_static_register()) {
		LOG_INFO("Ikemen", "Failed to register Ogg functions");
		return 1;
	}
	if (!sdlplugin_static_register()) {
		LOG_INFO("Ikemen", "Failed to register SDL plugin functions");
		return 1;
	}
	if (!alert_static_register()) {
		LOG_INFO("Ikemen", "Failed to register Alert functions");
		return 1;
	}
	if (!file_static_register()) {
		LOG_INFO("Ikemen", "Failed to register File functions");
		return 1;
	}
	if (!math_static_register()) {
		LOG_INFO("Ikemen", "Failed to register Math functions");
		return 1;
	}
	if (!regex_static_register()) {
		LOG_INFO("Ikemen", "Failed to register Regex functions");
		return 1;
	}
	if (!shell_static_register()) {
		LOG_INFO("Ikemen", "Failed to register Shell functions");
		return 1;
	}
	if (!socket_static_register()) {
		LOG_INFO("Ikemen", "Failed to register Socket functions");
		return 1;
	}
	if (!sound_static_register()) {
		LOG_INFO("Ikemen", "Failed to register Sound functions");
		return 1;
	}
	if (!thread_static_register()) {
		LOG_INFO("Ikemen", "Failed to register Thread functions");
		return 1;
	}
	if (!time_static_register()) {
		LOG_INFO("Ikemen", "Failed to register Time functions");
		return 1;
	}
	LOG_DEBUG("SSZ", "All static plugins registered successfully");

#ifdef IKEMEN_NATIVE_BOOT
	// ── Native C++ boot path (no SSZ JIT) ──────────────────────────
	LOG_INFO("Ikemen", "Starting native C++ engine...");
	int result = ikemen::ikemenMain();
	LOG_INFO("Ikemen", "Engine exited with code %d", result);
	ref.releaseanddelete();
	return result;
#else
	// ── SSZ JIT boot path ──────────────────────────────────────────
	// Run() compiles and executes in the same ssz.cpp TU — all internal
	// sszrefnewfunc/wstrToRef calls are consistent within that translation unit.
	LOG_DEBUG("SSZ", "Starting Run()...");
	if (!Run(&pu, ref)) {
		LOG_INFO("Ikemen", "Script failed");
		LOG_DEBUG("SSZ", "Run() FAILED");
		ref.releaseanddelete();
		return 1;
	}
	LOG_DEBUG("SSZ", "Run() completed successfully");
#endif

	ref.releaseanddelete();
	return 0;
}
