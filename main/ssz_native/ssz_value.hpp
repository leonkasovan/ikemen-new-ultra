#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ikemen::ssz_native {

using Index = intptr_t;

struct SszString {
    std::wstring value;
};

struct SszBytes {
    std::vector<uint8_t> data;
};

template <typename T>
using SszArray = std::vector<T>;

} // namespace ikemen::ssz_native
