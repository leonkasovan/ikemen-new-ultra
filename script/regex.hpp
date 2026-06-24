#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace ikemen {

class Regex {
public:
	Regex() = default;
	~Regex();

	std::wstring init(const std::wstring& pattern, int flags);
	std::vector<std::wstring> search(const std::wstring& str);

private:
	void clear();
	intptr_t m_re = 0;
};

} // namespace ikemen
