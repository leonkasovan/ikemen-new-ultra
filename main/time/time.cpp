#include <time.h>

#include "sszdef.h"


uint32_t SSZ_STDCALL TickCount()
{
#ifdef _WIN32
	return timeGetTime();
#else
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return now.tv_sec * 1000 + now.tv_nsec / 1000000;
#endif
}

int64_t SSZ_STDCALL UnixTime()
{
	return time(nullptr);
}
