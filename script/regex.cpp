#include "regex.hpp"

#include "../main/ssz/sszdef.h"
#include "../main/ssz/typeid.h"
#include "../main/ssz/arrayandref.hpp"
#include "../main/ssz/pluginutil.hpp"

extern "C" {
	void     SSZ_STDCALL DeleteRegex(PluginUtil*, void*);
	intptr_t SSZ_STDCALL NewRegex(PluginUtil*, Reference*, bool, Reference);
	void     SSZ_STDCALL RegexSearch(PluginUtil*, Reference*, Reference, void*);
}

namespace ikemen {

static PluginUtil makePU()
{
	static PluginSSZFuncs f = {nullptr, nullptr, nullptr};
	static PluginUtil pu(&f, nullptr);
	return pu;
}

Regex::~Regex() { clear(); }

void Regex::clear()
{
	if (m_re) {
		auto pu = makePU();
		DeleteRegex(&pu, reinterpret_cast<void*>(m_re));
		m_re = 0;
	}
}

std::wstring Regex::init(const std::wstring& pattern, int flags)
{
	clear();
	Reference rPat;  rPat.init();
	Reference rErr;  rErr.init();
	PluginUtil::wstrToRef(rPat, pattern);

	auto pu = makePU();
	m_re = NewRegex(&pu, &rErr, (flags & 1) != 0, rPat);

	std::wstring err = PluginUtil::refToWstr(rErr);
	rPat.releaseanddelete();
	rErr.releaseanddelete();
	return err;
}

std::vector<std::wstring> Regex::search(const std::wstring& str)
{
	std::vector<std::wstring> out;
	Reference rStr;   rStr.init();
	Reference rMatch; rMatch.init();
	PluginUtil::wstrToRef(rStr, str);

	auto pu = makePU();
	RegexSearch(&pu, &rMatch, rStr, reinterpret_cast<void*>(m_re));

	for (intptr_t i = 0; i < rMatch.len() / static_cast<intptr_t>(sizeof(Reference)); i++) {
		Reference* refs = reinterpret_cast<Reference*>(rMatch.atpos());
		out.push_back(PluginUtil::refToWstr(refs[i]));
	}
	rStr.releaseanddelete();
	rMatch.releaseanddelete();
	return out;
}

} // namespace ikemen
