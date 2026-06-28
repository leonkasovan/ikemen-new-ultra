#pragma once

// thread_service.hpp — Native equivalent of ssz_script/lib/thread.ssz
//
// Provides:
// - Thread delay (sleep) in milliseconds
//
// Design note: thread_service delegates to the SSZ native plugin layer
// (main/thread/thread.cpp). The native plugin calls Sleep() on Windows
// or usleep() on Linux.

#include <cstdint>

namespace ikemen::ssz_native::thread {

// Sleep for the specified number of milliseconds.
void delay(uint32_t ms);

} // namespace ikemen::ssz_native::thread
