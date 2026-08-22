#include "sszdef.h"

#ifndef _WIN32
#include "pluginutil.hpp"
#endif


void SSZ_STDCALL Alert(const std::wstring& title, const std::wstring& mes)
{
#ifdef _WIN32
	// Always mirror to stderr so headless runs capture the error in the log
	// even when the blocking message box is dismissed or unavailable.
	{
		std::string aTitle;
		std::string aMes;
		if(!title.empty()){
			int len = WideCharToMultiByte(
				CP_UTF8, 0, title.c_str(), (int)title.size(), nullptr, 0, nullptr, nullptr);
			aTitle.resize(len);
			WideCharToMultiByte(
				CP_UTF8, 0, title.c_str(), (int)title.size(),
				(char*)aTitle.data(), len, nullptr, nullptr);
		}
		if(!mes.empty()){
			int len = WideCharToMultiByte(
				CP_UTF8, 0, mes.c_str(), (int)mes.size(), nullptr, 0, nullptr, nullptr);
			aMes.resize(len);
			WideCharToMultiByte(
				CP_UTF8, 0, mes.c_str(), (int)mes.size(),
				(char*)aMes.data(), len, nullptr, nullptr);
		}
		fprintf(stderr, "[Alert] %s\n%s\n", aTitle.c_str(), aMes.c_str());
		fflush(stderr);
	}
# ifdef NDEBUG
	MessageBox(
		NULL, mes.c_str(), title.c_str(),
		MB_OK | MB_ICONWARNING);
# endif
#else
	fprintf(
		stderr, "%s\n%s\n",
		PluginUtil::wToA(PluginUtil::gwToW(title)).c_str(),
		PluginUtil::wToA(PluginUtil::gwToW(mes)).c_str());
#endif
}
