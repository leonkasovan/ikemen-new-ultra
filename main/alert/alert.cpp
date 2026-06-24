
#include "sszdef.h"

#include "typeid.h"
#include "arrayandref.hpp"
#include "pluginutil.hpp"

extern "C" void SSZ_STDCALL Alert(PluginUtil* pu, Reference title, Reference mes)
{
#ifdef _WIN32
	MessageBox(
		NULL, pu->refToWstr(mes).c_str(), pu->refToWstr(title).c_str(),
		MB_OK | MB_ICONWARNING);
#else
	fprintf(
		stderr, "%s\n%s\n",
		pu->wToA(pu->refToWstr(title)).c_str(),
		pu->wToA(pu->refToWstr(mes)).c_str());
#endif
}
