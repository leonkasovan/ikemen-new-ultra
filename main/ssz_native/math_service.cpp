#include "math_service.hpp"

#include <chrono>
#include <cstdlib>

namespace ikemen::ssz_native::math {
namespace {

int32_t g_seed = []() -> int32_t {
    // Matching SSZ initializer: (time.unixTime() ^ (long)time.tickCount()<<16) & RANDMAX
    auto now = std::chrono::system_clock::now();
    auto unixTime = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    auto tickMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return static_cast<int32_t>((unixTime ^ (tickMs << 16)) & RANDMAX);
}();

} // anonymous namespace

int32_t& seed() {
    return g_seed;
}

int32_t random() {
    int32_t w = g_seed / 127773;
    g_seed = (g_seed - w * 127773) * 16807 - w * 2836;
    if (g_seed <= 0) {
        g_seed += RANDMAX - (g_seed == 0 ? 1 : 0);
    }
    return g_seed;
}

int32_t rand(int32_t min, int32_t max) {
    return min + random() / (RANDMAX / (max - min + 1) + 1);
}

int32_t randI(int32_t x, int32_t y) {
    // Cast to int64_t before subtraction to avoid signed integer overflow UB
    // when the range exceeds INT32_MAX (~2.1 billion).
    int64_t x64 = x;
    int64_t y64 = y;
    if (y64 < x64) {
        if (x64 - y64 < 0)
            return static_cast<int32_t>(
                y64 + static_cast<int64_t>(random()) * (x64 - y64) / RANDMAX);
        return rand(y, x);
    }
    if (y64 - x64 < 0)
        return static_cast<int32_t>(
            x64 + static_cast<int64_t>(random()) * (y64 - x64) / RANDMAX);
    return rand(x, y);
}

// Note: random() returns up to RANDMAX (2147483647, 31 bits). float has a 24-bit
// mantissa, so values above 2^24 (~16.7M) lose precision when cast. For typical
// SSZ usage (small ranges like frame counts or character indices) this is not a
// problem. If distribution quality for very wide ranges matters, use double.
float randF(float x, float y) {
    double r = static_cast<double>(random()) * (static_cast<double>(y) - static_cast<double>(x))
               / static_cast<double>(RANDMAX);
    return static_cast<float>(x + r);
}

} // namespace ikemen::ssz_native::math
