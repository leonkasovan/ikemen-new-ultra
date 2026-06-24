#pragma once

#include <vector>

namespace ikemen {

template<typename T>
class Stack {
public:
	void push(const T& data) { m_data.push_back(data); }
	T    pop()               { T v = m_data.back(); m_data.pop_back(); return v; }
	T    top() const         { return m_data.back(); }
	void clear()             { m_data.clear(); }
	bool empty() const       { return m_data.empty(); }

private:
	std::vector<T> m_data;
};

} // namespace ikemen
