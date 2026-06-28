#include "time_service.hpp"

#ifndef SSZ_STDCALL
#define SSZ_STDCALL __stdcall
#endif

// Native time plugin functions (defined in main/time/time.cpp).
// These declarations are provided by plugin_native_api.hpp (which is the
// single source of truth for shared declarations). Tracked in M4 TODO.
uint32_t SSZ_STDCALL TickCount();
int64_t  SSZ_STDCALL UnixTime();

namespace ikemen::ssz_native::time_util {

uint32_t tick_count() {
    return TickCount();
}

int64_t unix_time() {
    return UnixTime();
}

} // namespace ikemen::ssz_native::time_util
