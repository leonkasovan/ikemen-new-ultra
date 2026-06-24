#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ikemen {

// ── OS time / tick ───────────────────────────────────────────────────────

uint32_t tickCount();
int64_t  unixTime();

// ── Floor division & modulo (always toward −∞) ──────────────────────────

int64_t div(int64_t x, int64_t y);
int64_t mod(int64_t x, int64_t y);

// ── Calendar conversion ──────────────────────────────────────────────────

int64_t            days(int64_t unix);
std::vector<int32_t> ymdhms(int64_t unix);
int64_t            ymdhmsToUnixTime(int64_t Y, int8_t M, int64_t D,
                                    int64_t h, int64_t m, int64_t s);

} // namespace ikemen
