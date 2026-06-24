
#include <time.h>

#include "sszdef.h"

#include "typeid.h"
#include "arrayandref.hpp"
#include "pluginutil.hpp"


extern "C" uint32_t SSZ_STDCALL TickCount(PluginUtil* pu)
{
#ifdef _WIN32
	return timeGetTime();
#else
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return now.tv_sec * 1000 + now.tv_nsec / 1000000;
#endif
}

extern "C" int64_t SSZ_STDCALL UnixTime(PluginUtil* pu)
{
	return time(nullptr);
}
