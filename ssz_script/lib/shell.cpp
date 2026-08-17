// ============================================================================
// shell.cpp — native (C++) implementation of the SSZ `shell` library.
//
// Converted from ssz_script/lib/shell.ssz.  Declared in SSZ scripts as:
//
//     lib sh = <shell>;
//
// The module is resolved by the native-lib registry (main/ssz/native_lib.hpp)
// — no shell.ssz file is required.  Every public function from shell.ssz is
// implemented below with the plugin ABI:
//
//     T name(PluginUtil*, ...)   (first arg is the SSZ runtime util; the JIT
//                                 passes g_gpsf.sf and reads the result from
//                                 the return slot)
//
// ABI note: the JIT pushes arguments in SSZ declaration order, so a C++
// function receives them reversed — the last SSZ parameter arrives first
// (the same convention as the plugin bridges in main/ssz/bridge.cpp, e.g.
// ShellOpen).  String parameters arrive as `Reference` values; convert them
// with ikemen::ssz_bridge::refToWstring().
// ============================================================================

#include <string>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include "sszdef.h"
#include "bridge.hpp"
#include "native_lib.hpp"

struct PluginUtil;
struct Reference;

// ---------------------------------------------------------------------------
// Functions (plugin ABI — arguments arrive reversed vs. the SSZ declaration)
// ---------------------------------------------------------------------------

// SSZ: public bool open(^/char file, ^/char arg, ^/char cdir, bool waitfor, bool active)
#ifdef _WIN32
static bool SSZ_STDCALL ShellLibOpen(
	PluginUtil* pu, bool active, bool waitfor,
	Reference cdir, Reference arg, Reference file)
{
	std::wstring wfile = ikemen::ssz_bridge::refToWstring(pu, file);
	std::wstring warg  = ikemen::ssz_bridge::refToWstring(pu, arg);
	std::wstring wdir  = ikemen::ssz_bridge::refToWstring(pu, cdir);

	SHELLEXECUTEINFO sei;
	sei.cbSize       = sizeof(sei);
	sei.fMask        = waitfor ? SEE_MASK_NOCLOSEPROCESS : 0;
	sei.hwnd         = 0;
	sei.lpVerb       = L"open";
	sei.lpFile       = wfile.c_str();
	sei.lpParameters = warg.c_str();
	sei.lpDirectory  = wdir.c_str();
	sei.nShow        = (active ? SW_NORMAL : SW_SHOWMINNOACTIVE);
	if(ShellExecuteEx(&sei)){
		if(waitfor) WaitForSingleObject(sei.hProcess, INFINITE);
		return true;
	}
	return false;
}
#else
static bool SSZ_STDCALL ShellLibOpen(
	PluginUtil* pu, bool active, bool waitfor,
	Reference cdir, Reference arg, Reference file)
{
	(void)pu; (void)active; (void)waitfor; (void)cdir; (void)arg; (void)file;
	return false;
}
#endif

// SSZ: public bool moveToTrash(^/char file)
#ifdef _WIN32
static bool SSZ_STDCALL ShellLibMoveToTrash(PluginUtil* pu, Reference file)
{
	std::wstring f = ikemen::ssz_bridge::refToWstring(pu, file);
	wchar_t* pwc = _wfullpath(NULL, f.c_str(), 0);
	if(pwc == NULL) return false;
	f = pwc;
	free(pwc);
	SHFILEOPSTRUCT sfos;
	ZeroMemory(&sfos, sizeof(SHFILEOPSTRUCT));
	f += L'\0';
	f += L'\0';
	sfos.hwnd = NULL;
	sfos.wFunc = FO_DELETE;
	sfos.pFrom = f.data();
	sfos.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_WANTNUKEWARNING;
	return SHFileOperation(&sfos) == 0;
}
#else
static bool SSZ_STDCALL ShellLibMoveToTrash(PluginUtil* pu, Reference file)
{
	(void)pu; (void)file;
	return false;
}
#endif

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

extern "C" bool shell_lib_register()
{
	static const NativeLib::NativeFunction funcs[] = {
		{ "open",        "bool (^/char, ^/char, ^/char, bool, bool)",
			(void*)ShellLibOpen },
		{ "moveToTrash", "bool (^/char)",
			(void*)ShellLibMoveToTrash },
	};

	NativeLib::NativeLibrary lib;
	lib.name = "shell";
	for(size_t i = 0; i < sizeof(funcs) / sizeof(funcs[0]); i++){
		lib.functions.push_back(funcs[i]);
	}
	return NativeLib::RegisterLibrary(lib);
}
