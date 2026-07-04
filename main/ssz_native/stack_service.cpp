// stack_service.cpp — Native C++ equivalent of ssz_script/lib/stack.ssz
//
// Stack<T> is a header-only template defined in stack_service.hpp.
// This file exists to satisfy the build system's expectation of a
// corresponding .cpp file for every *_service.hpp.  All Stack<T>
// functionality is inline in the header.
//
// The template is instantiated here for commonly used types to reduce
// code bloat in translation units that include stack_service.hpp.

#include <string>

#include "stack_service.hpp"

namespace ikemen::ssz_native {

// Explicit instantiations for common Stack types.
template struct Stack<int>;
template struct Stack<float>;
template struct Stack<double>;
template struct Stack<std::string>;

} // namespace ikemen::ssz_native
