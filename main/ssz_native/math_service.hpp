#pragma once

// math_service.hpp — Native equivalent of ssz_script/lib/math.ssz
//
// Provides:
// - Math functions reimplemented natively via the C standard library
// - PRNG (Park-Miller LCG) matching SSZ's random/srand/rand/randI/randF
// - Utility templates: min, max, inRange, clamp, swap
//
// Design note: math_service intentionally bypasses the SSZ native plugin layer
// (main/math/math.cpp's Sin, Cos, etc.). Those plugin functions are themselves
// thin wrappers around std::sin/std::cos, so the results are identical. This
// module compiles and tests without linking against any main/*.cpp plugin
// object files, making it fully independent of the SSZ runtime.
// If main/math/math.cpp ever adds SSZ-specific custom behavior, these inline
// wrappers must be updated to call through plugin_native_api.hpp instead.

#include <cstdint>
#include <cmath>

namespace ikemen::ssz_native::math {

// ---- Constants ----

inline constexpr double PI = 3.141592653589793238462643383279502884197;
inline constexpr double E  = 2.718281828459045235360287471352662497757;

// ---- Math functions (reimplemented via C standard library) ----
// These match the SSZ plugin signatures (defined in main/math/math.cpp).
// The SSZ native plugin functions are themselves just std::sin/std::cos
// wrappers, so calling the C standard library directly is equivalent.

inline double sin(double x) { return ::sin(x); }
inline double cos(double x) { return ::cos(x); }
inline double tan(double x) { return ::tan(x); }
inline double asin(double x) { return ::asin(x); }
inline double acos(double x) { return ::acos(x); }
inline double atan(double x) { return ::atan(x); }
inline double log(double y, double x) { return ::log(x) / ::log(y); }
inline double ln(double x) { return ::log(x); }
inline double exp(double x) { return ::exp(x); }
inline double sqrt(double x) { return ::sqrt(x); }
inline double ceil(double x) { return ::ceil(x); }
inline double floor(double x) { return ::floor(x); }
inline bool isfinite(double x) { return std::isfinite(x); }
inline bool isinf(double x) { return std::isinf(x); }
inline bool isnan(double x) { return std::isnan(x); }

// ---- Rounding (service-layer addition, no SSZ native plugin equivalent) ----
// round half away from zero, matching SSZ script conventions.
// This is new functionality added at the ssz_native layer — there is no
// corresponding Round function in main/math/math.cpp.

inline double round(double x) { return x < 0.0 ? -::floor(0.5 - x) : ::floor(0.5 + x); }

// ---- Park-Miller LCG PRNG (matching SSZ implementation) ----

// The SSZ PRNG uses RANDMAX = int_t::MAX = 2147483647
inline constexpr int32_t RANDMAX = 2147483647;

// Module-level seed, initialized once. Mirrors SSZ's module-level `randseed`.
// Default initializer matches SSZ: (time.unixTime() ^ (long)time.tickCount()<<16) & RANDMAX
int32_t& seed();

// Set seed explicitly (matches SSZ srand(int s))
inline void srand(int32_t s) { seed() = s; }

// Generate next random value in [0, RANDMAX] (matches SSZ random())
int32_t random();

// Uniform integer in [min, max] (matches SSZ rand)
int32_t rand(int32_t min, int32_t max);

// Inclusive range random (matches SSZ randI)
int32_t randI(int32_t x, int32_t y);

// Uniform float in [x, y] (matches SSZ randF)
float randF(float x, float y);

// ---- Utility templates ----

template <typename T>
inline T min(T x, T y) { return x < y ? x : y; }

template <typename T>
inline T max(T x, T y) { return x > y ? x : y; }

template <typename T>
inline bool inRange(T start, T end, T x) { return start <= x && x <= end; }

template <typename T>
inline void limMax(T& x, T y) { if (x > y) x = y; }

template <typename T>
inline void limMin(T& x, T y) { if (x < y) x = y; }

template <typename T>
inline void limRange(T& x, T low, T high) {
    if (x > high) x = high;
    if (x < low) x = low;
}

template <typename T>
inline void swap(T& x, T& y) { T tmp = x; x = y; y = tmp; }

} // namespace ikemen::ssz_native::math
