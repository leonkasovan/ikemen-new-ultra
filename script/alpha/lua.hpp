#pragma once

#include <cstdint>
#include <string>

namespace ikemen {

// ── Lua State ────────────────────────────────────────────────────────────

class LuaState {
public:
	LuaState();
	~LuaState();

	bool    runFile(const std::wstring& path);
	bool    runString(const std::wstring& code);
	int     getTop();
	void    getGlobal(const std::wstring& var);
	void    registerFunc(const std::wstring& name, intptr_t func);
	bool    pcall(int nargs, int nresults);
	void    pop(int n);
	void    pushNumber(double n);
	bool    isNumber(int idx);
	double  toNumber(int idx);
	void    pushBoolean(bool b);
	bool    isBoolean(int idx);
	bool    toBoolean(int idx);
	void    pushString(const std::wstring& s);
	bool    isString(int idx);
	std::wstring toString(int idx);
	void    pushRef(void* ref);
	void*   toRef(int idx);

	intptr_t handle() const { return m_ptr; }

private:
	intptr_t m_ptr = 0;
};

// ── SSZ file runner (re-inits Lua after each run) ────────────────────────

bool luaRunSsz(const std::wstring& file);

// ── Initialize Lua runtime ───────────────────────────────────────────────

void luaInit();

} // namespace ikemen
