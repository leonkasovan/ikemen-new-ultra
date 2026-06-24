#pragma once

#include <string>

namespace ikemen {

void memMarkBefore(const std::wstring& label);
void memMarkAfter(const std::wstring& label);
bool run(const std::wstring& file);

class Compiler {
public:
	Compiler();
	~Compiler();

	std::wstring compile(const std::wstring& file);
	std::wstring compileString(const std::wstring& code, const std::wstring& dir);
	bool run();

private:
	intptr_t m_ptr = 0;
};

} // namespace ikemen
