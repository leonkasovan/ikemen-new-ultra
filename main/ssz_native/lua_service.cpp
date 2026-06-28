#include "lua_service.hpp"

#ifndef SSZ_STDCALL
#define SSZ_STDCALL __stdcall
#endif

// Native callback typedef (matches main/lua/lua.cpp)
typedef void* (SSZ_STDCALL* SSZCALLBACK)(void*, intptr_t, void*, intptr_t, intptr_t);

// Native lua plugin functions (defined in main/lua/lua.cpp).
// These declarations duplicate bridge.cpp:100-120. They are tracked in
// plugin_native_api.hpp's M4 TODO for eventual consolidation.
// TODO: Move to plugin_native_api.hpp when lua is migrated (Phase 2).
void       SSZ_STDCALL LuaInit(intptr_t refcopy, intptr_t refdest, SSZCALLBACK callback, void* handle);
lua_State* SSZ_STDCALL NewState();
void       SSZ_STDCALL Close(lua_State* L);
bool       SSZ_STDCALL RunFile(const std::string& filename, lua_State* L);
bool       SSZ_STDCALL RunString(const std::string& s, lua_State* L);
int32_t    SSZ_STDCALL GetTop(lua_State* L);
void       SSZ_STDCALL GetGlobal(const std::string& var, lua_State* L);
void       SSZ_STDCALL Register(intptr_t func, const std::string& var, lua_State* L);
bool       SSZ_STDCALL Pcall(int32_t nresults, int32_t nargs, lua_State* L);
void       SSZ_STDCALL Pop(int32_t n, lua_State* L);
void       SSZ_STDCALL PushNumber(double n, lua_State* L);
bool       SSZ_STDCALL IsNumber(int32_t idx, lua_State* L);
double     SSZ_STDCALL ToNumber(int32_t idx, lua_State* L);
void       SSZ_STDCALL PushBoolean(bool b, lua_State* L);
bool       SSZ_STDCALL IsBoolean(int32_t idx, lua_State* L);
bool       SSZ_STDCALL ToBoolean(int32_t idx, lua_State* L);
void       SSZ_STDCALL PushString(const std::string& s, lua_State* L);
bool       SSZ_STDCALL IsString(int32_t idx, lua_State* L);
void       SSZ_STDCALL ToString(int32_t idx, lua_State* L, std::string& output);

namespace ikemen::ssz_native::lua {

LuaState::LuaState() {
    L_ = NewState();
}

LuaState::~LuaState() {
    destroy();
}

void LuaState::destroy() {
    if (L_) {
        Close(L_);
        L_ = nullptr;
    }
}

bool LuaState::run_file(const std::string& filename) {
    return L_ ? RunFile(filename, L_) : false;
}

bool LuaState::run_string(const std::string& str) {
    return L_ ? RunString(str, L_) : false;
}

int32_t LuaState::get_top() {
    return L_ ? GetTop(L_) : 0;
}

void LuaState::get_global(const std::string& var) {
    if (L_) GetGlobal(var, L_);
}

bool LuaState::pcall(int32_t nargs, int32_t nresults) {
    return L_ ? Pcall(nresults, nargs, L_) : false;
}

void LuaState::pop(int32_t n) {
    if (L_) Pop(n, L_);
}

void LuaState::push_number(double n) {
    if (L_) PushNumber(n, L_);
}

bool LuaState::is_number(int32_t idx) {
    return L_ ? IsNumber(idx, L_) : false;
}

double LuaState::to_number(int32_t idx) {
    return L_ ? ToNumber(idx, L_) : 0.0;
}

void LuaState::push_boolean(bool b) {
    if (L_) PushBoolean(b, L_);
}

bool LuaState::is_boolean(int32_t idx) {
    return L_ ? IsBoolean(idx, L_) : false;
}

bool LuaState::to_boolean(int32_t idx) {
    return L_ ? ToBoolean(idx, L_) : false;
}

void LuaState::push_string(const std::string& s) {
    if (L_) PushString(s, L_);
}

bool LuaState::is_string(int32_t idx) {
    return L_ ? IsString(idx, L_) : false;
}

std::string LuaState::to_string(int32_t idx) {
    std::string output;
    if (L_) ToString(idx, L_, output);
    return output;
}

} // namespace ikemen::ssz_native::lua
