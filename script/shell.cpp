#include "shell.hpp"

#include "../main/ssz/sszdef.h"
#include "../main/ssz/typeid.h"
#include "../main/ssz/arrayandref.hpp"
#include "../main/ssz/pluginutil.hpp"

extern "C" {
	bool SSZ_STDCALL ShellOpen(PluginUtil*, Reference, Reference, Reference, bool, bool);
	bool SSZ_STDCALL MoveTrash(PluginUtil*, Reference);
}

namespace ikemen {

static PluginUtil makePU()
{
	static PluginSSZFuncs f = {nullptr, nullptr, nullptr};
	static PluginUtil pu(&f, nullptr);
	return pu;
}

bool open(const std::wstring& file, const std::wstring& arg,
          const std::wstring& cdir, bool waitfor, bool active)
{
	Reference rFile;  rFile.init();
	Reference rArg;   rArg.init();
	Reference rCdir;  rCdir.init();
	PluginUtil::wstrToRef(rFile, file);
	PluginUtil::wstrToRef(rArg,  arg);
	PluginUtil::wstrToRef(rCdir, cdir);
	auto pu = makePU();
	bool r = ShellOpen(&pu, rFile, rArg, rCdir, waitfor, active);
	rFile.releaseanddelete();
	rArg.releaseanddelete();
	rCdir.releaseanddelete();
	return r;
}

bool moveToTrash(const std::wstring& file)
{
	Reference rFile; rFile.init();
	PluginUtil::wstrToRef(rFile, file);
	auto pu = makePU();
	bool r = MoveTrash(&pu, rFile);
	rFile.releaseanddelete();
	return r;
}

} // namespace ikemen
