// stack_static.hpp — Static plugin registration for ssz_script/lib/stack.ssz
//
// lib/stack.ssz (36 lines) implements a generic stack template (&Stack<_t>)
// with push, pop, top, and clear operations. The native equivalent is
// the header-only Stack<T> template in stack_service.hpp, backed by
// std::vector<T>.
//
// Stack is a pure data-structure template — it does not export any plugin
// bridge functions to the SSZ runtime.  stack_static_register() is a no-op
// that exists only for consistency with the static plugin registration
// pattern.  Other native modules use Stack<T> directly via
// #include "ssz_native/stack_service.hpp".

#pragma once

/// Register the stack module's (empty) plugin table.
/// Always returns true; stack is header-only, no bridge functions.
inline bool stack_static_register()
{
	return true;
}
