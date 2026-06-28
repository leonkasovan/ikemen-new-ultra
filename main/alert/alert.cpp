#include "sszdef.h"

#ifndef _WIN32
#include "pluginutil.hpp"
#endif


void SSZ_STDCALL Alert(const std::wstring& title, const std::wstring& mes)
{
#ifdef _WIN32
	MessageBox(
		NULL, mes.c_str(), title.c_str(),
		MB_OK | MB_ICONWARNING);
#else
	fprintf(
		stderr, "%s\n%s\n",
		PluginUtil::wToA(PluginUtil::gwToW(title)).c_str(),
		PluginUtil::wToA(PluginUtil::gwToW(mes)).c_str());
#endif
}
