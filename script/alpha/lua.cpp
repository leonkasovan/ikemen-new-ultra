#include "lua.hpp"
#include "../ssz.hpp"

#include "sszdef.h"
#include "typeid.h"
#include "arrayandref.hpp"
#include "pluginutil.hpp"

extern "C" {
	void     SSZ_STDCALL LuaInit(PluginUtil*, intptr_t, intptr_t);
	void*    SSZ_STDCALL NewState(PluginUtil*);
	void     SSZ_STDCALL Close(PluginUtil*, void*);
	bool     SSZ_STDCALL RunFile(PluginUtil*, Reference, void*);
	bool     SSZ_STDCALL RunString(PluginUtil*, Reference, void*);
	int32_t  SSZ_STDCALL GetTop(PluginUtil*, void*);
	void     SSZ_STDCALL GetGlobal(PluginUtil*, Reference, void*);
	void     SSZ_STDCALL Register(PluginUtil*, intptr_t, Reference, void*);
	bool     SSZ_STDCALL Pcall(PluginUtil*, int32_t, int32_t, void*);
	void     SSZ_STDCALL Pop(PluginUtil*, int32_t, void*);
	void     SSZ_STDCALL PushNumber(PluginUtil*, double, void*);
	bool     SSZ_STDCALL IsNumber(PluginUtil*, int32_t, void*);
	double   SSZ_STDCALL ToNumber(PluginUtil*, int32_t, void*);
	void     SSZ_STDCALL PushBoolean(PluginUtil*, bool, void*);
	bool     SSZ_STDCALL IsBoolean(PluginUtil*, int32_t, void*);
	bool     SSZ_STDCALL ToBoolean(PluginUtil*, int32_t, void*);
	void     SSZ_STDCALL PushString(PluginUtil*, Reference, void*);
	bool     SSZ_STDCALL IsString(PluginUtil*, int32_t, void*);
	void     SSZ_STDCALL ToString(PluginUtil*, int32_t, Reference*, void*);
	void     SSZ_STDCALL PushRef(PluginUtil*, DynamicRef*, void*);
	void     SSZ_STDCALL ToRef(PluginUtil*, int32_t, DynamicRef*, void*);
	PluginSSZFuncs* SSZ_STDCALL GetSSZFuncs();
}

namespace ikemen {

static PluginUtil makePU() {
	PluginSSZFuncs* sf = GetSSZFuncs();
	static PluginUtil pu(sf, nullptr);
	return pu;
}

extern "C" {
	static void SSZ_STDCALL RefSetNullCb(DynamicRef* o) { o->init(); }
	static void SSZ_STDCALL RefCopyCb(DynamicRef* o, DynamicRef r) { *o = r; }
}

void luaInit()
{
	auto pu = makePU();
	intptr_t fnSetNull = reinterpret_cast<intptr_t>(&RefSetNullCb);
	intptr_t fnCopy    = reinterpret_cast<intptr_t>(&RefCopyCb);
	LuaInit(&pu, fnCopy, fnSetNull);
}

bool luaRunSsz(const std::wstring& file)
{
	bool ok = ikemen::run(file);
	luaInit();
	return ok;
}

// ── LuaState ─────────────────────────────────────────────────────────────

LuaState::LuaState()
{
	auto pu = makePU();
	m_ptr = reinterpret_cast<intptr_t>(NewState(&pu));
}

LuaState::~LuaState()
{
	if (m_ptr) {
		auto pu = makePU();
		Close(&pu, reinterpret_cast<void*>(m_ptr));
	}
}

bool LuaState::runFile(const std::wstring& path)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, path);
	auto pu = makePU();
	bool ok = RunFile(&pu, r, reinterpret_cast<void*>(m_ptr));
	r.releaseanddelete();
	return ok;
}

bool LuaState::runString(const std::wstring& code)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, code);
	auto pu = makePU();
	bool ok = RunString(&pu, r, reinterpret_cast<void*>(m_ptr));
	r.releaseanddelete();
	return ok;
}

int LuaState::getTop()
{
	auto pu = makePU();
	return static_cast<int>(GetTop(&pu, reinterpret_cast<void*>(m_ptr)));
}

void LuaState::getGlobal(const std::wstring& var)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, var);
	auto pu = makePU();
	GetGlobal(&pu, r, reinterpret_cast<void*>(m_ptr));
	r.releaseanddelete();
}

void LuaState::registerFunc(const std::wstring& name, intptr_t func)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, name);
	auto pu = makePU();
	Register(&pu, func, r, reinterpret_cast<void*>(m_ptr));
	r.releaseanddelete();
}

bool LuaState::pcall(int nargs, int nresults)
{
	auto pu = makePU();
	return Pcall(&pu, static_cast<int32_t>(nargs), static_cast<int32_t>(nresults),
	             reinterpret_cast<void*>(m_ptr));
}

void LuaState::pop(int n)
{
	auto pu = makePU();
	Pop(&pu, static_cast<int32_t>(n), reinterpret_cast<void*>(m_ptr));
}

void LuaState::pushNumber(double n)
{
	auto pu = makePU();
	PushNumber(&pu, n, reinterpret_cast<void*>(m_ptr));
}

bool LuaState::isNumber(int idx)
{
	auto pu = makePU();
	return IsNumber(&pu, static_cast<int32_t>(idx), reinterpret_cast<void*>(m_ptr));
}

double LuaState::toNumber(int idx)
{
	auto pu = makePU();
	return ToNumber(&pu, static_cast<int32_t>(idx), reinterpret_cast<void*>(m_ptr));
}

void LuaState::pushBoolean(bool b)
{
	auto pu = makePU();
	PushBoolean(&pu, b, reinterpret_cast<void*>(m_ptr));
}

bool LuaState::isBoolean(int idx)
{
	auto pu = makePU();
	return IsBoolean(&pu, static_cast<int32_t>(idx), reinterpret_cast<void*>(m_ptr));
}

bool LuaState::toBoolean(int idx)
{
	auto pu = makePU();
	return ToBoolean(&pu, static_cast<int32_t>(idx), reinterpret_cast<void*>(m_ptr));
}

void LuaState::pushString(const std::wstring& s)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, s);
	auto pu = makePU();
	PushString(&pu, r, reinterpret_cast<void*>(m_ptr));
	r.releaseanddelete();
}

bool LuaState::isString(int idx)
{
	auto pu = makePU();
	return IsString(&pu, static_cast<int32_t>(idx), reinterpret_cast<void*>(m_ptr));
}

std::wstring LuaState::toString(int idx)
{
	Reference r; r.init();
	auto pu = makePU();
	ToString(&pu, static_cast<int32_t>(idx), &r, reinterpret_cast<void*>(m_ptr));
	std::wstring out = PluginUtil::refToWstr(r);
	r.releaseanddelete();
	return out;
}

void LuaState::pushRef(void* ref)
{
	auto pu = makePU();
	PushRef(&pu, static_cast<DynamicRef*>(ref), reinterpret_cast<void*>(m_ptr));
}

void* LuaState::toRef(int idx)
{
	auto pu = makePU();
	DynamicRef* ref = reinterpret_cast<DynamicRef*>(sszrefnewfunc(sizeof(DynamicRef)));
	ref->init();
	ToRef(&pu, static_cast<int32_t>(idx), ref, reinterpret_cast<void*>(m_ptr));
	return ref;
}

} // namespace ikemen
