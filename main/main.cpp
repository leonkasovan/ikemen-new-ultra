
#define SDL_MAIN_HANDLED
#include <io.h>
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

static bool stringInSlice(const std::vector<std::string>& slice, const std::string& s)
{
	for (size_t i = 0; i < slice.size(); i++) {
		if (slice[i] == s) return true;
	}
	return false;
}

// Trim leading and trailing whitespace from a string.
static std::string trimStr(const std::string& s)
{
	size_t start = s.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) return "";
	size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end - start + 1);
}

// Convert a string to lowercase (returns a copy).
static std::string toLower(std::string s)
{
	for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
	return s;
}

// ── updateCharInSelectDef ─────────────────────────────────────────────────
// Update [Characters] section in select.def based on chars/ directory.
// Directories found in chars/ that are not already listed get appended
// with ", random" before the [ExtraStages] section.

static int updateCharInSelectDef(const char* fname)
{
	FILE* file = fopen(fname, "r");
	if (!file) {
		printf("Failed to open file %s\n", fname);
		return 1;
	}

	std::string updateName = std::string(fname) + ".update";
	FILE* file2 = fopen(updateName.c_str(), "w");
	if (!file2) {
		printf("Failed to create update file\n");
		fclose(file);
		return 1;
	}

	std::vector<std::string> chars;
	int section = 0;
	char buf[4096];

	while (fgets(buf, sizeof(buf), file)) {
		std::string originalLine = buf;
		// Strip trailing newline/cr
		while (!originalLine.empty() &&
			(originalLine.back() == '\n' || originalLine.back() == '\r'))
			originalLine.pop_back();

		std::string trimmed = trimStr(originalLine);
		std::string lowerTrimmed = toLower(trimmed);

		// 1. Handle comments and empty lines
		if (trimmed.empty() || trimmed[0] == ';') {
			fprintf(file2, "%s\n", originalLine.c_str());
			continue;
		}

		// 2. Identify [characters] section
		if (lowerTrimmed.find("[characters]") != std::string::npos) {
			section = 1;
			fprintf(file2, "%s\n", originalLine.c_str());
			continue;
		}

		// 3. Identify [extrastages] section — inject new chars before it
		if (lowerTrimmed.find("[extrastages]") != std::string::npos) {
			// Enumerate directories in chars/
			struct _finddata_t fd;
			intptr_t handle = _findfirst("chars/*", &fd);
			if (handle != -1) {
				do {
					if (fd.attrib & _A_SUBDIR) {
						const char* dirName = fd.name;
						if (strcmp(dirName, ".") != 0 && strcmp(dirName, "..") != 0) {
							if (!stringInSlice(chars, dirName)) {
								printf(" add new char: %s\n", dirName);
								fprintf(file2, "%s\n", dirName);
							}
						}
					}
				} while (_findnext(handle, &fd) == 0);
				_findclose(handle);
			}
			section = 2;
			fprintf(file2, "%s\n", originalLine.c_str());
			continue;
		}

		// 4. Process character entries in section 1
		if (section == 1) {
			// Extract the name (text before the first comma)
			size_t commaPos = trimmed.find(',');
			std::string charName = (commaPos != std::string::npos)
				? trimStr(trimmed.substr(0, commaPos))
				: trimmed;
			if (!charName.empty()) {
				// Check if the char directory actually exists
				std::string charDir = std::string("chars/") + charName;
				if (_access(charDir.c_str(), 0) == 0) {
					chars.push_back(charName);
					printf(" existing char: %s\n", charName.c_str());
					fprintf(file2, "%s\n", originalLine.c_str());
				} else {
					printf(" remove missing char: %s\n", charName.c_str());
				}
				continue;
			}
		}

		fprintf(file2, "%s\n", originalLine.c_str());
	}

	fclose(file2);
	fclose(file);

	// Atomic rename (remove old .bak first — Windows rename doesn't overwrite)
	std::string bakName = std::string(fname) + ".bak";
	remove(bakName.c_str());
	rename(fname, bakName.c_str());
	rename(updateName.c_str(), fname);

	return 0;
}

// ── updateStageInSelectDef ────────────────────────────────────────────────
// Update [ExtraStages] section in select.def based on *.def files in stages/
// directory. Stage paths not already listed get appended before [Options].

static int updateStageInSelectDef(const char* fname)
{
	FILE* file = fopen(fname, "r");
	if (!file) {
		printf("Failed to open: %s\n", fname);
		return 1;
	}

	std::string updateName = std::string(fname) + ".update";
	FILE* file2 = fopen(updateName.c_str(), "w");
	if (!file2) {
		printf("Failed to create update file\n");
		fclose(file);
		return 1;
	}

	std::vector<std::string> stages;
	int section = 0;
	char pathSep1 = 0;
	char pathSep2 = 0;
	char buf[4096];

	while (fgets(buf, sizeof(buf), file)) {
		std::string originalLine = buf;
		while (!originalLine.empty() &&
			(originalLine.back() == '\n' || originalLine.back() == '\r'))
			originalLine.pop_back();

		std::string trimmed = trimStr(originalLine);
		std::string lowerTrimmed = toLower(trimmed);

		if (trimmed.empty() || trimmed[0] == ';') {
			fprintf(file2, "%s\n", originalLine.c_str());
			continue;
		}

		// Identify [extrastages] section
		if (lowerTrimmed.find("[extrastages]") != std::string::npos) {
			section = 2;
			fprintf(file2, "%s\n", originalLine.c_str());
			continue;
		}

		// Identify [options] section — inject new stages before it
		if (lowerTrimmed.find("[options]") != std::string::npos) {
			// Enumerate *.def files in stages/
			struct _finddata_t fd;
			intptr_t handle = _findfirst("stages/*.def", &fd);
			if (handle != -1) {
				do {
					if (!(fd.attrib & _A_SUBDIR)) {
						std::string stagePath = std::string("stages/") + fd.name;
						// Normalize path separators to match existing style
						if (pathSep1) {
							for (size_t i = 0; i < stagePath.size(); i++) {
								if (stagePath[i] == pathSep2) stagePath[i] = pathSep1;
							}
						}
						if (!stringInSlice(stages, stagePath)) {
							printf(" add new stage: %s\n", stagePath.c_str());
							fprintf(file2, "%s\n", stagePath.c_str());
						}
					}
				} while (_findnext(handle, &fd) == 0);
				_findclose(handle);
			}
			section = 3;
			fprintf(file2, "%s\n", originalLine.c_str());
			continue;
		}

		// Collect stage entries in section 2
		if (section == 2) {
			std::string stagePath = trimStr(originalLine);

			// Check if the stage file actually exists
			if (_access(stagePath.c_str(), 0) == 0) {
				stages.push_back(stagePath);
				printf(" existing stage: %s\n", stagePath.c_str());

				// Detect path separator style
				if (pathSep1 == 0) {
					if (stagePath.find('/') != std::string::npos) {
						pathSep1 = '/';
						pathSep2 = '\\';
					} else if (stagePath.find('\\') != std::string::npos) {
						pathSep1 = '\\';
						pathSep2 = '/';
					}
				}
				fprintf(file2, "%s\n", originalLine.c_str());
			} else {
				printf(" remove missing stage: %s\n", stagePath.c_str());
			}
			continue;
		}

		fprintf(file2, "%s\n", originalLine.c_str());
	}

	fclose(file2);
	fclose(file);

	// Atomic rename (remove old .bak first — Windows rename doesn't overwrite)
	std::string bakName = std::string(fname) + ".bak";
	remove(bakName.c_str());
	rename(fname, bakName.c_str());
	rename(updateName.c_str(), fname);

	return 0;
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

	updateCharInSelectDef("data/select.def");
	updateStageInSelectDef("data/select.def");

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

	ref.releaseanddelete();
	return 0;
}
