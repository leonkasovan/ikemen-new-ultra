#include "system.hpp"
#include "common.hpp"
#include "../string.hpp"

namespace ikemen {

SystemModule syst;
CommonSettings com;

void MatchState::reset()
{
	matchNo = 1; roundNo = 1; homeSide = 0;
	p1Wins = p2Wins = draws = 0;
	p1MatchWins = p2MatchWins = 0;
	consecutiveWins = 0;
	teamMode = 0; roundsToWin = 2; matchsToWin = 0;
	matchOver = false;
}

void Select::addChar(const std::wstring& def)
{
	SelectChar ch;

	auto parts = split(L",", def);
	auto chdef = trim(parts.empty() ? def : parts[0]);
	if (chdef.find(L"chars/") != 0 && chdef.find(L"chars\\") != 0)
		chdef = L"chars/" + chdef;
	ch.def = chdef;

	bool unicode;
	auto text = loadText(chdef, unicode);
	if (text.empty()) {
		ch.name = chdef;
		charlist.push_back(ch);
		return;
	}

	auto lines = splitLines(text);
	for (size_t i = 0; i < lines.size(); i++) {
		auto line = trim(lines[i]);
		if (line.empty()) continue;
		if (line[0] != L'[') continue;

		auto secname = toLower(line);
		if (secname.find(L"info") != std::wstring::npos) {
			i++;
			Section sc;
			sc.parse(lines, i);
			ch.name = sc.get(L"displayname");
			if (ch.name.empty()) ch.name = sc.get(L"name");
			i--;
		}
	}

	if (ch.name.empty()) ch.name = chdef;
	charlist.push_back(ch);
}

void Select::addStage(const std::wstring& def)
{
	SelectStage st;
	st.def = def;
	st.name = def;
	stagelist.push_back(st);
}

SelectChar Select::getChar(int n) const
{
	if (n >= 0 && n < (int)charlist.size())
		return charlist[n];
	return SelectChar{};
}

SelectStage Select::getStage(int n) const
{
	if (n >= 0 && n < (int)stagelist.size())
		return stagelist[n];
	return SelectStage{};
}

void Select::clear()
{
	charlist.clear();
	stagelist.clear();
	randomspr = nullptr;
}

} // namespace ikemen
