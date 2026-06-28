#pragma once

// lua_service.hpp — Native equivalent of ssz_script/lib/alpha/lua.ssz
//
// Provides:
// - RAII wrapper for lua_State
// - Lua stack operations: push, pop, get, call, type checking
// - Lua file/string execution
//
// Design note: lua_service delegates to the SSZ native plugin layer
// (main/lua/lua.cpp). The native plugin wraps the Lua C API (lua.hpp).
// Using the call-through pattern avoids duplicating Lua initialization
// and callback management.

#include <cstdint>
#include <string>

struct lua_State;

namespace ikemen::ssz_native::lua {

// RAII wrapper around a lua_State*.
// Mirrors the &State object from ssz_script/lib/alpha/lua.ssz.
class LuaState {
public:
    LuaState();
    ~LuaState();

    bool is_valid() const { return L_ != nullptr; }

    bool run_file(const std::string& filename);
    bool run_string(const std::string& str);

    int32_t get_top();
    void get_global(const std::string& var);

    bool pcall(int32_t nargs, int32_t nresults);
    void pop(int32_t n);

    void push_number(double n);
    bool is_number(int32_t idx);
    double to_number(int32_t idx);

    void push_boolean(bool b);
    bool is_boolean(int32_t idx);
    bool to_boolean(int32_t idx);

    void push_string(const std::string& s);
    bool is_string(int32_t idx);
    std::string to_string(int32_t idx);

    // Non-copyable, movable.
    LuaState(const LuaState&) = delete;
    LuaState& operator=(const LuaState&) = delete;
    LuaState(LuaState&& other) noexcept : L_(other.L_) { other.L_ = nullptr; }
    LuaState& operator=(LuaState&& other) noexcept {
        if (this != &other) { destroy(); L_ = other.L_; other.L_ = nullptr; }
        return *this;
    }

private:
    void destroy();
    lua_State* L_ = nullptr;
};

} // namespace ikemen::ssz_native::lua
