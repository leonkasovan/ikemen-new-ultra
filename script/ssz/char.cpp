#include "char.hpp"

namespace ikemen {

PlayerState::PlayerState() : sysvar(SYSVAR_COUNT, 0) {}

void PlayerState::reset()
{
	life = lifeMax = 1000; power = 0; powerMax = 1000;
	attack = 100; defence = 100; palNo = 1; facing = 1;
	playerNo = 0; id = 0; teamSide = 0;
	sysvar.assign(SYSVAR_COUNT, 0);
}

void PlayerState::loadPalette(const std::wstring& def, int palno)
{
	palNo = palno;
}

void CharGlobalInfo::clear()
{
	def.clear(); palno = 1; drawpalno = -1;
}

void CharList::clear()
{
	players.clear(); cgi.clear(); chars.clear();
	code.clear(); wakewakaLength.clear(); id = 0;
}

void CharList::charInit(PlayerState& p, int playerNo, int teamSide)
{
	p.reset();
	p.playerNo = playerNo;
	p.teamSide = teamSide;
}

CharList g_chars;

int    g_changeStateNest = 0;
int    g_waitdown = 0, g_shuttertime = 0;
bool   g_winskipped = false;
bool   g_bgmflag = false;
KOTy   g_ko = KOTy::NotYet;
std::vector<WinType> g_winty(2, WinType::N);
std::vector<int> g_wakewakaLength;
Stage* g_stage = nullptr;

} // namespace ikemen
