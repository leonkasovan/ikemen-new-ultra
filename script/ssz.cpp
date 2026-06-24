#include "ssz.hpp"

#include "../main/ssz/sszdef.h"
#include "../main/ssz/typeid.h"
#include "../main/ssz/arrayandref.hpp"
#include "../main/ssz/pluginutil.hpp"

extern "C" {
	void     SSZ_STDCALL MemMarkBefore(PluginUtil*, Reference);
	void     SSZ_STDCALL MemMarkAfter(PluginUtil*, Reference);
	bool     SSZ_STDCALL Run(PluginUtil*, Reference);
	void*    SSZ_STDCALL NewCompiler(PluginUtil*);
	void     SSZ_STDCALL DeleteCompiler(PluginUtil*, void*);
	void     SSZ_STDCALL CompilerCompile(PluginUtil*, Reference*, Reference, void*);
	void     SSZ_STDCALL CompilerCompileString(PluginUtil*, Reference*, Reference, Reference, void*);
	bool     SSZ_STDCALL CompilerRun(PluginUtil*, void*);
}

namespace ikemen {

static PluginUtil makePU()
{
	static PluginSSZFuncs f = {nullptr, nullptr, nullptr};
	static PluginUtil pu(&f, nullptr);
	return pu;
}

void memMarkBefore(const std::wstring& label)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, label);
	auto pu = makePU();
	MemMarkBefore(&pu, r);
	r.releaseanddelete();
}

void memMarkAfter(const std::wstring& label)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, label);
	auto pu = makePU();
	MemMarkAfter(&pu, r);
	r.releaseanddelete();
}

bool run(const std::wstring& file)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, file);
	auto pu = makePU();
	bool ok = Run(&pu, r);
	r.releaseanddelete();
	return ok;
}

Compiler::Compiler()
{
	auto pu = makePU();
	m_ptr = reinterpret_cast<intptr_t>(NewCompiler(&pu));
}

Compiler::~Compiler()
{
	if (m_ptr) {
		auto pu = makePU();
		DeleteCompiler(&pu, reinterpret_cast<void*>(m_ptr));
	}
}

std::wstring Compiler::compile(const std::wstring& file)
{
	Reference rFile; rFile.init();
	Reference rErr;  rErr.init();
	PluginUtil::wstrToRef(rFile, file);

	auto pu = makePU();
	CompilerCompile(&pu, &rErr, rFile, reinterpret_cast<void*>(m_ptr));

	std::wstring err = PluginUtil::refToWstr(rErr);
	rFile.releaseanddelete();
	rErr.releaseanddelete();
	return err;
}

std::wstring Compiler::compileString(const std::wstring& code, const std::wstring& dir)
{
	Reference rErr;  rErr.init();
	Reference rDir;  rDir.init();
	Reference rCode; rCode.init();
	PluginUtil::wstrToRef(rDir,  dir);
	PluginUtil::wstrToRef(rCode, code);

	auto pu = makePU();
	CompilerCompileString(&pu, &rErr, rDir, rCode, reinterpret_cast<void*>(m_ptr));

	std::wstring err = PluginUtil::refToWstr(rErr);
	rErr.releaseanddelete();
	rDir.releaseanddelete();
	rCode.releaseanddelete();
	return err;
}

bool Compiler::run()
{
	auto pu = makePU();
	return CompilerRun(&pu, reinterpret_cast<void*>(m_ptr));
}

} // namespace ikemen
