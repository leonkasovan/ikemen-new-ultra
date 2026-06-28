#pragma once

// time_service.hpp — Native equivalent of ssz_script/lib/time.ssz
//
// Provides:
// - Monotonic tick count in milliseconds
// - Unix timestamp (seconds since epoch)
//
// Design note: time_service delegates to the SSZ native plugin layer
// (main/time/time.cpp). The native plugin calls timeGetTime() on Windows
// or clock_gettime() on Linux.

#include <cstdint>

namespace ikemen::ssz_native::time_util {

// Returns the number of milliseconds since system start (monotonic).
uint32_t tick_count();

// Returns the current Unix timestamp in seconds.
int64_t unix_time();

} // namespace ikemen::ssz_native::time_util
