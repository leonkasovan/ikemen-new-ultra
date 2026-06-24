#pragma once

#include <cstdint>
#include <limits>
#include <vector>

namespace ikemen {

// ── Compile-time min/max for each primitive width ─────────────────────

template<typename T>
struct Unsigned {
	static constexpr T MAX = ~static_cast<T>(0);
	static constexpr T MIN = static_cast<T>(0);
};

template<typename T>
struct Signed {
	static constexpr T MAX = (static_cast<T>(1) << (8 * sizeof(T) - 1)) - 1;
	static constexpr T MIN = ~MAX;
};

// ── Convenience aliases matching SSZ type names ───────────────────────

using byte_t   = Signed<int8_t>;
using short_t  = Signed<int16_t>;
using int_t    = Signed<int32_t>;
using long_t   = Signed<int64_t>;

using ubyte_t  = Unsigned<uint8_t>;
using ushort_t = Unsigned<uint16_t>;
using uint_t   = Unsigned<uint32_t>;
using ulong_t  = Unsigned<uint64_t>;

using char_t   = Unsigned<char16_t>;
using index_t  = Signed<intptr_t>;

// ── Generic empty-list factory ────────────────────────────────────────

template<typename T>
std::vector<T> null()
{
	return {};
}

} // namespace ikemen
