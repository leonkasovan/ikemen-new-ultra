#include "sszdef.h"

void SSZ_STDCALL Alert(const std::wstring& title, const std::wstring& mes)
{
#if defined(_WIN32) && defined(NDEBUG)
	// MessageBox only in Release builds (_WIN32 + NDEBUG); Debug builds
	// print to stderr so logs are captured during development.
	MessageBoxW(NULL, mes.c_str(), title.c_str(), MB_OK | MB_ICONWARNING);
#else
	fprintf(
		stderr, "%ls\n%ls\n",
		title.c_str(), mes.c_str());
#endif
}
