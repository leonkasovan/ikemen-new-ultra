
#define SDL_MAIN_HANDLED
#include <io.h>
#include <winsock2.h>        // must precede <windows.h> pulled in by sszdef.h
#include "sszdef.h"
#include "commandline.hpp"
#include "mem_profiler.hpp"
#include "ssz/ssz_plugin.hpp"
#include "lua/lua_plugin.hpp"
#include "mesdialog/mesdialog_plugin.hpp"
#include "ogg/ogg_plugin.hpp"
#include "sdlplugin/sdlplugin_plugin.hpp"
#include "alert/alert_plugin.hpp"
#include "file/file_plugin.hpp"
#include "math/math_plugin.hpp"
#include "regex/regex_plugin.hpp"
#include "shell/shell_plugin.hpp"
#include "socket/socket_plugin.hpp"
#include "sound/sound_plugin.hpp"
#include "thread/thread_plugin.hpp"
#include "time/time_plugin.hpp"

// Native (C++) SSZ libraries (e.g. ssz_script/lib/time.cpp).
extern "C" bool time_lib_register();
extern "C" bool shell_lib_register();
extern "C" bool thread_lib_register();
extern "C" bool math_lib_register();
extern "C" bool string_lib_register();
extern "C" bool md5_lib_register();
extern "C" bool arcfour_lib_register();
extern "C" bool file_lib_register();
extern "C" bool regex_lib_register();
extern "C" bool sound_lib_register();
extern "C" bool ssz_lib_register();
extern "C" bool socket_lib_register();
extern "C" bool base64_lib_register();
extern "C" bool table_lib_register();
extern "C" bool sdlplugin_lib_register();
extern "C" bool sdlevent_lib_register();


#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

// Required for complete Reference type in extern "C" bridge declarations
// (static plugin registration headers use Reference by value for __stdcall decoration).
#include "arrayandref.hpp"

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

// Forward declaration of native Run — defined in ssz.cpp
bool Run(const std::wstring& scriptPath);

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
	CommandLineString<WCHR> cmdline;
#ifdef _WIN32
	cmdline.set(GetCommandLineW());
	SetDllDirectoryW(L"lib/external"); //Change dir where external dlls are loaded.
#else
	std::vector<std::WSTR> arg;
	for (int i = 0; i < argc; i++) {
		std::string s(argv[i]);
		std::wstring ws(s.begin(), s.end());
		arg.push_back(ws);
	}
	cmdline.swap(arg);
#endif

	std::wstring scriptPath = cmdline.get().size() > 1
		? cmdline.get()[1]
		: L"ssz/ikemen.ssz";
	LOG_INFO("Ikemen", "Running Script: %ls", scriptPath.c_str());

	LOG_DEBUG("SSZ", "=== I.K.E.M.E.N. Plus Ultra startup ===");
	LOG_DEBUG("SSZ", "Registering static plugins...");
	if (!ssz_plugin_register()) {
		LOG_INFO("Ikemen", "Failed to register SSZ functions");
		return 1;
	}
	if (!lua_plugin_register()) {
		LOG_INFO("Ikemen", "Failed to register Lua functions");
		return 1;
	}
	if (!mesdialog_plugin_register()) {
		LOG_INFO("Ikemen", "Failed to register MesDialog functions");
		return 1;
	}
	if (!ogg_plugin_register()) {
		LOG_INFO("Ikemen", "Failed to register Ogg functions");
		return 1;
	}
	if (!sdlplugin_plugin_register()) {
		LOG_INFO("Ikemen", "Failed to register SDL plugin functions");
		return 1;
	}
	if (!alert_plugin_register()) {
		LOG_INFO("Ikemen", "Failed to register Alert functions");
		return 1;
	}
	if (!file_plugin_register()) {
		LOG_INFO("Ikemen", "Failed to register File functions");
		return 1;
	}
	if (!math_plugin_register()) {
		LOG_INFO("Ikemen", "Failed to register Math functions");
		return 1;
	}
	if (!regex_plugin_register()) {
		LOG_INFO("Ikemen", "Failed to register Regex functions");
		return 1;
	}
	if (!shell_plugin_register()) {
		LOG_INFO("Ikemen", "Failed to register Shell functions");
		return 1;
	}
	if (!socket_plugin_register()) {
		LOG_INFO("Ikemen", "Failed to register Socket functions");
		return 1;
	}
	if (!sound_plugin_register()) {
		LOG_INFO("Ikemen", "Failed to register Sound functions");
		return 1;
	}
	if (!thread_plugin_register()) {
		LOG_INFO("Ikemen", "Failed to register Thread functions");
		return 1;
	}
	if (!time_plugin_register()) {
		LOG_INFO("Ikemen", "Failed to register Time functions");
		return 1;
	}
	LOG_DEBUG("SSZ", "All static plugins registered successfully");

	// ── Native (C++) SSZ libraries ────────────────────────────────
	// Registered into the native-lib registry (main/ssz/native_lib.hpp)
	// so `lib name = <name>` resolves to C++ code (e.g. ssz_script/lib/time.cpp).
	if (!time_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'time'");
		return 1;
	}
	if (!shell_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'shell'");
		return 1;
	}
	if (!thread_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'thread'");
		return 1;
	}
	if (!math_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'math'");
		return 1;
	}
	if (!string_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'string'");
		return 1;
	}
	if (!md5_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'md5'");
		return 1;
	}
	if (!arcfour_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'arcfour'");
		return 1;
	}
	if (!file_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'file'");
		return 1;
	}
	if (!regex_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'regex'");
		return 1;
	}
	if (!sound_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'sound'");
		return 1;
	}
	if (!ssz_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'ssz'");
		return 1;
	}
	if (!socket_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'socket'");
		return 1;
	}
	if (!base64_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'base64'");
		return 1;
	}
	if (!table_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'table'");
		return 1;
	}
	if (!sdlplugin_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'sdlplugin'");
		return 1;
	}
	if (!sdlevent_lib_register()) {
		LOG_INFO("Ikemen", "Failed to register native SSZ library 'sdlevent'");
		return 1;
	}
	LOG_DEBUG("SSZ", "Native SSZ libraries registered successfully");

	updateCharInSelectDef("data/select.def");
	updateStageInSelectDef("data/select.def");

	// ── SSZ JIT boot path ──────────────────────────────────────────
	// Run() compiles and executes via the native ABI.
	LOG_DEBUG("SSZ", "Starting Run()...");
	if (!Run(scriptPath)) {
		LOG_INFO("Ikemen", "Script failed");
		LOG_DEBUG("SSZ", "Run() FAILED");
		return 1;
	}
	LOG_DEBUG("SSZ", "Run() completed successfully");

	return 0;
}
