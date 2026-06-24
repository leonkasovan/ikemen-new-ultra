
#include "sszdef.h"

#include "typeid.h"
#include "arrayandref.hpp"
#include "pluginutil.hpp"


extern "C" void SSZ_STDCALL ThreadDelay(PluginUtil* pu, uint32_t ui)
{
	Sleep(ui);
}
