#include "script.hpp"

namespace ikemen {

double numArg(LuaState& L, int& re, int& argc, int nresults)
{
	double num = 0.0;
	argc++;
	if (!L.isNumber(argc)) {
		L.pushString(std::to_wstring(argc) + L"The argument is not a number.");
		re = -1;
		return num;
	}
	num = L.toNumber(argc);
	re = nresults;
	return num;
}

bool blArg(LuaState& L, int& re, int& argc, int nresults)
{
	bool b = false;
	argc++;
	if (!L.isBoolean(argc)) {
		L.pushString(std::to_wstring(argc) + L"The argument is not a boolean.");
		re = -1;
		return b;
	}
	b = L.toBoolean(argc);
	re = nresults;
	return b;
}

std::wstring strArg(LuaState& L, int& re, int& argc, int nresults)
{
	std::wstring str;
	argc++;
	if (!L.isString(argc)) {
		L.pushString(std::to_wstring(argc) + L"The argument is not a string.");
		re = -1;
		return str;
	}
	str = L.toString(argc);
	re = nresults;
	return str;
}

bool sszReload(const std::wstring& file)
{
	return luaRunSsz(file);
}

} // namespace ikemen
