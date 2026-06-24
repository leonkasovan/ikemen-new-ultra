#include "alert.hpp"

#include "../main/ssz/sszdef.h"
#include "../main/ssz/typeid.h"
#include "../main/ssz/arrayandref.hpp"
#include "../main/ssz/pluginutil.hpp"

extern "C" void SSZ_STDCALL Alert(PluginUtil* pu, Reference title, Reference mes);

namespace ikemen {

void Alert::alert(const Reference& mes, const std::string& typeName)
{
	Reference title;
	title.init();

#ifdef _WIN32
	PluginUtil::astrToRef(CP_UTF8, title, typeName);
#else
	std::WSTR wtypeName = PluginUtil::aToW(typeName);
	PluginUtil::wstrToRef(title, wtypeName);
#endif

	PluginSSZFuncs dummyFuncs = {nullptr, nullptr, nullptr};
	PluginUtil dummyPU(&dummyFuncs, nullptr);

	::Alert(&dummyPU, title, mes);

	title.releaseanddelete();
}

} // namespace ikemen
