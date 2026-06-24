#include "mesdialog.hpp"

#include "sszdef.h"
#include "typeid.h"
#include "arrayandref.hpp"
#include "pluginutil.hpp"

extern "C" {
	bool     SSZ_STDCALL YesNo(PluginUtil*, Reference);
	bool     SSZ_STDCALL GetClipboardStr(PluginUtil*, Reference*);
	void     SSZ_STDCALL GetInifileString(PluginUtil*, Reference*, Reference, Reference, Reference, Reference);
	int32_t  SSZ_STDCALL GetInifileInt(PluginUtil*, int32_t, Reference, Reference, Reference);
	bool     SSZ_STDCALL WriteInifileString(PluginUtil*, Reference, Reference, Reference, Reference);
	void     SSZ_STDCALL InputStr(PluginUtil*, Reference*, Reference);
	bool     SSZ_STDCALL UnCompress(PluginUtil*, Reference*, Reference);
	void     SSZ_STDCALL UbytesToStr(PluginUtil*, Reference, Reference*, uint32_t);
	void     SSZ_STDCALL StrToUbytes(PluginUtil*, Reference, Reference*, uint32_t);
	void     SSZ_STDCALL AsciiToLocal(PluginUtil*, Reference, Reference*);
	void     SSZ_STDCALL SetSharedString(PluginUtil*, Reference);
	void     SSZ_STDCALL GetSharedString(PluginUtil*, Reference*);
	intptr_t SSZ_STDCALL TazyuuCheck(PluginUtil*, Reference);
	void     SSZ_STDCALL CloseTazyuuHandle(PluginUtil*, intptr_t);
}

namespace ikemen {

static PluginUtil makePU()
{
	static PluginSSZFuncs f = {nullptr, nullptr, nullptr};
	static PluginUtil pu(&f, nullptr);
	return pu;
}

// ── Dialog / system ──────────────────────────────────────────────────────

bool yesNo(const std::wstring& msg)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, msg);
	auto pu = makePU();
	bool ok = YesNo(&pu, r);
	r.releaseanddelete();
	return ok;
}

bool getClipboardStr(std::wstring& out)
{
	Reference r; r.init();
	auto pu = makePU();
	bool ok = GetClipboardStr(&pu, &r);
	out = PluginUtil::refToWstr(r);
	r.releaseanddelete();
	return ok;
}

// ── Shared clipboard string ──────────────────────────────────────────────

void setSharedString(const std::wstring& s)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, s);
	auto pu = makePU();
	SetSharedString(&pu, r);
	r.releaseanddelete();
}

void getSharedString(std::wstring& out)
{
	Reference r; r.init();
	auto pu = makePU();
	GetSharedString(&pu, &r);
	out = PluginUtil::refToWstr(r);
	r.releaseanddelete();
}

// ── INI file ─────────────────────────────────────────────────────────────

std::wstring getInifileString(const std::wstring& app, const std::wstring& key,
                              const std::wstring& file, const std::wstring& def)
{
	Reference rApp;  rApp.init();
	Reference rKey;  rKey.init();
	Reference rFile; rFile.init();
	Reference rDef;  rDef.init();
	Reference rOut;  rOut.init();
	PluginUtil::wstrToRef(rApp,  app);
	PluginUtil::wstrToRef(rKey,  key);
	PluginUtil::wstrToRef(rFile, file);
	PluginUtil::wstrToRef(rDef,  def);

	auto pu = makePU();
	GetInifileString(&pu, &rOut, rDef, rKey, rApp, rFile);
	std::wstring out = PluginUtil::refToWstr(rOut);

	rApp.releaseanddelete(); rKey.releaseanddelete(); rFile.releaseanddelete();
	rDef.releaseanddelete(); rOut.releaseanddelete();
	return out;
}

int32_t getInifileInt(const std::wstring& app, const std::wstring& key,
                      const std::wstring& file, int32_t def)
{
	Reference rKey;  rKey.init();
	Reference rApp;  rApp.init();
	Reference rFile; rFile.init();
	PluginUtil::wstrToRef(rKey,  key);
	PluginUtil::wstrToRef(rApp,  app);
	PluginUtil::wstrToRef(rFile, file);

	auto pu = makePU();
	int32_t val = GetInifileInt(&pu, def, rKey, rApp, rFile);

	rKey.releaseanddelete(); rApp.releaseanddelete(); rFile.releaseanddelete();
	return val;
}

bool writeInifileString(const std::wstring& app, const std::wstring& key,
                        const std::wstring& file, const std::wstring& val)
{
	Reference rApp;  rApp.init();
	Reference rKey;  rKey.init();
	Reference rFile; rFile.init();
	Reference rVal;  rVal.init();
	PluginUtil::wstrToRef(rApp,  app);
	PluginUtil::wstrToRef(rKey,  key);
	PluginUtil::wstrToRef(rFile, file);
	PluginUtil::wstrToRef(rVal,  val);

	auto pu = makePU();
	bool ok = WriteInifileString(&pu, rVal, rKey, rApp, rFile);

	rApp.releaseanddelete(); rKey.releaseanddelete(); rFile.releaseanddelete();
	rVal.releaseanddelete();
	return ok;
}

// ── Input dialog ─────────────────────────────────────────────────────────

void inputStr(const std::wstring& title, std::wstring& out)
{
	Reference rTitle; rTitle.init();
	Reference rOut;   rOut.init();
	PluginUtil::wstrToRef(rTitle, title);

	auto pu = makePU();
	InputStr(&pu, &rOut, rTitle);
	out = PluginUtil::refToWstr(rOut);

	rTitle.releaseanddelete(); rOut.releaseanddelete();
}

// ── Compression ──────────────────────────────────────────────────────────

bool uncompress(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst)
{
	Reference rSrc; rSrc.init();
	Reference rDst; rDst.init();
	rSrc.refnew(static_cast<intptr_t>(src.size()), 1);
	if (!rSrc.null()) memcpy(rSrc.atpos(), src.data(), src.size());

	auto pu = makePU();
	bool ok = UnCompress(&pu, &rDst, rSrc);

	if (ok && rDst.len() > 0) {
		dst.resize(static_cast<size_t>(rDst.len()));
		memcpy(dst.data(), rDst.atpos(), static_cast<size_t>(rDst.len()));
	}

	rSrc.releaseanddelete(); rDst.releaseanddelete();
	return ok;
}

// ── Code-page conversion ─────────────────────────────────────────────────

std::wstring ubytesToStr(const std::vector<uint8_t>& src, CodePage cp)
{
	Reference rSrc; rSrc.init();
	Reference rDst; rDst.init();
	rSrc.refnew(static_cast<intptr_t>(src.size()), 1);
	if (!rSrc.null()) memcpy(rSrc.atpos(), src.data(), src.size());

	auto pu = makePU();
	UbytesToStr(&pu, rSrc, &rDst, static_cast<uint32_t>(cp));
	std::wstring out = PluginUtil::refToWstr(rDst);

	rSrc.releaseanddelete(); rDst.releaseanddelete();
	return out;
}

std::vector<uint8_t> strToUbytes(const std::wstring& src, CodePage cp)
{
	Reference rSrc; rSrc.init();
	Reference rDst; rDst.init();
	PluginUtil::wstrToRef(rSrc, src);

	auto pu = makePU();
	StrToUbytes(&pu, rSrc, &rDst, static_cast<uint32_t>(cp));

	std::vector<uint8_t> dst;
	if (rDst.len() > 0) {
		dst.resize(static_cast<size_t>(rDst.len()));
		memcpy(dst.data(), rDst.atpos(), static_cast<size_t>(rDst.len()));
	}

	rSrc.releaseanddelete(); rDst.releaseanddelete();
	return dst;
}

std::wstring asciiToLocal(const std::wstring& src)
{
	Reference rSrc; rSrc.init();
	Reference rDst; rDst.init();
	PluginUtil::wstrToRef(rSrc, src);

	auto pu = makePU();
	AsciiToLocal(&pu, rSrc, &rDst);
	std::wstring out = PluginUtil::refToWstr(rDst);

	rSrc.releaseanddelete(); rDst.releaseanddelete();
	return out;
}

// ── Exclusive-access check ───────────────────────────────────────────────

namespace {
	intptr_t& thandle() { static intptr_t h = 0; return h; }
}

bool tajuuCheck(const std::wstring& name)
{
	Reference r; r.init();
	PluginUtil::wstrToRef(r, name);
	auto pu = makePU();
	if (thandle() == 0) thandle() = TazyuuCheck(&pu, r);
	r.releaseanddelete();
	return thandle() == 0;
}

} // namespace ikemen
