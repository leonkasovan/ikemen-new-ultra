#include "file.hpp"

#include "../main/ssz/sszdef.h"
#include "../main/ssz/typeid.h"
#include "../main/ssz/arrayandref.hpp"
#include "../main/ssz/pluginutil.hpp"

extern "C" {
	void     SSZ_STDCALL LoadAsciiText(PluginUtil*, Reference*, Reference);
	bool     SSZ_STDCALL SaveAsciiText(PluginUtil*, Reference, Reference);
	bool     SSZ_STDCALL Delete(PluginUtil*, Reference);
	bool     SSZ_STDCALL Move(PluginUtil*, Reference, Reference);
	bool     SSZ_STDCALL Copy(PluginUtil*, bool, Reference, Reference);
	void     SSZ_STDCALL Find(PluginUtil*, Reference*, Reference);
	void     SSZ_STDCALL FindDir(PluginUtil*, Reference*, Reference);
	bool     SSZ_STDCALL CreateDir(PluginUtil*, Reference);
	bool     SSZ_STDCALL RemoveDir(PluginUtil*, Reference);
	bool     SSZ_STDCALL SetCurrentDir(PluginUtil*, Reference);
	void     SSZ_STDCALL GetCurrentDir(PluginUtil*, Reference*);
}

namespace ikemen {

static PluginUtil makePU()
{
	static PluginSSZFuncs f = {nullptr, nullptr, nullptr};
	static PluginUtil pu(&f, nullptr);
	return pu;
}

bool File::open(const std::wstring& fn, const std::wstring& mode)
{
	close();
#ifdef _WIN32
	m_fh = _wfopen(fn.c_str(), mode.c_str());
#else
	std::string n(fn.begin(), fn.end());
	std::string m(mode.begin(), mode.end());
	m_fh = std::fopen(n.c_str(), m.c_str());
#endif
	return m_fh != nullptr;
}

void File::close()
{
	if (m_fh) { std::fclose(m_fh); m_fh = nullptr; }
}

bool File::seek(int64_t offset, Seek origin)
{
	if (!m_fh) return false;
	int o = origin == Seek::SET ? SEEK_SET : origin == Seek::CUR ? SEEK_CUR : SEEK_END;
	return _fseeki64(m_fh, offset, o) == 0;
}

std::wstring loadAsciiText(const std::wstring& fn)
{
	Reference rFn;  rFn.init();
	Reference rOut; rOut.init();
	PluginUtil::wstrToRef(rFn, fn);
	auto pu = makePU();
	LoadAsciiText(&pu, &rOut, rFn);
	std::wstring r = PluginUtil::refToWstr(rOut);
	rFn.releaseanddelete();
	rOut.releaseanddelete();
	return r;
}

bool saveAsciiText(const std::wstring& fn, const std::wstring& txt)
{
	Reference rFn;  rFn.init();
	Reference rTxt; rTxt.init();
	PluginUtil::wstrToRef(rFn, fn);
	PluginUtil::wstrToRef(rTxt, txt);
	auto pu = makePU();
	bool ok = SaveAsciiText(&pu, rFn, rTxt);
	rFn.releaseanddelete();
	rTxt.releaseanddelete();
	return ok;
}

bool remove(const std::wstring& fn)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, fn);
	auto pu = makePU();
	bool ok = Delete(&pu, r);
	r.releaseanddelete();
	return ok;
}

bool move(const std::wstring& oldn, const std::wstring& newn)
{
	Reference rOld; rOld.init();
	Reference rNew; rNew.init();
	PluginUtil::wstrToRef(rOld, oldn);
	PluginUtil::wstrToRef(rNew, newn);
	auto pu = makePU();
	bool ok = Move(&pu, rOld, rNew);
	rOld.releaseanddelete();
	rNew.releaseanddelete();
	return ok;
}

bool copy(const std::wstring& src, const std::wstring& dst, bool overwrite)
{
	Reference rSrc; rSrc.init();
	Reference rDst; rDst.init();
	PluginUtil::wstrToRef(rSrc, src);
	PluginUtil::wstrToRef(rDst, dst);
	auto pu = makePU();
	bool ok = Copy(&pu, overwrite, rSrc, rDst);
	rSrc.releaseanddelete();
	rDst.releaseanddelete();
	return ok;
}

std::vector<std::wstring> find(const std::wstring& pattern)
{
	Reference rP;  rP.init();
	Reference rOut; rOut.init();
	PluginUtil::wstrToRef(rP, pattern);
	auto pu = makePU();
	Find(&pu, &rOut, rP);
	std::vector<std::wstring> out;
	for (intptr_t i = 0; rOut.len() > 0 && i < rOut.len() / static_cast<intptr_t>(sizeof(Reference)); i++) {
		auto* refs = reinterpret_cast<Reference*>(rOut.atpos());
		out.push_back(PluginUtil::refToWstr(refs[i]));
	}
	rP.releaseanddelete();
	rOut.releaseanddelete();
	return out;
}

std::vector<std::wstring> findDir(const std::wstring& pattern)
{
	Reference rP;  rP.init();
	Reference rOut; rOut.init();
	PluginUtil::wstrToRef(rP, pattern);
	auto pu = makePU();
	FindDir(&pu, &rOut, rP);
	std::vector<std::wstring> out;
	for (intptr_t i = 0; rOut.len() > 0 && i < rOut.len() / static_cast<intptr_t>(sizeof(Reference)); i++) {
		auto* refs = reinterpret_cast<Reference*>(rOut.atpos());
		out.push_back(PluginUtil::refToWstr(refs[i]));
	}
	rP.releaseanddelete();
	rOut.releaseanddelete();
	return out;
}

bool createDir(const std::wstring& dir)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, dir);
	auto pu = makePU();
	bool ok = CreateDir(&pu, r);
	r.releaseanddelete();
	return ok;
}

bool removeDir(const std::wstring& dir)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, dir);
	auto pu = makePU();
	bool ok = RemoveDir(&pu, r);
	r.releaseanddelete();
	return ok;
}

bool setCurrentDir(const std::wstring& dir)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, dir);
	auto pu = makePU();
	bool ok = SetCurrentDir(&pu, r);
	r.releaseanddelete();
	return ok;
}

std::wstring getCurrentDir()
{
	Reference r; r.init();
	auto pu = makePU();
	GetCurrentDir(&pu, &r);
	std::wstring out = PluginUtil::refToWstr(r);
	r.releaseanddelete();
	return out;
}

} // namespace ikemen
