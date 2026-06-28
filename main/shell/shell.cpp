#include <windows.h>
#include <process.h>
#include <stdint.h>


#include "sszdef.h"


bool SSZ_STDCALL ShellOpen(bool act, bool wait, const std::wstring& direct, const std::wstring& param, const std::wstring& file)
{
	SHELLEXECUTEINFO sei;
	sei.cbSize		 = sizeof(sei);
	sei.fMask		 = wait ? SEE_MASK_NOCLOSEPROCESS : 0;
	sei.hwnd		 = 0;
	sei.lpVerb		 = L"open";
	sei.lpFile		 = file.c_str();
	sei.lpParameters = param.c_str();
	sei.lpDirectory  = direct.c_str();
	sei.nShow		 = (act ? SW_NORMAL : SW_SHOWMINNOACTIVE);
	if(ShellExecuteEx(&sei)){
		if(wait) WaitForSingleObject(sei.hProcess, INFINITE);
	}else{
		return false;
	}
	return true;
}

bool SSZ_STDCALL MoveTrash(const std::wstring& file)
{
	std::wstring f = file;
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
