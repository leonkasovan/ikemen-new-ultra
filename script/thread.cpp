#include "thread.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <thread>
#endif

namespace ikemen {

void sleep(uint32_t milliseconds)
{
#ifdef _WIN32
	Sleep(milliseconds);
#else
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
#endif
}

} // namespace ikemen
