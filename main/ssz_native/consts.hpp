#pragma once

// consts.hpp — Native C++ equivalents of ssz_script/lib/consts.ssz
//
// SSZ type sizes (from sszdef.h / type system):
//   byte  = 1 byte, short = 2 bytes, int = 4 bytes, long = 8 bytes
//   index = intptr_t (same width as pointer)

#include <cstdint>
#include <limits>

namespace ikemen::ssz_native::consts {

// Signed integer limits (corresponds to &Signed<_t> template in consts.ssz)
template <typename T>
struct Signed {
    static constexpr T MIN = std::numeric_limits<T>::min();
    static constexpr T MAX = std::numeric_limits<T>::max();
};

// Unsigned integer limits (corresponds to &Unsigned<_t> template)
template <typename T>
struct Unsigned {
    static constexpr T MIN = 0;
    static constexpr T MAX = std::numeric_limits<T>::max();
};

// Type aliases matching consts.ssz.
// SSZ uses fixed widths: byte=1B, short=2B, int=4B, long=8B.
// These differ from C types on some platforms (e.g. C `long` may be 4B or 8B).
// The `consts::` namespace makes the origin clear.
using byte_t   = int8_t;    // signed 1-byte   (SSZ: &Signed!byte?)
using short_t  = int16_t;   // signed 2-byte   (SSZ: &Signed!short?)
using int_t    = int32_t;   // signed 4-byte   (SSZ: &Signed!int?)
using long_t   = int64_t;   // signed 8-byte   (SSZ: &Signed!long?)

using ubyte_t  = uint8_t;   // unsigned 1-byte (SSZ: &Unsigned!byte?)
using ushort_t = uint16_t;  // unsigned 2-byte (SSZ: &Unsigned!short?)
using uint_t   = uint32_t;  // unsigned 4-byte (SSZ: &Unsigned!int?)
using ulong_t  = uint64_t;  // unsigned 8-byte (SSZ: &Unsigned!long?)

using char_t   = ubyte_t;   // unsigned 1-byte (SSZ: &Unsigned!char?)
using index_t  = intptr_t;  // pointer-width   (SSZ: &Signed!index?)

// Commonly used sentinel values (mirrors .consts.int_t::MIN / .consts.int_t::MAX)
inline constexpr int32_t SENTINEL_MIN = Signed<int32_t>::MIN;
inline constexpr int32_t SENTINEL_MAX = Signed<int32_t>::MAX;
inline constexpr uint32_t SENTINEL_UMAX = Unsigned<uint32_t>::MAX;

} // namespace ikemen::ssz_native::consts
