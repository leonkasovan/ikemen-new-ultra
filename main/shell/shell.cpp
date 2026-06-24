
#include <windows.h>
#include <process.h>
#include <stdint.h>


#include "sszdef.h"
#include "typeid.h"
#include "arrayandref.hpp"
#include "pluginutil.hpp"

extern "C" bool SSZ_STDCALL ShellOpen(PluginUtil* pu, bool act, bool wait, Reference direct, Reference param, Reference file)
{
	SHELLEXECUTEINFO sei;
	std::wstring f = pu->refToWstr(file);
	std::wstring p = pu->refToWstr(param);
	std::wstring d = pu->refToWstr(direct);
	sei.cbSize		 = sizeof(sei);
	sei.fMask		 = wait ? SEE_MASK_NOCLOSEPROCESS : 0;
	sei.hwnd		 = 0;
	sei.lpVerb		 = L"open";
	sei.lpFile		 = f.c_str();
	sei.lpParameters = p.c_str();
	sei.lpDirectory  = d.c_str();
	sei.nShow		 = (act ? SW_NORMAL : SW_SHOWMINNOACTIVE);
	if(ShellExecuteEx(&sei)){
		if(wait) WaitForSingleObject(sei.hProcess, INFINITE);
	}else{
		return false;
	}
	return true;
}

extern "C" bool SSZ_STDCALL MoveTrash(PluginUtil* pu, Reference file)
{
	std::wstring f = pu->refToWstr(file);
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

