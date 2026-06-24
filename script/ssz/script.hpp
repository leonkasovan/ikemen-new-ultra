#pragma once

#include "common.hpp"
#include "command.hpp"
#include "fight.hpp"

#include "../alpha/lua.hpp"
#include "../alpha/sdlplugin.hpp"

#include <string>
#include <vector>

namespace ikemen {

// ── Lua argument helpers ────────────────────────────────────────────────

double           numArg(LuaState& L, int& re, int& argc, int nresults);
bool             blArg(LuaState& L, int& re, int& argc, int nresults);
std::wstring     strArg(LuaState& L, int& re, int& argc, int nresults);

// ── Script reload ───────────────────────────────────────────────────────

bool sszReload(const std::wstring& file);

} // namespace ikemen
