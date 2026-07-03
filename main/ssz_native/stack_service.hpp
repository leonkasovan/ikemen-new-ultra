// stack_service.hpp — Native C++ equivalent of ssz_script/lib/stack.ssz
//
// stack.ssz (36 lines) implements a generic stack template (&Stack<_t>)
// with push, pop, and clear operations. The native equivalent is
// std::vector<T> used as a stack (push_back/pop_back/clear).

#pragma once

#include <vector>

namespace ikemen::ssz_native {

template<typename T>
struct Stack {
	std::vector<T> data;

	void push(const T& item) { data.push_back(item); }
	T pop() { T t = std::move(data.back()); data.pop_back(); return t; }
	const T* top() const { return data.empty() ? nullptr : &data.back(); }
	void clear() { data.clear(); }
	bool empty() const { return data.empty(); }
	std::size_t size() const { return data.size(); }
};

} // namespace ikemen::ssz_native
