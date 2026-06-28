#include "thread_service.hpp"

#ifndef SSZ_STDCALL
#define SSZ_STDCALL __stdcall
#endif

// Native thread plugin function (defined in main/thread/thread.cpp).
// This declaration is provided by plugin_native_api.hpp (which is the
// single source of truth for shared declarations). Tracked in M4 TODO.
void SSZ_STDCALL ThreadDelay(uint32_t ui);

namespace ikemen::ssz_native::thread {

void delay(uint32_t ms) {
    ThreadDelay(ms);
}

} // namespace ikemen::ssz_native::thread
