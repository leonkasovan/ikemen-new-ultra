#ifdef _WIN32
#include <regex>
#define RNS std
#else
#include <boost/regex.hpp>
#define RNS boost
#endif

#include <string>
#include <vector>

#include "sszdef.h"

#ifndef _WIN32
#include "pluginutil.hpp"
#endif

#include "bridge.hpp"


RNS::wregex* SSZ_STDCALL NewRegex(bool i, const std::wstring& ptn, std::wstring* error)
{
	RNS::wregex* re = nullptr;
	try{
		auto flag = RNS::wregex::ECMAScript | RNS::wregex::optimize;
		if(i) flag |= RNS::wregex::icase;
#ifdef _WIN32
		re = new RNS::wregex(ptn, flag);
#else
		re = new RNS::wregex(PluginUtil::gwToW(ptn), flag);
#endif
	}catch(const RNS::regex_error& e){
		if(error){
#ifdef _WIN32
			int len = MultiByteToWideChar(CP_THREAD_ACP, 0, e.what(), -1, nullptr, 0);
			if(len > 0){
				error->resize(len - 1);
				MultiByteToWideChar(CP_THREAD_ACP, 0, e.what(), -1, &(*error)[0], len);
			}
#else
			*error = PluginUtil::wToGw(PluginUtil::aToW(e.what()));
#endif
		}
		delete re;
		re = nullptr;
	}
	return re;
}

void SSZ_STDCALL DeleteRegex(RNS::wregex* re)
{
	delete re;
}

std::vector<ikemen::ssz_bridge::RegexMatchInfo> SSZ_STDCALL RegexSearch(const std::wstring& str, RNS::wregex* re)
{
	std::vector<ikemen::ssz_bridge::RegexMatchInfo> matches;
	if(!re) return matches;
	bool found = false;
#ifdef _WIN32
	RNS::wcmatch match;
	if(!str.empty()){
		auto first = str.c_str();
		auto last = first + str.size();
		found = RNS::regex_search(first, last, match, *re);
	}else{
		found = RNS::regex_search(L"", match, *re);
	}
#else
	RNS::match_results<std::wstring::const_iterator> match;
	if(!str.empty()){
		found = RNS::regex_search(str, match, *re);
	}else{
		found = RNS::regex_search(std::wstring(), match, *re);
	}
#endif
	if(!found) return matches;
	matches.reserve(match.size());
	for(decltype(match.size()) i = 0; i < match.size(); i++){
		ikemen::ssz_bridge::RegexMatchInfo m;
		m.pos = match.position(i);
		m.len = match.length(i);
#ifndef _WIN32
		if(m.pos != (decltype(match)::difference_type)-1){
			for(intptr_t j = 0; j < m.pos; j++){
				if(str[(size_t)j] >= 0x10000) ++m.pos;
			}
		}
		if(m.len > 0){
			auto end = match.position(i) + m.len;
			for(auto j = match.position(i); j < end; ++j){
				if(str[(size_t)j] >= 0x10000) ++m.len;
			}
		}
#endif
		matches.push_back(m);
	}
	return matches;
}
